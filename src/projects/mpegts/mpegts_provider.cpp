//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "mpegts_provider.h"
#include "mpegts_application.h"
#include "mpegts_private.h"
#include "mpegts_stream.h"

#include <config/config.h>
#include <unistd.h>
#include <iostream>

std::shared_ptr<MpegTsProvider> MpegTsProvider::Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
{
	auto provider = std::make_shared<MpegTsProvider>(application_info, router);

	if (provider->Start() == false)
	{
		logte("An error occurred while creating MpegTsProvider");
		return nullptr;
	}

	return provider;
}

MpegTsProvider::MpegTsProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: Provider(application_info, router)
{
	logtd("MPEG-TS Provider is created");
}

MpegTsProvider::~MpegTsProvider()
{
	Stop();

	logtd("MPEG-TS Provider is destroyed");
}

bool MpegTsProvider::ParseAddressList(const ov::String &ip, const cfg::RangedPort &ranged_port, std::map<ov::port_t, ov::SocketAddress> *address_list, ov::port_t *min_port)
{
	if (ranged_port.IsParsed() == false)
	{
		return false;
	}

	if (ranged_port.GetSocketType() != ov::SocketType::Udp)
	{
		logte("MPEG-TS provider does not support socket type: %d", static_cast<int>(ranged_port.GetSocketType()));
		return false;
	}

	auto ports = ranged_port.GetRangedPort().Split(",");
	ov::port_t minimum_port = std::numeric_limits<ov::port_t>::max();

	for (auto port : ports)
	{
		port = port.Trim();

		auto range = port.Split("-");
		ov::port_t start = 0;
		ov::port_t end = 0;

		switch (range.size())
		{
			case 1:
				// single port number
				start = ov::Converter::ToUInt16(port);
				end = start;
				break;

			case 2:
				// port range
				start = ov::Converter::ToUInt16(range[0]);
				end = ov::Converter::ToUInt16(range[1]);
				break;

			default:
				logte("Invalid port range: %s (%zu tokens found)", port.CStr(), range.size());
				return false;
		}

		if (end < start)
		{
			logte("Invalid port range: %s (end port MUST be greater than start port)", port.CStr());
			return false;
		}

		minimum_port = std::min(minimum_port, start);

		for (ov::port_t port_num = start; port_num <= end; port_num++)
		{
			if (address_list->find(port_num) != address_list->end())
			{
				logte("The port number is in conflict: %d", port_num);
				return false;
			}

			address_list->emplace(port_num, std::move(ov::SocketAddress(ip, port_num)));
		}
	}

	if (address_list->size() == 0)
	{
		logte("No ports are available");
		return false;
	}

	*min_port = minimum_port;

	return true;
}

