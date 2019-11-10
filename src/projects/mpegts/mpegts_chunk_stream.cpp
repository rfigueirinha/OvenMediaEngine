//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include <string>

#include "mpegts_chunk_stream.h"
#include "mpegts_private.h"

MpegTsChunkStream::MpegTsChunkStream(ov::String client_ipaddr, IMpegTsChunkStream *stream_interface)
	: _client_ipaddr(std::move(client_ipaddr)),
	  _stream_interface(stream_interface)

{
}

int32_t MpegTsChunkStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
{
	int32_t process_size = 0;

	// TODO(dimiden): Use a timer to do this job
	CheckStreamAlive();

	auto process_data = data->Clone();
	process_size = ReceiveChunkPacket(process_data);

	if (process_size < 0)
	{
		logte("Could not process MPEG-TS packet: [%s/%s] (%u/%u), %d",
			  _app_name.CStr(), _stream_name.CStr(),
			  _app_id, _stream_id,
			  process_size);

		return -1;
	}

	return process_size;
}

int32_t MpegTsChunkStream::ReceiveChunkPacket(const std::shared_ptr<const ov::Data> &data)
{
	int32_t process_size = 0;

	while (process_size < static_cast<int32_t>(data->GetLength()))
	{
		/*
         * TS header
         * 1Byte + 2Byte + 1Byte = 4Byte
         *
         * sync_byte : 8bits
         *
         * transport_error_indicator : 1 bit
         * payload_unit_start_indicator : 1 bit
         * transport_priority : 1 bit
         * packet_identifier : 13 bit
         *
         * Transport scrambling control : 2 bit
         * adaptation_field_control : 2 bit
         * Continuity counter : 4bit
         *
         * adaptation_field : (0~N)
         * pes header : (0~N)
         * payload (pes data) : (0~N)
         *
         * Total : 188 byte
         */

		_packet = std::make_shared<TSPacket>();

		// FIXME(dimiden): If the client has sent only part of the TS header, there is a possibility of buffer overflow

		// sync_byte
		_packet->sync_byte = data->At(process_size + 0);

		// payload_unit_start_indicator
		_packet->payload_unit_start_indicator = (data->At(process_size + 1) & TS_UNIT_START_INDICATOR_MASK);

		// transport_error_indicator
		_packet->transport_error_indicator = (data->At(process_size + 1) & TS_TRANSPORT_ERROR_INDICATOR_MASK);

		// transport_priority
		_packet->transport_priority = (data->At(process_size + 1) & TS_TRANSPORT_PRIORITY_MASK);

		// continuity_counter
		_packet->continuity_counter = (data->At(process_size + 3) & TS_CONTINUITY_COUNTER_MASK);

		// packet_identifier
		_packet->packet_identifier = (data->At(process_size + 1) << 8) | data->At(process_size + 2);
		_packet->packet_identifier &= TS_PACKET_IDENTIFIER_MASK;

		// adaptation_field
		_packet->adaptation_field = ((data->At(process_size + 3) & TS_ADAPTATION_FIELD_MASK) == TS_ADAPTATION_FIELD_MASK);

		// adaptation_field_length
		if (_packet->adaptation_field)
		{
			_packet->adaptation_field_length = data->At(process_size + 4);
		}
		else
		{
			_packet->adaptation_field_length = 0;
		}

		if (_packet->packet_identifier == 0)
		{
			process_size += MPEGTS_MAX_PACKET_SIZE;
			continue;
		}

		if (_packet->transport_priority == 1)
		{
			// TODO priority process
			OV_ASSERT2(0);
		}

		if ((_packet->sync_byte != MPEGTS_SYNC_BYTE) || _packet->transport_error_indicator)
		{
			logte("ReceiveChunkPacket error, sync_byte(0x%02X), transport_error_indicator(%d)",
				  _packet->sync_byte,
				  _packet->transport_error_indicator);

			process_size += MPEGTS_MAX_PACKET_SIZE;
			continue;
		}

		auto pes_payload = MPEGTS_HEADER_SIZE + _packet->adaptation_field + _packet->adaptation_field_length;

		auto pes_packet = data->Subdata(process_size + pes_payload, (MPEGTS_MAX_PACKET_SIZE - pes_payload));
		ReceiveChunkMessage(pes_packet);

		process_size += MPEGTS_MAX_PACKET_SIZE;
	}

	return process_size;
}

