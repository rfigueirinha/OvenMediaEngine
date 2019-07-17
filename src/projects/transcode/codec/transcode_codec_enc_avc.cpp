//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcode_codec_enc_avc.h"

#define OV_LOG_TAG "TranscodeCodec"

extern "C" {
#include <libavutil/hwcontext.h>
}


bool OvenCodecImplAvcodecEncAVC::Configure(std::shared_ptr<TranscodeContext> context)
{
	_transcode_context = context;

	bool hw_accel = true;
	AVCodec *codec = nullptr;

	if (TranscodeContext::GetHwAccel())
	{
        codec = avcodec_find_encoder_by_name(GetCodecName());
	}

    if(codec == nullptr)
	{
		codec = avcodec_find_encoder(GetCodecID());
        hw_accel = false;
	}

	if(codec == nullptr)
	{
		logte("Codec not found");
		return false;
	}

	_context = avcodec_alloc_context3(codec);
	if(!_context)
	{
		logte("Could not allocate video codec context");
		return false;
	}

//    _context->width = 800;
//    _context->height = 600;
//    _context->pix_fmt = codec->pix_fmts[0]; // NV12
//    _context->bit_rate = _transcode_context->GetBitrate();
//    _context->bit_rate_tolerance = _context->bit_rate / 2;
//    _context->time_base.den = _transcode_context->GetFrameRate();
//    _context->time_base.num = 1;
//    _context->framerate.den = 1;
//    _context->framerate.num = _transcode_context->GetFrameRate();
//    _context->gop_size = _transcode_context->GetFrameRate()*5;
//    _context->max_b_frames = 0;

//    av_opt_set(_context->priv_data, "preset", "medium", 0);
//    av_opt_set(_context->priv_data, "look_ahead", "0", 0);

//    av_opt_set(_context->priv_data, "preset", "veryfast", 0);
//    av_opt_set(_context->priv_data, "avbr_accuracy", "1", 0);
//    av_opt_set(_context->priv_data, "async_depth", "1", 0);
//    av_opt_set(_context->priv_data, "profile", "main", 0);

    _context->bit_rate = _transcode_context->GetBitrate();
	_context->rc_max_rate = _context->bit_rate;
	_context->rc_buffer_size = static_cast<int>(_context->bit_rate * 2);
	_context->sample_aspect_ratio = (AVRational){ 1, 1 };
	_context->time_base = (AVRational){
		_transcode_context->GetTimeBase().GetNum(),
        static_cast<int>(_transcode_context->GetFrameRate())
	};
    _context->framerate = av_d2q(_transcode_context->GetFrameRate(), AV_TIME_BASE);
    _context->gop_size = _transcode_context->GetGOP();
    _context->max_b_frames = 0;
	_context->pix_fmt = codec->pix_fmts[0];
	_context->width = _transcode_context->GetVideoWidth();
	_context->height = _transcode_context->GetVideoHeight();
	_context->thread_count = 4;

    if (hw_accel)
	{
#ifdef NVENC_EN
		av_opt_set(_context->priv_data, "preset", "ultrafast", 0);
		av_opt_set(_context->priv_data, "tune", "zerolatency", 0);
		av_opt_set(_context->priv_data, "x264opts", "no-mbtree:sliced-threads:sync-lookahead=0", 0);
#endif

#ifdef QSV_EN
		av_opt_set(_context->priv_data, "preset", "veryfast", 0);
		av_opt_set(_context->priv_data, "avbr_accuracy", "1", 0);
		av_opt_set(_context->priv_data, "async_depth", "1", 0);
		av_opt_set(_context->priv_data, "profile", "baseline", 0);

		int ret = av_hwdevice_ctx_create(&_hw_device_ctx, AVHWDeviceType::AV_HWDEVICE_TYPE_QSV, "auto", nullptr, 0);
		if ( ret < 0)
		{
			char error_msg[AV_ERROR_MAX_STRING_SIZE] = {0, };
			av_strerror(ret, error_msg, sizeof(error_msg));

			logte("Could not create hardware device, error(%d), reason(%s)", ret, error_msg);
		}
		else
		{
			_context->hw_device_ctx = av_buffer_ref(_hw_device_ctx);
		}
#endif
	}

    int ret = avcodec_open2(_context, codec, nullptr);
	if(ret < 0)
	{
        char error_msg[AV_ERROR_MAX_STRING_SIZE] = {0, };
        av_strerror(ret, error_msg, sizeof(error_msg));

		logte("Could not open codec, error(%d), reason(%s)", ret, error_msg);
		return false;
	}

	return true;
}