bool MpegTsProvider::Start()
{
	// Find MPEG-TS provider configuration
	_provider_info = _application_info->GetProvider<cfg::MpegTsProvider>();

	if (_provider_info == nullptr)
	{
		logte("Cannot initialize MpegTsProvider using config information");
		return false;
	}

	auto host = _application_info->GetParentAs<cfg::Host>("Host");

	if (host == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	auto mpegts_port = dynamic_cast<const cfg::RangedPort &>(host->GetPorts().GetMpegTsProviderPort());
	std::map<ov::port_t, ov::SocketAddress> address_list;
	ov::port_t min_port = 0;

	if (ParseAddressList(host->GetIp(), mpegts_port, &address_list, &min_port) == false)
	{
		return false;
	}

	{
		std::lock_guard<__decltype(_chunk_stream_list_mutex)> lock_guard(_chunk_stream_list_mutex);
		// Create a stream mapping table
		auto streams = _provider_info->GetStreamMap().GetStreamList();

		_stream_table.clear();

		// To check stream name duplication
		std::map<ov::String, ov::port_t> stream_name_map;

		for (auto &stream : streams)
		{
			auto name = stream.GetName();
			auto port = stream.GetPort();

			if (name.IsEmpty() || port.IsEmpty())
			{
				logte("Invalid MPEG-TS stream configuration: app: %s, name: %s, port: %s", _application_info->GetName().CStr(), name.CStr(), port.CStr());
				return false;
			}

			auto full_name = ov::String::FormatString("%s/%s", _application_info->GetName().CStr(), name.CStr());

			// "+" prefix indicates a relative value from the first port number in <Ports>.<MPEGTSProvider>
			ov::port_t port_num;
			bool is_relative = port.HasPrefix("+");

			if (is_relative)
			{
				port_num = ov::Converter::ToUInt16(port.Substring(1)) + min_port;
			}
			else
			{
				port_num = ov::Converter::ToUInt16(port);
			}

			if (address_list.find(port_num) == address_list.end())
			{
				if (is_relative)
				{
					logte("The MPEG-TS port %d (%d%s) does not exists in the list of MPEG-TS provider ports", port_num, min_port, port.CStr());
				}
				else
				{
					logte("The MPEG-TS port %d does not exists in the list of MPEG-TS provider ports", port_num);
				}

				return false;
			}

			auto item = _stream_table.find(port_num);

			if (item != _stream_table.end())
			{
				if (is_relative)
				{
					logte("The MPEG-TS port is conflicted: %d (%d%s) for [%s]", port_num, min_port, port.CStr(), full_name.CStr());
				}
				else
				{
					logte("The MPEG-TS port is conflicted: [%s] for [%s]", port.CStr(), full_name.CStr());
				}

				return false;
			}

			if (stream_name_map.find(name) != stream_name_map.end())
			{
				logte("The MPEG-TS stream name is conflicted: %s", name.CStr());
				return false;
			}

			auto port_info = std::make_shared<MpegTsStreamInfo>(
				MpegTsStreamInfo::PrivateToken(),
				// Unknown address
				ov::SocketAddress(),
				// Application/Stream name
				_application_info->GetName(), name,
				// Application ID
				_application_info->GetId(),
				// Stream ID is not known at this point
				0);

			stream_name_map.emplace(name, port_num);

			_stream_table.emplace(port_num, std::move(port_info));
		}
	}

	_mpegts_server = std::make_shared<MpegTsServer>();
	_mpegts_server->AddObserver(MpegTsObserver::GetSharedPtr());

	logti("MPEG-TS Provider is starting...");

	if (_mpegts_server->Start(address_list) == false)
	{
		return false;
	}

	return Provider::Start();
}

std::shared_ptr<Application> MpegTsProvider::OnCreateApplication(const info::Application *application_info)
{
	return MpegTsApplication::Create(application_info);
}

std::shared_ptr<const MpegTsStreamInfo> MpegTsProvider::OnQueryStreamInfo(uint16_t port_num, const ov::SocketAddress *address)
{
	std::lock_guard<__decltype(_chunk_stream_list_mutex)> lock_guard(_chunk_stream_list_mutex);
	auto stream_info = _stream_table.find(port_num);

	if (stream_info == _stream_table.end())
	{
		return nullptr;
	}

	if (address != nullptr)
	{
		stream_info->second->address = *address;
	}

	return stream_info->second;
}

bool MpegTsProvider::OnStreamReady(const std::shared_ptr<const MpegTsStreamInfo> &stream_info,
								   const std::shared_ptr<MpegTsMediaInfo> &media_info)
{
	OV_ASSERT2(stream_info != nullptr);
	OV_ASSERT2(media_info != nullptr);

	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationByName(stream_info->app_name.CStr()));

	if (application == nullptr)
	{
		logte("Could not find an application for %s", stream_info->ToString().CStr());
		_mpegts_server->Disconnect(stream_info);
		return false;
	}

	// stream_id does not available at this point
	auto stream = application->GetStreamByName(stream_info->stream_name);

	if (stream != nullptr)
	{
		logti("The input stream is duplicated. Rejecting... %s", stream_info->ToString().CStr());
		_mpegts_server->Disconnect(stream_info);
		return false;
	}

	// Create a new stream
	auto new_stream = application->MakeStream();

	if (new_stream == nullptr)
	{
		logte("Could not create stream for %s", stream_info->ToString().CStr());
		_mpegts_server->Disconnect(stream_info);
		return false;
	}

	{
		// Make stream_info writable temporary

		// This is safe operation - MpegTsStreamInfo instance is always created by MpegTsProvider only
		auto mutable_stream_info = std::const_pointer_cast<MpegTsStreamInfo>(stream_info);

		OV_ASSERT2(mutable_stream_info->stream_id == 0);

		mutable_stream_info->stream_id = new_stream->GetId();
	}

	new_stream->SetName(stream_info->stream_name.CStr());

	if (media_info->is_video_available)
	{
		auto new_track = std::make_shared<MediaTrack>();

		new_track->SetId(0);
		new_track->SetMediaType(common::MediaType::Video);
		new_track->SetCodecId(common::MediaCodecId::H264);
		new_track->SetWidth((uint32_t)media_info->video_width);
		new_track->SetHeight((uint32_t)media_info->video_height);
		new_track->SetFrameRate(media_info->video_framerate);

		new_track->SetTimeBase(1, 90000);
		new_stream->AddTrack(new_track);
	}

	if (media_info->is_audio_available)
	{
		auto new_track = std::make_shared<MediaTrack>();

		new_track->SetId(1);
		new_track->SetMediaType(common::MediaType::Audio);
		new_track->SetCodecId(common::MediaCodecId::Aac);
		new_track->SetSampleRate(media_info->audio_samplerate);
		new_track->SetTimeBase(1, 90000);
		new_track->GetSample().SetFormat(common::AudioSample::Format::S16);

		if (media_info->audio_channels == 1)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutMono);
		}
		else if (media_info->audio_channels == 2)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutStereo);
		}

		new_stream->AddTrack(new_track);
	}

	auto result = application->CreateStream2(new_stream);

	OV_ASSERT2(result);

	logtd("The MPEG-TS stream is ready for %s", stream_info->ToString().CStr());

	return true;
}

