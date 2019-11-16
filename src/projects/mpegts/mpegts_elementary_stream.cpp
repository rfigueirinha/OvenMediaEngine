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

		while ((i + 5) < _data->GetLength())
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

bool MpegTsElementaryStream::Add(const MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	if (packet != nullptr)
	{
		auto &pes = packet->pes;

		if (pes.stream_id > 0)
		{
			if (pes.IsVideoStream())
			{
				MpegTsParseData data_for_validate = *parse_data;

				// Check a sequence header (it starts with 0x000001B3)
				if (
					(data_for_validate.ReadByte() == 0x00) &&
					(data_for_validate.ReadByte() == 0x00) &&
					(data_for_validate.ReadByte() == 0x01) &&
					(data_for_validate.ReadByte() == 0xB3))
				{
					//  76543210  76543210  76543210  76543210
					// [hhhhhhhh][hhhhvvvv][vvvvvvvv][aaaaffff]
					// h: horizontal_size
					_horizontal_size = data_for_validate.ReadBits16(12);
					// v: vertical_size
					_vertical_size = data_for_validate.ReadBits16(12);
					// a: aspect_ratio
					//     1: square samples
					//     2: 4:3 display aspect ratio
					//     3: 16:9 display aspect ratio
					_aspect_ratio = static_cast<MpegTsAspectRatio>(data_for_validate.Read(4));
					// f: frame_rate
					_frame_rate = static_cast<MpegTsFrameRate>(data_for_validate.Read(4));

					//  76543210  76543210  76543210  76543210
					// [bbbbbbbb][bbbbbbbb][bbmvvvvv][vvvvvcin]

					// b: bit_rate
					_bit_rate = (data_for_validate.ReadBits16(16) << 16) | data_for_validate.Read(2);
					// m: marker bit, must be 1
					if (data_for_validate.Read1() != 0b1)
					{
						logte("Invalid marker bit in video elementary stream");
						return false;
					}

					// v: vbv_buf_size
					_vbv_buf_size = data_for_validate.ReadBits16(10);
					// c: constrainted_parameters_flag
					_constrainted_parameters_flag = data_for_validate.ReadBoolBit();
					// i: load_intra_quantizer_matrix
					_load_intra_quantizer_matrix = data_for_validate.ReadBoolBit();
					// n:  load_non_intra_quantizer_matrix
					_load_non_intra_quantizer_matrix = data_for_validate.ReadBoolBit();
				}
				else
				{
				}
			}

			if (_id == 0)
			{
				_id = packet->packet_identifier;
			}

			OV_ASSERT2(_id == packet->packet_identifier);

			_pts = pes.pts;
			_dts = pes.dts;

			if (pes.length == 0)
			{
				// A value of 0 indicates that the PES packet length is neither specified nor bounded and is allowed only in
				// PES packets whose payload consists of bytes from a video elementary stream contained in Transport Stream packets

				if (pes.IsVideoStream() == false)
				{
					OV_ASSERT2(false);
					return false;
				}

				_remained = -1LL;
			}
			else
			{
				_remained = pes.length;
			}
		}

		_cc = packet->continuity_counter;
	}

	if (_data->Append(parse_data->GetCurrent(), parse_data->GetRemained()))
	{
		if (_remained >= 0)
		{
			OV_ASSERT(_remained >= parse_data->GetRemained(), "Not enough data: %d bytes, expected: %zu bytes", _remained, parse_data->GetRemained());
			_remained -= parse_data->GetRemained();
		}

		parse_data->SkipAll();
		return true;
	}

	return false;
}

void MpegTsElementaryStream::Clear()
{
	_data->Clear();

	_id = 0U;
	_pts = -1LL;
	_dts = -1LL;
	_cc = 0U;
	_remained = -1LL;
}