std::unique_ptr<MediaPacket> OvenCodecImplAvcodecEncAVC::RecvBuffer(TranscodeResult *result)
{
	int ret = avcodec_receive_packet(_context, _pkt);

    if(ret == AVERROR(EAGAIN))
    {
        // Packet is enqueued to encoder's buffer
    }
    else if(ret == AVERROR_EOF)
    {
        logte("Error receiving a packet for decoding : AVERROR_EOF");

        *result = TranscodeResult::DataError;
        return nullptr;
    }
    else if(ret < 0)
    {
        logte("Error receiving a packet for encoding : %d", ret);

        *result = TranscodeResult::DataError;
        return nullptr;
    }
    else
    {
        auto packet_buffer = std::make_unique<MediaPacket>(
                common::MediaType::Video,
                0,
                _pkt->data,
                _pkt->size,
                _pkt->dts,
                (_pkt->flags & AV_PKT_FLAG_KEY) ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag
        );
        packet_buffer->_frag_hdr = MakeFragmentHeader();

        av_packet_unref(_pkt);

        *result = TranscodeResult::DataReady;
        return std::move(packet_buffer);
    }

	while(_input_buffer.size() > 0)
	{
		auto buffer = std::move(_input_buffer[0]);
		_input_buffer.erase(_input_buffer.begin(), _input_buffer.begin() + 1);

		const MediaFrame *frame = buffer.get();
		OV_ASSERT2(frame != nullptr);

#ifdef QSV_EN
		_frame->format = AV_PIX_FMT_NV12;
#else
        _frame->format = frame->GetFormat();
#endif

		_frame->width = frame->GetWidth();
		_frame->height = frame->GetHeight();
		_frame->pts = frame->GetPts();

		_frame->linesize[0] = frame->GetStride(0);
		_frame->linesize[1] = frame->GetStride(1);
		_frame->linesize[2] = frame->GetStride(2);

		if(av_frame_get_buffer(_frame, 32) < 0)
		{
			logte("Could not allocate the video frame data");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		if(av_frame_make_writable(_frame) < 0)
		{
			logte("Could not make sure the frame data is writable");
			*result = TranscodeResult::DataError;
			return nullptr;
		}

		memcpy(_frame->data[0], frame->GetBuffer(0), frame->GetBufferSize(0));
		memcpy(_frame->data[1], frame->GetBuffer(1), frame->GetBufferSize(1));
		memcpy(_frame->data[2], frame->GetBuffer(2), frame->GetBufferSize(2));

		ret = avcodec_send_frame(_context, _frame);

		if(ret < 0)
		{
            char error_msg[AV_ERROR_MAX_STRING_SIZE] = {0, };
            av_strerror(ret, error_msg, sizeof(error_msg));

            logte("Error sending a frame for encoding, error(%d), reason(%s)", ret, error_msg);

			//logte("Error sending a frame for encoding : %d", ret);
		}

		av_frame_unref(_frame);
	}

	*result = TranscodeResult::NoData;
	return nullptr;
}

std::unique_ptr<FragmentationHeader> OvenCodecImplAvcodecEncAVC::MakeFragmentHeader()
{
	auto fragment_header = std::make_unique<FragmentationHeader>();

	int nal_pattern_size = 4;
	int sps_start_index = -1;
	int sps_end_index = -1;
	int pps_start_index = -1;
	int pps_end_index = -1;
	int fragment_count = 0;
	uint32_t current_index = 0;

	while ((current_index + nal_pattern_size) < _pkt->size)
	{
		if (_pkt->data[current_index] == 0 && _pkt->data[current_index + 1] == 0 &&
			_pkt->data[current_index + 2] == 0 && _pkt->data[current_index + 3] == 1)
		{
			// Pattern 0x00 0x00 0x00 0x01
			nal_pattern_size = 4;
		}
		else if (_pkt->data[current_index] == 0 && _pkt->data[current_index + 1] == 0 &&
				 _pkt->data[current_index + 2] == 1)
		{
			// Pattern 0x00 0x00 0x01
			nal_pattern_size = 3;
		}
		else
		{
			current_index++;
			continue;
		}

		int nal_unit_type = _pkt->data[current_index + nal_pattern_size] & 0x1f;

		if ((nal_unit_type == 0x07) || (nal_unit_type == 0x08) || (nal_unit_type == 0x05) || (nal_unit_type == 0x01))
		{
			// SPS, PPS, IDR, Non-IDR
		}
		else
		{
			// nal_unit_type == 0x06 -> SEI
			current_index++;
			continue;
		}

		fragment_count++;

		if (sps_start_index == -1)
		{
			sps_start_index = current_index + nal_pattern_size;
			current_index += nal_pattern_size;

            if(_pkt->flags != AV_PKT_FLAG_KEY)
            {
                break;
            }
		}
		else if (sps_end_index == -1)
		{
			sps_end_index = current_index - 1;
			pps_start_index = current_index + nal_pattern_size;
			current_index += nal_pattern_size;
		}
		else // (pps_end_index == -1)
		{
			pps_end_index = current_index - 1;
			break;
		}
	}

    fragment_header->VerifyAndAllocateFragmentationHeader(fragment_count);

    if (_pkt->flags == AV_PKT_FLAG_KEY) // KeyFrame
	{
	    // SPS + PPS + IDR
		fragment_header->fragmentation_offset[0] = sps_start_index;
		fragment_header->fragmentation_offset[1] = pps_start_index;
		fragment_header->fragmentation_offset[2] = (pps_end_index + 1) + nal_pattern_size;

		fragment_header->fragmentation_length[0] = sps_end_index - (sps_start_index - 1);
		fragment_header->fragmentation_length[1] = pps_end_index - (pps_start_index - 1);
		fragment_header->fragmentation_length[2] = _pkt->size - (pps_end_index + nal_pattern_size);
	}
	else
	{
        // NON-IDR
		fragment_header->fragmentation_offset[0] = sps_start_index ;
		fragment_header->fragmentation_length[0] = _pkt->size - (sps_start_index);
	}

	return std::move(fragment_header);
}


void OvenCodecImplAvcodecEncAVC::YUV420PtoNV12(unsigned char *Src, unsigned char* Dst,int Width,int Height)
{
    unsigned char* SrcU = Src + Width * Height;
    unsigned char* SrcV = SrcU + Width * Height / 4 ;
    unsigned char* DstU = Dst + Width * Height;
    int i = 0;
    for( i = 0 ; i < Width * Height / 4 ; i++ ){
        *(DstU++) = *(SrcU++);
        *(DstU++) = *(SrcV++);
    }
}
