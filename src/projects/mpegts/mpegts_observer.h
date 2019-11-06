//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_define.h"

#include <base/application/application.h>
#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>
#include <memory>

class MpegTsObserver : public ov::EnableSharedFromThis<MpegTsObserver>
{
public:
	virtual bool OnStreamReadyComplete(const ov::String &app_name, const ov::String &stream_name, std::shared_ptr<MpegTsMediaInfo> &media_info, info::application_id_t &application_id, uint32_t &stream_id) = 0;
	virtual bool OnVideoData(info::application_id_t application_id, uint32_t stream_id, uint32_t timestamp, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) = 0;
	virtual bool OnAudioData(info::application_id_t application_id, uint32_t stream_id, uint32_t timestamp, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) = 0;
	virtual bool OnDeleteStream(info::application_id_t application_id, uint32_t stream_id) = 0;
};
