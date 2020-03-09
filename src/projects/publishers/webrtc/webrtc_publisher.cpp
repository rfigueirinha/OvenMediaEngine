#include <utility>

#include "rtc_private.h"
#include "rtc_session.h"
#include "rtc_stream.h"
#include "webrtc_publisher.h"

#include "config/config_manager.h"

#include <orchestrator/orchestrator.h>

std::shared_ptr<WebRtcPublisher> WebRtcPublisher::Create(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto webrtc = std::make_shared<WebRtcPublisher>(server_config, host_info, router);

	if (!webrtc->Start())
	{
		logte("An error occurred while creating WebRtcPublisher");
		return nullptr;
	}
	return webrtc;
}

WebRtcPublisher::WebRtcPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, host_info, router)
{
}

WebRtcPublisher::~WebRtcPublisher()
{
	logtd("WebRtcPublisher has been terminated finally");
}

/*
 * Publisher Implementation
 */

bool WebRtcPublisher::Start()
{
	auto server_config = GetServerConfig();
	auto webrtc_port_info = server_config.GetBind().GetPublishers().GetWebrtc();

	_ice_port = IcePortManager::Instance()->CreatePort(webrtc_port_info.GetIceCandidates(), IcePortObserver::GetSharedPtr());
	if (_ice_port == nullptr)
	{
		logte("Cannot initialize ICE Port. Check your ICE configuration");
		return false;
	}

	// Signalling에 Observer 연결
	ov::SocketAddress signalling_address = ov::SocketAddress(server_config.GetIp(), static_cast<uint16_t>(webrtc_port_info.GetSignallingPort()));

	logti("WebRTC Publisher is listening on %s...", signalling_address.ToString().CStr());

	_signalling = std::make_shared<RtcSignallingServer>(server_config, GetHostInfo());
	_signalling->AddObserver(RtcSignallingObserver::GetSharedPtr());
	if (!_signalling->Start(signalling_address))
	{
		return false;
	}

	// Publisher::Start()에서 Application을 생성한다.
	return Publisher::Start();
}

bool WebRtcPublisher::Stop()
{
	_ice_port.reset();
	_signalling.reset();

	return Publisher::Stop();
}

void WebRtcPublisher::StatLog(const std::shared_ptr<WebSocketClient> &ws_client,
							  const std::shared_ptr<RtcStream> &stream,
							  const std::shared_ptr<RtcSession> &session,
							  const RequestStreamResult &result)
{
	// logging for statistics
	// server domain yyyy-mm-dd tt:MM:ss url sent_bytes request_time upstream_cache_status http_status client_ip http_user_agent http_referer origin_addr origin_http_status geoip geoip_org http_encoding content_length upstream_connect_time upstream_header_time upstream_response_time
	// OvenMediaEngine 127.0.0.1       02-2020-08      01:18:21        /app/stream_o   -       -       -       200     127.0.0.1:57233 Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.130 Safari/537.36                -       -       -       -       -       -       -       -
	ov::String log;

	// Server Name
	log.AppendFormat("%s", _server_config.GetName().CStr());
	// Domain
	log.AppendFormat("\t%s", ws_client->GetClient()->GetRequest()->GetHeader("HOST").Split(":")[0].CStr());
	// Current Time
	std::time_t t = std::time(nullptr);
	char mbstr[100];
	memset(mbstr, 0, sizeof(mbstr));
	// yyyy-mm-dd
	std::strftime(mbstr, sizeof(mbstr), "%Y-%m-%d", std::localtime(&t));
	log.AppendFormat("\t%s", mbstr);
	// tt:mm:ss
	memset(mbstr, 0, sizeof(mbstr));
	std::strftime(mbstr, sizeof(mbstr), "%H:%M:%S", std::localtime(&t));
	log.AppendFormat("\t%s", mbstr);
	// Url
	log.AppendFormat("\t%s", ws_client->GetClient()->GetRequest()->GetRequestTarget().CStr());
	
	if(result == RequestStreamResult::transfer_completed && session != nullptr)
	{
		// Bytes sents
		log.AppendFormat("\t%llu", session->GetSentBytes());
		// request_time
		auto now = std::chrono::system_clock::now();
		std::chrono::duration<double, std::ratio<1>> elapsed = now - session->GetCreatedTime();
		log.AppendFormat("\t%f", elapsed.count());
	}
	else
	{
		// Bytes sents : -
		// request_time : -
		log.AppendFormat("\t-\t-");
	}
	
	// upstream_cache_status : -
	if(result == RequestStreamResult::local_success)
	{
		log.AppendFormat("\t%s", "hit");
	}
	else
	{
		log.AppendFormat("\t%s", "miss");
	}
	// http_status : 200 or 404
	log.AppendFormat("\t%s", stream != nullptr ? "200" : "404");
	// client_ip
	log.AppendFormat("\t%s", ws_client->GetClient()->GetRemote()->GetRemoteAddress()->ToString().CStr());
	// http user agent
	log.AppendFormat("\t%s", ws_client->GetClient()->GetRequest()->GetHeader("User-Agent").CStr());
	// http referrer
	log.AppendFormat("\t%s", ws_client->GetClient()->GetRequest()->GetHeader("Referer").CStr());
	// origin addr
	// orchestrator->GetUrlListForLocation()
	
	if (result == RequestStreamResult::origin_success && stream != nullptr)
	{
		// origin http status : 200 or 404
		// geoip : -
		// geoip_org : -
		// http_encoding : -
		// content_length : -
		log.AppendFormat("\t%s\t-\t-\t-\t-", "200");
		// upstream_connect_time
		// upstream_header_time : -
		// upstream_response_time
		auto stream_metric = mon::Monitoring::GetInstance()->GetStreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
		if(stream_metric != nullptr)
		{
			log.AppendFormat("\t%f\t-\t%f", stream_metric->GetOriginRequestTimeMSec(), stream_metric->GetOriginResponseTimeMSec());
		}
		else
		{
			log.AppendFormat("\t-\t-\t-");
		}
	}
	else if (result == RequestStreamResult::origin_failed)
	{
		log.AppendFormat("\t%s\t-\t-\t-\t-", "404");
		log.AppendFormat("\t-\t-\t-");
	}
	else
	{
		log.AppendFormat("\t-\t-\t-\t-\t-");
		log.AppendFormat("\t-\t-\t-");
	}

	stat_log(STAT_LOG_WEBRTC_EDGE, "%s", log.CStr());
}

