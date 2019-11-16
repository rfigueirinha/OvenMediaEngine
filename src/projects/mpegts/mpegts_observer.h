//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_datastructure.h"
#include "mpegts_define.h"

#include <base/application/application.h>
#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <memory>

class MpegTsObserver : public ov::EnableSharedFromThis<MpegTsObserver>
{
public:
	// This callback is called to fill the info variable when the client sent a packet to the MPEG-TS server
	virtual std::shared_ptr<const MpegTsStreamInfo> OnQueryStreamInfo(uint16_t port_num, const ov::SocketAddress *address) = 0;

	virtual bool OnStreamReady(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsMediaInfo> &media_info) = 0;
	virtual bool OnVideoData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) = 0;
	virtual bool OnAudioData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) = 0;
	virtual bool OnDeleteStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info) = 0;
};
