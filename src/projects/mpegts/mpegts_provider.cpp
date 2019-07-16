//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include <iostream>
#include <unistd.h>
#include <config/config.h>

#include "mpegts_provider.h"
#include "mpegts_application.h"
#include "mpegts_stream.h"

#define OV_LOG_TAG "MpegTsProvider"

using namespace common;

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<MpegTsProvider>
MpegTsProvider::Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
{
	auto provider = std::make_shared<MpegTsProvider>(application_info, router);
	if (!provider->Start())
	{
		logte("An error occurred while creating MpegTsProvider");
		return nullptr;
	}
	return provider;
}

//====================================================================================================
// MpegTsProvider
//====================================================================================================
MpegTsProvider::MpegTsProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
		: Provider(application_info, router)
{
	logtd("Created MpegTs Provider modules.");
}

//====================================================================================================
// ~MpegTsProvider
//====================================================================================================
MpegTsProvider::~MpegTsProvider()
{
//	Stop();
	logtd("Terminated MpegTs Provider modules.");
}

//====================================================================================================
// Start
//====================================================================================================
bool MpegTsProvider::Start()
{
	// Find MpegTs provider configuration
	_provider_info = _application_info->GetProvider<cfg::MpegTsProvider>();

	if(_provider_info == nullptr)
	{
		logte("Cannot initialize MpegTsProvider using config information");
		return false;
	}

	auto host = _application_info->GetParentAs<cfg::Host>("Host");

	if(host == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	int port = host->GetPorts().GetMpegTsProviderPort().GetPort();

	// auto mpegts_provider = host->
	auto mpegts_address = ov::SocketAddress(host->GetIp(), static_cast<uint16_t>(port));

	logti("MpegTs Provider is listening on %s...", mpegts_address.ToString().CStr());

	// MpegTsServer 생성
	_mpegts_server = std::make_shared<MpegTsServer>();

	// MpegTsServer 에 Observer 연결
	_mpegts_server->AddObserver(MpegTsObserver::GetSharedPtr());
	if (!_mpegts_server->Start(mpegts_address))
	{
		return false;
	}

	return Provider::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool MpegTsProvider::Stop()
{
	return Provider::Stop();
}

std::shared_ptr<Application> MpegTsProvider::OnCreateApplication(const info::Application *application_info)
{
	return MpegTsApplication::Create(application_info);
}

//====================================================================================================
// OnStreamReadyComplete
// - MpegTsObserver 구현
//====================================================================================================
bool MpegTsProvider::OnStreamReadyComplete(const ov::String &app_name,
										   const ov::String &stream_name,
										   std::shared_ptr<MpegTsMediaInfo> &media_info,
										   info::application_id_t &application_id,
										   uint32_t &stream_id)
{
	// 어플리케이션 조회, 어플리케이션명에 해당하는 정보가 없다면 커넥션을 종료함.
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationByName(app_name.CStr()));
	if(application == nullptr)
	{
		logte("Cannot Find Applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	if(GetStreamByName(app_name, stream_name))
	{
		if(_provider_info->IsBlockDuplicateStreamName())
		{
			logti("Duplicate Stream Input(reject) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			return false;
		}
		else
		{
			uint32_t stream_id = GetStreamByName(app_name, stream_name)->GetId();

			logti("Duplicate Stream Input(change) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());

			// 스트림 정보 종료
			application->DeleteStream2(application->GetStreamByName(stream_name));
		}
	}

	// Application -> MpegTsApplication: create mpegts stream -> Application 에 Stream 정보 저장
	auto stream = application->MakeStream();
	if(stream == nullptr)
	{
		logte("can not create stream - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	stream->SetName(stream_name.CStr());

	if(media_info->video_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		new_track->SetId(0);
		new_track->SetMediaType(MediaType::Video);
		new_track->SetCodecId(MediaCodecId::H264);
		new_track->SetWidth((uint32_t)media_info->video_width);
		new_track->SetHeight((uint32_t)media_info->video_height);
		new_track->SetFrameRate(media_info->video_framerate);

		new_track->SetTimeBase(1, 90000);
		stream->AddTrack(new_track);
	}

	if(media_info->audio_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		// 오디오는 TrackID를 1로 고정
		new_track->SetId(1);
		new_track->SetMediaType(MediaType::Audio);
		new_track->SetCodecId(MediaCodecId::Aac);
		new_track->SetSampleRate(media_info->audio_samplerate);
		new_track->SetTimeBase(1, 90000);
		new_track->GetSample().SetFormat(common::AudioSample::Format::S16);

		if(media_info->audio_channels == 1)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutMono);
		}
		else if(media_info->audio_channels == 2)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutStereo);
		}

		stream->AddTrack(new_track);
	}

	// 라우터에 스트림이 생성되었다고 알림
	application->CreateStream2(stream);

	// id 설정
	application_id = application->GetId();
	stream_id = stream->GetId();

	logtd("Strem ready complete - app(%s/%u) stream(%s/%u)", app_name.CStr(), application_id, stream_name.CStr(), stream_id);

	return true;
}

//====================================================================================================
// OnVideoStreamData
// - MpegTsObserver 구현
//====================================================================================================
bool MpegTsProvider::OnVideoData(info::application_id_t application_id,
								 uint32_t stream_id,
								 uint32_t timestamp,
								 MpegTsFrameType frame_type,
								 std::shared_ptr<std::vector<uint8_t>> &data)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(application_id));

	if(application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_id));
	if(stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto pbuf = std::make_unique<MediaPacket>(MediaType::Video,
											  0,
											  data->data(),
											  data->size(),
											  timestamp,
											  frame_type == MpegTsFrameType::VideoIFrame ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

	application->SendFrame(stream, std::move(pbuf));

	return true;
}

//====================================================================================================
// OnAudioStreamData
// - MpegTsObserver 구현
//====================================================================================================
bool MpegTsProvider::OnAudioData(info::application_id_t application_id,
								 uint32_t stream_id,
								 uint32_t timestamp,
								 MpegTsFrameType frame_type,
								 std::shared_ptr<std::vector<uint8_t>> &data)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(application_id));

	if(application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_id));

	if(stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto pbuf = std::make_unique<MediaPacket>(MediaType::Audio,
											  1,
											  data->data(),
											  data->size(),
											  timestamp,
											  MediaPacketFlag::Key);
	application->SendFrame(stream, std::move(pbuf));

	return true;
}

//====================================================================================================
// OnDeleteStream
// - MpegTsObserver 구현
//====================================================================================================
bool MpegTsProvider::OnDeleteStream(info::application_id_t app_id, uint32_t stream_id)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(app_id));
	if(application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_id));
	if(stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	// 라우터에 스트림이 삭제되었다고 알림
	return application->DeleteStream2(stream);
}