//====================================================================================================
// monitoring data pure virtual function
// - collections vector must be insert processed
//====================================================================================================
bool WebRtcPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return _signalling->GetMonitoringCollectionData(collections);
}

// Publisher에서 Application 생성 요청이 온다.
std::shared_ptr<pub::Application> WebRtcPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	return RtcApplication::Create(application_info, _ice_port, _signalling);
}

/*
 * Signalling Implementation
 */

// Called when receives request offer sdp from client
std::shared_ptr<SessionDescription> WebRtcPublisher::OnRequestOffer(const std::shared_ptr<WebSocketClient> &ws_client, const ov::String &application_name, const ov::String &stream_name, std::vector<RtcIceCandidate> *ice_candidates)
{
	// Application -> Stream에서 SDP를 얻어서 반환한다.
	auto orchestrator = Orchestrator::GetInstance();
	RequestStreamResult result = RequestStreamResult::init;

	auto stream = std::static_pointer_cast<RtcStream>(GetStream(application_name, stream_name));
	if (stream == nullptr)
	{
		// If the stream does not exists, request to the provider
		if (orchestrator->RequestPullStream(application_name, stream_name) == false)
		{
			logtd("Could not pull the stream: [%s/%s]", application_name.CStr(), stream_name.CStr());
			result = RequestStreamResult::origin_failed;
		}
		else
		{
			stream = std::static_pointer_cast<RtcStream>(GetStream(application_name, stream_name));
			if (stream == nullptr)
			{
				logtd("Could not pull the stream: [%s/%s]", application_name.CStr(), stream_name.CStr());
				result = RequestStreamResult::local_failed;
			}
			else
			{
				result = RequestStreamResult::origin_success;
			}
		}
	}
	else
	{
		result = RequestStreamResult::local_success;
	}

	StatLog(ws_client, stream, nullptr, result);

	if (stream == nullptr)
	{
		return nullptr;
	}

	auto &candidates = _ice_port->GetIceCandidateList();
	ice_candidates->insert(ice_candidates->end(), candidates.cbegin(), candidates.cend());
	auto session_description = std::make_shared<SessionDescription>(*stream->GetSessionDescription());
	// Generate Unique Session Id
	session_description->SetOrigin("OvenMediaEngine", stream->IssueUniqueSessionId(), 2, "IN", 4, "127.0.0.1");
	session_description->SetIceUfrag(_ice_port->GenerateUfrag());
	session_description->Update();

	return session_description;
}

// Called when receives an answer sdp from client
bool WebRtcPublisher::OnAddRemoteDescription(const std::shared_ptr<WebSocketClient> &ws_client,
											 const ov::String &application_name,
											 const ov::String &stream_name,
											 const std::shared_ptr<SessionDescription> &offer_sdp,
											 const std::shared_ptr<SessionDescription> &peer_sdp)
{
	auto application = GetApplicationByName(application_name);
	auto stream = GetStream(application_name, stream_name);

	if (!stream)
	{
		logte("Cannot find stream (%s/%s)", application_name.CStr(), stream_name.CStr());
		return false;
	}

	ov::String remote_sdp_text;
	peer_sdp->ToString(remote_sdp_text);
	logtd("OnAddRemoteDescription: %s", remote_sdp_text.CStr());

	// Stream에 Session을 생성한다.
	auto session = RtcSession::Create(application, stream, offer_sdp, peer_sdp, _ice_port, ws_client);
	if (session != nullptr)
	{
		// Stream에 Session을 등록한다.
		stream->AddSession(session);
		auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
		if (stream_metrics != nullptr)
		{
			stream_metrics->OnSessionConnected(PublisherType::Webrtc);
		}

		// ice_port에 SessionInfo을 전달한다.
		// 향후 해당 session에서 Ice를 통해 패킷이 들어오면 SessionInfo와 함께 Callback을 준다.
		_ice_port->AddSession(session, offer_sdp, peer_sdp);
	}
	else
	{
		// peer_sdp가 잘못되거나, 다른 이유로 인해 session을 생성하지 못함
		logte("Cannot create session");
	}

	return true;
}

