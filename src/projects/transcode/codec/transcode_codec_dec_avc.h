//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_decoder.h"

class OvenCodecImplAvcodecDecAVC : public TranscodeDecoder
{
public:

    bool Configure(std::shared_ptr<TranscodeContext> context) override;

    AVCodecID GetCodecID() const noexcept override
    {
        return AV_CODEC_ID_H264;
    }

    const char* GetCodecName() const noexcept
    {
        // TODO set priority of using hardware
        return "h264_cuvid";
    }

    std::unique_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;
};
