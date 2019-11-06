//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <libavutil/intreadwrite.h>

#include "mpegts_define.h"

enum TimeStampType
{
	PTS,
	DTS,
};

struct TSPacket
{
	uint8_t sync_byte = 0;
	bool payload_unit_start_indicator = false;
	bool transport_error_indicator = false;
	uint8_t transport_priority = 0;
	uint16_t packet_identifier = 0;
	uint8_t continuity_counter = 0;
	bool adaptation_field = false;
	uint8_t adaptation_field_length = 0;

	struct PESPacket
	{
		uint8_t stream_id = 0;
		int64_t pts = -1LL;
		int64_t dts = 1LL;
		uint16_t length = 0;
		uint8_t optional_header_length = 0;

		int64_t GetTimeStamp(const void *pes_header, TimeStampType type)
		{
			int offset = 9;

			if (type == TimeStampType::DTS)
			{
				offset += 5;
			}

			const uint8_t *header = static_cast<const uint8_t *>(pes_header);

			auto pos = header + offset;
			auto timestamp = (int64_t)((pos[0] >> 1) & 0x07) << 30;
			timestamp |= (AV_RB16(pos + 1) >> 1) << 15;
			timestamp |= AV_RB16(pos + 3) >> 1;

			return timestamp;
		}

	} pes_packet;
};

class MpegTsElementaryStream
{
public:
	MpegTsElementaryStream(common::MediaType media_type)
		: _media_type(media_type)
	{
		_id = 0;
		_ts = 0;
		_cc = 0;
		_remained = 0;

		_data = std::make_shared<ov::Data>();
	}

	MpegTsFrameType GetFrameType();

	uint16_t GetIdentifier();

	uint32_t GetMediaTimeStamp();

	uint8_t GetContinuityCounter();

	int32_t GetRemainedPacketLength();

	std::shared_ptr<const ov::Data> GetData() const;

	void Add(std::shared_ptr<TSPacket> _packet, const std::shared_ptr<const ov::Data> &chunk_message);
	void Clear();

private:
	common::MediaType _media_type;

	uint16_t _id;
	uint32_t _ts;
	uint8_t _cc;
	int32_t _remained;

	std::shared_ptr<ov::Data> _data;
};
