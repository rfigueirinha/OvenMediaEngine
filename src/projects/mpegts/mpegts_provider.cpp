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

	int port = host->GetPorts().GetMpegTsProviderPort().GetPort();

	auto mpegts_address = ov::SocketAddress(host->GetIp(), static_cast<uint16_t>(port));
	_mpegts_server = std::make_shared<MpegTsServer>();

	logti("MPEG-TS Provider is listening on %s...", mpegts_address.ToString().CStr());

	_mpegts_server->AddObserver(MpegTsObserver::GetSharedPtr());

	if (_mpegts_server->Start(mpegts_address) == false)
	{
		return false;
	}

	return Provider::Start();
}

std::shared_ptr<Application> MpegTsProvider::OnCreateApplication(const info::Application *application_info)
{
	return MpegTsApplication::Create(application_info);
}

bool MpegTsProvider::OnStreamReadyComplete(const ov::String &app_name, const ov::String &stream_name,
										   std::shared_ptr<MpegTsMediaInfo> &media_info,
										   info::application_id_t &application_id,
										   uint32_t &stream_id)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationByName(app_name.CStr()));
	if (application == nullptr)
	{
		logte("Could not find an application for [%s/%s]", app_name.CStr(), stream_name.CStr());
		return false;
	}

	if (GetStreamByName(app_name, stream_name))
	{
		if (_provider_info->IsBlockDuplicateStreamName())
		{
			logti("The input stream is duplicated. Rejecting... [%s/%s]", app_name.CStr(), stream_name.CStr());
			return false;
		}
		else
		{
			uint32_t stream_id = GetStreamByName(app_name, stream_name)->GetId();

			logti("The input stream is duplicated. Disconnecting previous connection... [%s/%s]", app_name.CStr(), stream_name.CStr());

			// Disconnect the previous connection
			if (
				(_mpegts_server->Disconnect(app_name, stream_id) == false) ||
				(application->DeleteStream2(application->GetStreamByName(stream_name)) == false))
			{
				logte("Failed to disconnect client from MPEG-TS Server for [%s/%s]", app_name.CStr(), stream_name.CStr());
			}
		}
	}

	// Create a new stream
	auto stream = application->MakeStream();

	if (stream == nullptr)
	{
		logte("Could not create stream for [%s/%s]", app_name.CStr(), stream_name.CStr());
		return false;
	}

	stream->SetName(stream_name.CStr());

	if (media_info->video_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		new_track->SetId(0);
		new_track->SetMediaType(common::MediaType::Video);
		new_track->SetCodecId(common::MediaCodecId::H264);
		new_track->SetWidth((uint32_t)media_info->video_width);
		new_track->SetHeight((uint32_t)media_info->video_height);
		new_track->SetFrameRate(media_info->video_framerate);

		new_track->SetTimeBase(1, 90000);
		stream->AddTrack(new_track);
	}

	if (media_info->audio_streaming)
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

		stream->AddTrack(new_track);
	}

	application->CreateStream2(stream);

	application_id = application->GetId();
	stream_id = stream->GetId();

	logtd("The MPEG-TS stream is ready: [%s/%s] (%u/%u)", app_name.CStr(), stream_name.CStr(), application_id, stream_id);

	return true;
}

bool MpegTsProvider::OnVideoData(info::application_id_t application_id, uint32_t stream_id,
								 uint32_t timestamp,
								 MpegTsFrameType frame_type,
								 const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(application_id));

	if (application == nullptr)
	{
		logte("Could not find an application for [%d/%d]", application_id, stream_id);
		OV_ASSERT2(false);
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logte("Could not find a stream for [%d/%d]", application_id, stream_id);
		OV_ASSERT2(false);
		return false;
	}

	auto packet = std::make_unique<MediaPacket>(common::MediaType::Video,
												0,
												data,
												// PTS
												timestamp,
												// DTS
												timestamp,
												// Duration
												-1LL,
												frame_type == MpegTsFrameType::VideoIFrame ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

	application->SendFrame(stream, std::move(packet));

	return true;
}

bool MpegTsProvider::OnAudioData(info::application_id_t application_id, uint32_t stream_id,
								 uint32_t timestamp,
								 MpegTsFrameType frame_type,
								 const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(application_id));

	if (application == nullptr)
	{
		logte("Could not find an application for [%d/%d]", application_id, stream_id);
		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_id));

	if (stream == nullptr)
	{
		logte("Could not find a stream for [%d/%d]", application_id, stream_id);
		return false;
	}

	auto packet = std::make_unique<MediaPacket>(common::MediaType::Audio,
												1,
												data,
												// PTS
												timestamp,
												// DTS
												timestamp,
												// Duration
												-1LL,
												MediaPacketFlag::Key);

	application->SendFrame(stream, std::move(packet));

	return true;
}

bool MpegTsProvider::OnDeleteStream(info::application_id_t application_id, uint32_t stream_id)
{
	auto application = std::dynamic_pointer_cast<MpegTsApplication>(GetApplicationById(application_id));

	if (application == nullptr)
	{
		logte("Could not find an application for [%d/%d]", application_id, stream_id);
		OV_ASSERT2(false);

		return false;
	}

	auto stream = std::dynamic_pointer_cast<MpegTsStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logte("Could not find a stream for [%d/%d]", application_id, stream_id);
		OV_ASSERT2(false);

		return false;
	}

	return application->DeleteStream2(stream);
}