bool MpegTsProvider::OnVideoData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info,
								 int64_t pts, int64_t dts,
								 MpegTsFrameType frame_type,
								 const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(stream_info->app_id));

	if (application == nullptr)
	{
		logte("Could not find an application for %s", stream_info->ToString().CStr());
		OV_ASSERT2(false);
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_info->stream_id));
	if (stream == nullptr)
	{
		logte("Could not find a stream for %s", stream_info->ToString().CStr());
		OV_ASSERT2(false);
		return false;
	}

	auto packet = std::make_unique<MediaPacket>(common::MediaType::Video,
												0,
												data,
												// PTS
												pts,
												// DTS
												dts,
												// Duration
												-1LL,
												frame_type == MpegTsFrameType::VideoIFrame ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

	application->SendFrame(stream, std::move(packet));

	return true;
}

bool MpegTsProvider::OnAudioData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info,
								 int64_t pts, int64_t dts,
								 MpegTsFrameType frame_type,
								 const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(stream_info->app_id));

	if (application == nullptr)
	{
		logte("Could not find an application for %s", stream_info->ToString().CStr());
		OV_ASSERT2(false);
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_info->stream_id));

	if (stream == nullptr)
	{
		logte("Could not find a stream for %s", stream_info->ToString().CStr());
		OV_ASSERT2(false);
		return false;
	}

	auto packet = std::make_unique<MediaPacket>(common::MediaType::Audio,
												1,
												data,
												// PTS
												pts,
												// DTS
												dts,
												// Duration
												-1LL,
												MediaPacketFlag::Key);

	application->SendFrame(stream, std::move(packet));

	return true;
}

bool MpegTsProvider::OnDeleteStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(stream_info->app_id));

	if (application == nullptr)
	{
		logte("Could not find an application for %s", stream_info->ToString().CStr());
		// OV_ASSERT2(false);

		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_info->stream_id));
	if (stream == nullptr)
	{
		logte("Could not find a stream for %s", stream_info->ToString().CStr());
		OV_ASSERT2(false);

		return false;
	}

	return application->DeleteStream2(stream);
}
