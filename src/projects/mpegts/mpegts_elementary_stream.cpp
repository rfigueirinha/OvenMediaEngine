//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_elementary_stream.h"
#include "mpegts_private.h"

MpegTsFrameType MpegTsElementaryStream::GetFrameType()
{
	if (_media_type == common::MediaType::Video)
	{
		int i = 0;
		int nal_unit_type = 0;

		while ((i + 4) < _data->GetLength())
		{
			if (_data->At(i) == 0 && _data->At(i + 1) == 0 && _data->At(i + 2) == 0 && _data->At(i + 3) == 1)
			{
				int nal_unit_type = _data->At(i + 4) & 0x1F;
				if (nal_unit_type == 0x07 || nal_unit_type == 0x05)
				{
					return MpegTsFrameType::VideoIFrame;
				}
			}
			i++;
		}
		return MpegTsFrameType::VideoPFrame;
	}

	return MpegTsFrameType::AudioFrame;
}

uint8_t MpegTsElementaryStream::GetContinuityCounter()
{
	return _cc;
}

uint16_t MpegTsElementaryStream::GetIdentifier()
{
	return _id;
}

int32_t MpegTsElementaryStream::GetRemainedPacketLength()
{
	return _remained;
}

std::shared_ptr<const ov::Data> MpegTsElementaryStream::GetData() const
{
	return _data;
}

uint32_t MpegTsElementaryStream::GetMediaTimeStamp()
{
	return _ts;
}

void MpegTsElementaryStream::Add(std::shared_ptr<TSPacket> _packet,
								 const std::shared_ptr<const ov::Data> &chunk_message)
{
	// pes start packet
	if (_packet->pes_packet.stream_id)
	{
		if (_id == 0)
		{
			_id = _packet->packet_identifier;
		}

		OV_ASSERT2(_id == _packet->packet_identifier);

		_ts = _packet->pes_packet.dts ? _packet->pes_packet.dts : _packet->pes_packet.pts;

		if (_packet->pes_packet.length == 0)
		{
			if (_packet->pes_packet.stream_id == MPEGTS_STREAM_VIDEO)
			{
				_remained = -1;
			}
			else
			{
				// This can not occur in audio
				return;
			}
		}
		else
		{
			_remained = _packet->pes_packet.length - (chunk_message->GetLength() - PES_START_SIZE);
		}

		auto payload_start = PES_HEADER_SIZE + _packet->pes_packet.optional_header_length;

		_data->Append(chunk_message->Subdata(payload_start));
		// _data->insert(_data->end(), chunk_message->begin() + payload_start, chunk_message->end());
	}
	else
	{
		if (_remained != -1)
		{
			_remained -= chunk_message->GetLength();
		}

		_data->Append(chunk_message);
		// _data->insert(_data->end(), chunk_message->begin(), chunk_message->end());
	}

	_cc = _packet->continuity_counter;
}

void MpegTsElementaryStream::Clear()
{
	_data->Clear();
}