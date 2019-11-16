//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_observer.h"
#include "mpegts_server.h"

#include <stdint.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

#include <base/media_route/media_buffer.h>
#include <base/media_route/media_type.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/provider/application.h>
#include <base/provider/provider.h>

class MpegTsProvider : public pvd::Provider, public MpegTsObserver
{
public:
	static std::shared_ptr<MpegTsProvider>
	Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

	explicit MpegTsProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

	~MpegTsProvider() override;

	cfg::ProviderType GetProviderType() override
	{
		return cfg::ProviderType::MpegTs;
	}

	bool Start() override;

	std::shared_ptr<pvd::Application> OnCreateApplication(const info::Application *application_info) override;

	//--------------------------------------------------------------------
	// Implementation of MpegTsObserver
	//--------------------------------------------------------------------
	std::shared_ptr<const MpegTsStreamInfo> OnQueryStreamInfo(uint16_t port_num, const ov::SocketAddress *address) override;

	bool OnStreamReady(const std::shared_ptr<const MpegTsStreamInfo> &stream_info,
					   const std::shared_ptr<MpegTsMediaInfo> &media_info) override;

	bool OnVideoData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info,
					 int64_t pts, int64_t dts,
					 MpegTsFrameType frame_type,
					 const std::shared_ptr<const ov::Data> &data) override;

	bool OnAudioData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info,
					 int64_t pts, int64_t dts,
					 MpegTsFrameType frame_type,
					 const std::shared_ptr<const ov::Data> &data) override;

	bool OnDeleteStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info) override;

private:
	bool ParseAddressList(const ov::String &ip, const cfg::RangedPort &ranged_port, std::map<ov::port_t, ov::SocketAddress> *address_list, ov::port_t *min_port);

	const cfg::MpegTsProvider *_provider_info = nullptr;

	std::shared_ptr<MpegTsServer> _mpegts_server;

	// key: port
	// value: stream information
	std::mutex _chunk_stream_list_mutex;
	std::map<uint16_t, std::shared_ptr<MpegTsStreamInfo>> _stream_table;
};