bool WebRtcPublisher::OnStopCommand(const std::shared_ptr<WebSocketClient> &ws_client,
									const ov::String &application_name, const ov::String &stream_name,
									const std::shared_ptr<SessionDescription> &offer_sdp,
									const std::shared_ptr<SessionDescription> &peer_sdp)
{
	// 플레이어에서 stop 이벤트가 수신 된 경우 처리
	logtd("Stop commnad received : %s/%s/%u", application_name.CStr(), stream_name.CStr(), offer_sdp->GetSessionId());
	// Find Stream
	auto stream = std::static_pointer_cast<RtcStream>(GetStream(application_name, stream_name));
	if (!stream)
	{
		logte("To stop session failed. Cannot find stream (%s/%s)", application_name.CStr(), stream_name.CStr());
		return false;
	}

	// Offer SDP의 Session ID로 세션을 찾는다.
	auto session = std::static_pointer_cast<RtcSession>(stream->GetSession(offer_sdp->GetSessionId()));
	if (session == nullptr)
	{
		logte("To stop session failed. Cannot find session by peer sdp session id (%u)", offer_sdp->GetSessionId());
		return false;
	}

	StatLog(session->GetWSClient(), stream, session, RequestStreamResult::transfer_completed);

	// Session을 Stream에서 정리한다.
	stream->RemoveSession(session->GetId());
	auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
	if (stream_metrics != nullptr)
	{
		stream_metrics->OnSessionDisconnected(PublisherType::Webrtc);
	}
	// IcePort에서 Remove 한다.
	_ice_port->RemoveSession(session);

	return true;
}

// bitrate info(frome signalling)
uint32_t WebRtcPublisher::OnGetBitrate(const std::shared_ptr<WebSocketClient> &ws_client, const ov::String &application_name, const ov::String &stream_name)
{
	auto stream = GetStream(application_name, stream_name);
	uint32_t bitrate = 0;

	if (!stream)
	{
		logte("Cannot find stream (%s/%s)", application_name.CStr(), stream_name.CStr());
		return 0;
	}

	auto tracks = stream->GetTracks();
	for (auto &track_iter : tracks)
	{
		MediaTrack *track = track_iter.second.get();

		if (track->GetCodecId() == common::MediaCodecId::Vp8 || track->GetCodecId() == common::MediaCodecId::Opus)
		{
			bitrate += track->GetBitrate();
		}
	}

	return bitrate;
}

bool WebRtcPublisher::OnIceCandidate(const std::shared_ptr<WebSocketClient> &ws_client,
									 const ov::String &application_name,
									 const ov::String &stream_name,
									 const std::shared_ptr<RtcIceCandidate> &candidate,
									 const ov::String &username_fragment)
{
	return true;
}

/*
 * IcePort Implementation
 */

void WebRtcPublisher::OnStateChanged(IcePort &port, const std::shared_ptr<info::Session> &session_info,
									 IcePortConnectionState state)
{
	logtd("IcePort OnStateChanged : %d", state);

	auto session = std::static_pointer_cast<RtcSession>(session_info);
	auto application = session->GetApplication();
	auto stream = std::static_pointer_cast<RtcStream>(session->GetStream());
	
	// state를 보고 session을 처리한다.
	switch (state)
	{
		case IcePortConnectionState::New:
		case IcePortConnectionState::Checking:
		case IcePortConnectionState::Connected:
		case IcePortConnectionState::Completed:
			// 연결되었을때는 할일이 없다.
			break;
		case IcePortConnectionState::Failed:
		case IcePortConnectionState::Disconnected:
		case IcePortConnectionState::Closed:
		{
			StatLog(session->GetWSClient(), stream, session, RequestStreamResult::transfer_completed);
			// Session을 Stream에서 정리한다.
			stream->RemoveSession(session->GetId());
			auto stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(stream));
			if (stream_metrics != nullptr)
			{
				stream_metrics->OnSessionDisconnected(PublisherType::Webrtc);
			}

			// Signalling에 종료 명령을 내린다.
			_signalling->Disconnect(application->GetName(), stream->GetName(), session->GetPeerSDP());
			break;
		}
		default:
			break;
	}
}

void WebRtcPublisher::OnDataReceived(IcePort &port, const std::shared_ptr<info::Session> &session_info, std::shared_ptr<const ov::Data> data)
{
	// ice_port를 통해 STUN을 제외한 모든 Packet이 들어온다.
	auto session = std::static_pointer_cast<pub::Session>(session_info);

	//받는 Data 형식을 협의해야 한다.
	auto application = session->GetApplication();
	application->PushIncomingPacket(session_info, data);
}