bool MpegTsChunkStream::ReceiveChunkMessage(const std::shared_ptr<const ov::Data> &chunk_message)
{
	/*
     * pes header (Packetized Elementary Stream)
     * packet start code prefix : 3 byte (00 00 01)
     * stream id : 1 byte (0xC0-0xDF)(Audio) or (0xE0-0xEF)(Video)
     * pes packet length : 2 byte
     * optional pes header : length >= 3 byte
     * stuffing bytes :
     * es data :
     *
     *  00 | 00 00 01 E0 03 D6 84 C0 0A 37 44 2D CD D5 17 44 | .........7D-...D
     *  10 | 2D B6 5F 00 00 00 01 09 F0 00 00 00 01 01 9F AD | -._.............
     *  20 | 6A 41 0F 00 2C FC F8 54 B8 28 19 80 7E 11 33 F4 | jA..,..T.(..~.3.
     *  30 | EB E8 92 EB 09 99 93 BC 4F 13 10 E5 C6 4B C7 3F | ........O....K.?
     */

	if (_packet->payload_unit_start_indicator == 1)
	{
		if ((chunk_message->At(0) == 0x00) && (chunk_message->At(1) == 0x00) && (chunk_message->At(2) == 0x01))
		{
			// stream id : Video -> 0xE0, Audio -> 0xC0
			_packet->pes_packet.stream_id = chunk_message->At(3);

			if ((_packet->pes_packet.stream_id != MPEGTS_STREAM_VIDEO) &&
				_packet->pes_packet.stream_id != MPEGTS_STREAM_AUDIO)
			{
				OV_ASSERT2(0);
				return false;
			}

			_packet->pes_packet.length = chunk_message->At(4) << 8 | chunk_message->At(5);
			_packet->pes_packet.optional_header_length = chunk_message->At(8);

			if ((chunk_message->At(7) & PES_PTS_DTS_MASK) == PES_PTS_DTS_MASK)
			{
				_packet->pes_packet.pts = _packet->pes_packet.GetTimeStamp(chunk_message->GetData(), TimeStampType::PTS);
				_packet->pes_packet.dts = _packet->pes_packet.GetTimeStamp(chunk_message->GetData(), TimeStampType::DTS);
			}
			else if ((chunk_message->At(7) & PES_PTS_MASK) == PES_PTS_MASK)
			{
				_packet->pes_packet.pts = _packet->pes_packet.GetTimeStamp(chunk_message->GetData(), TimeStampType::PTS);
				_packet->pes_packet.dts = 0;
			}
			else
			{
				OV_ASSERT2(0);
				return false;
			}

			if (_packet->pes_packet.stream_id == MPEGTS_STREAM_VIDEO)
			{
				// pes packet length zero
				if (_video_data->GetRemainedPacketLength() == -1)
				{
					// TODO check packet loss process
					ReceiveVideoMessage();
				}

				if (_video_data->GetRemainedPacketLength() > 0)
				{
					// TODO packet loss process
					return false;
				}

				_video_data->Add(_packet, chunk_message);
			}
			else if (_packet->pes_packet.stream_id == MPEGTS_STREAM_AUDIO)
			{
				if (_audio_data->GetRemainedPacketLength() > 0)
				{
					// TODO packet loss process
					return false;
				}
				_audio_data->Add(_packet, chunk_message);
			}
		}
		else
		{
			// This is not a audio or video stream
		}
	}
	else
	{
		if (_packet->packet_identifier == _video_data->GetIdentifier())
		{
			_video_data->Add(_packet, chunk_message);
		}
		else if (_packet->packet_identifier == _audio_data->GetIdentifier())
		{
			_audio_data->Add(_packet, chunk_message);
		}
		else
		{
			// This is not a audio or video stream
		}
	}

	if (_video_data->GetRemainedPacketLength() == 0 && _video_data->GetData()->GetLength())
	{
		ReceiveVideoMessage();
	}

	if (_audio_data->GetRemainedPacketLength() == 0 && _audio_data->GetData()->GetLength())
	{
		ReceiveAudioMessage();
	}

	if (_video_data->GetRemainedPacketLength() < 0)
	{
		//OV_ASSERT2(0);
		return false;
	}

	if (_audio_data->GetRemainedPacketLength() < 0)
	{
		//OV_ASSERT2(0);
		return false;
	}

	return true;
}

bool MpegTsChunkStream::ReceiveVideoMessage()
{
	if (_video_data->GetFrameType() != MpegTsFrameType::VideoIFrame)
	{
		_video_data->Clear();
		return false;
	}

	_media_info->video_streaming = true;

	if ((_media_info->start_streaming == false) && (_media_info->video_streaming && _media_info->audio_streaming))
	{
		_media_info->start_streaming = StreamCreate();
	}

	if (_media_info->start_streaming == false)
	{
		_media_info->video_streaming = false;
		_video_data->Clear();
		return false;
	}

	_stream_interface->OnChunkStreamVideoData(nullptr,
											  _app_id, _stream_id,
											  _video_data->GetMediaTimeStamp(),
											  _video_data->GetFrameType(),
											  _video_data->GetData());
	_video_data->Clear();

	return true;
}

bool MpegTsChunkStream::ReceiveAudioMessage()
{
	_media_info->audio_streaming = true;

	auto data = _audio_data->GetData();

	int adts_start = 0;

	while (adts_start < data->GetLength())
	{
		// Invalid audio data
		if (data->At(adts_start) != 0xFF)
		{
			//OV_ASSERT2(0);
			return false;
		}

		uint32_t frame_length = ((data->At(adts_start + 3) & 0x03) << 11 | (data->At(adts_start + 4) & 0xFF) << 3 | data->At(adts_start + 5) >> 5);

		auto buffer = data->Subdata(adts_start, frame_length);

		adts_start += frame_length;

		_stream_interface->OnChunkStreamAudioData(nullptr,
												  _app_id, _stream_id,
												  _audio_data->GetMediaTimeStamp(),
												  _audio_data->GetFrameType(),
												  buffer);
	}

	_audio_data->Clear();

	return true;
}

bool MpegTsChunkStream::StreamCreate()
{
	return _stream_interface->OnChunkStreamReady(_client_ipaddr, _app_name, _stream_name, _media_info, _app_id, _stream_id);
}

void MpegTsChunkStream::CheckStreamAlive()
{
	auto current = ::time(nullptr);

	if ((_last_packet_time != 0ULL) && ((current - _last_packet_time) > MPEGTS_ALIVE_TIME))
	{
		_stream_interface->OnDeleteStream(nullptr, _app_name, _stream_name, _app_id, _stream_id);

		_media_info->start_streaming = false;
		_media_info->video_streaming = false;
		_media_info->audio_streaming = false;
	}

	_last_packet_time = current;
}