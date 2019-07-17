//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "transcode_encoder.h"

#define NVENC_EN

class OvenCodecImplAvcodecEncAVC : public TranscodeEncoder
{
public:
	AVCodecID GetCodecID() const noexcept override
	{
		return AV_CODEC_ID_H264;
	}

	const char* GetCodecName() const noexcept
	{
		std::string codec_name;
#ifdef NVENC_EN
		codec_name = "h264_nvenc";
#endif

#ifdef QSV_EN
		codec_name = "h264_qsv";
#endif

        // TODO set priority of using hardware
		return codec_name.c_str();

	}

	bool Configure(std::shared_ptr<TranscodeContext> context) override;

	std::unique_ptr<MediaPacket> RecvBuffer(TranscodeResult *result) override;

    std::string codec_name;

private:
	std::unique_ptr<FragmentationHeader> MakeFragmentHeader();

    void YUV420PtoNV12(unsigned char *Src, unsigned char* Dst,int Width,int Height);

};
