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
#include "mpegts_utilities.h"

class MpegTsElementaryStream
{
public:
	MpegTsElementaryStream(common::MediaType media_type)
		: _media_type(media_type)
	{
		_data = std::make_shared<ov::Data>();
	}

	MpegTsFrameType GetFrameType();

	uint16_t GetIdentifier();
	int64_t GetPts() const
	{
		return _pts;
	}

	int64_t GetDts() const
	{
		if(_dts == 1)
		{
			auto aaa  = _dts;
		}
		return _dts;
	}

	uint8_t GetContinuityCounter();
	int32_t GetRemainedPacketLength();

	uint16_t GetWidth() const
	{
		return _horizontal_size;
	}

	uint16_t GetHeight() const
	{
		return _vertical_size;
	}

	float GetFramerateAsFloat() const
	{
		switch (_frame_rate)
		{
			case MpegTsFrameRate::Rate23_976:
				return 23.976f;

			case MpegTsFrameRate::Rate24:
				return 24.0f;

			case MpegTsFrameRate::Rate25:
				return 25.0f;

			case MpegTsFrameRate::Rate29_97:
				return 29.97f;

			case MpegTsFrameRate::Rate30:
				return 30.0f;

			case MpegTsFrameRate::Rate50:
				return 50.0f;

			case MpegTsFrameRate::Rate59_94:
				return 59.94f;

			case MpegTsFrameRate::Rate60:
				return 60.0f;
		}

		return 0.0f;
	}

	std::shared_ptr<const ov::Data> GetData() const;

	bool Add(const MpegTsPacket *packet, MpegTsParseData *parse_data);
	void Clear();

private:
	common::MediaType _media_type;

	uint16_t _id = 0U;
	int64_t _pts = -1LL;
	int64_t _dts = -1LL;
	uint8_t _cc = 0U;
	int32_t _remained = -1LL;

	// Referenced: https://en.wikipedia.org/wiki/Elementary_stream

	// For video
	uint16_t _horizontal_size = 0U;								   // 12 bits
	uint16_t _vertical_size = 0U;								   // 12 bits
	MpegTsAspectRatio _aspect_ratio = MpegTsAspectRatio::Unknown;  // 4 bits
	MpegTsFrameRate _frame_rate = MpegTsFrameRate::Unknown;		   // 4 bits
	uint32_t _bit_rate = 0U;									   // 18 bits, Actual bit rate = bit rate * 400, rounded upwards. Use 0x3FFFF for variable bit rate
	uint16_t _vbv_buf_size = 0U;								   // 10 bits, Size of video buffer verifier = 16 * 1024 * vbv buf size
	bool _constrainted_parameters_flag = false;					   // 1 bit
	bool _load_intra_quantizer_matrix = false;					   // 1 bit, If bit set then intra quantizer matrix follows, otherwise use default values.
	// intra quantizer matrix: 0 or 64 * 8 bits
	bool _load_non_intra_quantizer_matrix = false;  // 1 bit, If bit set then non intra quantizer matrix follows.
	// non intra quantizer matrix: 0 or 64 * 8 bits

	std::shared_ptr<ov::Data> _data;
};
