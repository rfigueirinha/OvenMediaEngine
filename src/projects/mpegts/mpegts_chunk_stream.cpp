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

MpegTsChunkStream::MpegTsChunkStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, IMpegTsChunkStream *stream_interface)
	: _stream_info(stream_info),
	  _stream_interface(stream_interface)
{
}

int32_t MpegTsChunkStream::OnDataReceived(const std::shared_ptr<const ov::Data> &data)
{
	int32_t process_size = ParseTsHeader(data);

	if (process_size < 0)
	{
		logte("Could not parse MPEG-TS packet: %s, %d", _stream_info->ToString().CStr(), process_size);

		return -1;
	}

	return process_size;
}

int32_t MpegTsChunkStream::ParseTsHeader(const std::shared_ptr<const ov::Data> &data)
{
	int32_t process_size = 0;
	size_t remained = data->GetLength();
	auto current = data->GetDataAs<const uint8_t>();

	while (remained > 0)
	{
		// TODO(dimiden): Handle even when packet size is 188/192/204
		if (remained < MPEGTS_MAX_PACKET_SIZE)
		{
			logte("Could not parse MPEG-TS packet: Packet size is too small: %zu", remained);
			break;
		}

		MpegTsParseData parse_data(current, MPEGTS_MAX_PACKET_SIZE);
		MpegTsPacket packet;

		//  76543210  76543210  76543210  76543210
		// [ssssssss][tpTPPPPP][PPPPPPPP][SSaacccc]...

		// s: sync byte
		packet.sync_byte = parse_data.ReadByte();

		if (packet.sync_byte != MPEGTS_SYNC_BYTE)
		{
			logte("Could not parse MPEG-TS packet: Invalid sync byte found: %d (%02X)", packet.sync_byte, packet.sync_byte);
			break;
		}

		// t: transport error indicator
		packet.transport_error_indicator = parse_data.ReadBoolBit();
		// p: payload unit start indicator
		packet.payload_unit_start_indicator = parse_data.ReadBoolBit();
		// T: transport priority
		packet.transport_priority = parse_data.Read1();
		// P: packet identifier
		packet.packet_identifier = parse_data.ReadBits16(13);

		if (packet.transport_error_indicator)
		{
			logte("The packet indicates error for PID %d (%04X)", packet.packet_identifier, packet.packet_identifier);
			break;
		}

		// S: transport scrambling control
		packet.transport_scrambling_control = parse_data.Read(2);
		// a: adaption field control
		packet.adaptation_field_control = parse_data.Read(2);
		// c: continuity counter
		packet.continuity_counter = parse_data.Read(4);

		// Check if there is an adaption field
		if (packet.adaptation_field_control != 0b00)
		{
			// 01: No adaptation_field, payload only
			// 10: Adaptation_field only, no payload
			// 11: Adaptation_field followed by payload
			bool has_adaptation_field = OV_GET_BIT(packet.adaptation_field_control, 1);
			bool has_payload = OV_GET_BIT(packet.adaptation_field_control, 0);

			if (has_adaptation_field)
			{
				if (ParseAdaptationHeader(&packet, &parse_data) == false)
				{
					logte("Could not parse adaptation header");
					OV_ASSERT2(false);
					break;
				}
			}

			if (has_payload)
			{
				if (ParsePayload(&packet, &parse_data) == false)
				{
					logte("Could not parse payload");
					OV_ASSERT2(false);
					break;
				}
			}

			uint8_t expected_cc = has_payload ? (_last_continuity_counter + 1) & 0x0f : _last_continuity_counter;

			bool cc_ok =
				(packet.packet_identifier == MPEGTS_PID_NULL_PACKET) ||
				packet.adaptation_field.discontinuity_indicator ||
				(_last_continuity_counter < 0) ||
				(packet.continuity_counter == expected_cc);

			if (cc_ok == false)
			{
				logtw("Invalid continuity counter for pid %d (%04x): %d (expected: %d)",
					  packet.packet_identifier, packet.packet_identifier,
					  packet.continuity_counter, expected_cc);
			}

			if (packet.transport_priority == 1)
			{
				// TODO(dimiden): Need to handle the priority packet
				OV_ASSERT2(false);
			}

			// logtd("%s", packet.ToString().CStr());

#if DEBUG
			auto remained_bytes = parse_data.GetRemained();
			auto remained_data = parse_data.GetCurrent();

			while (remained_bytes > 0)
			{
				if (remained_data[(remained_bytes - 1)] != 0xFF)
				{
					logte("Remained\n%s", parse_data.Dump().CStr());
					OV_ASSERT2(false);
					break;
				}

				remained_bytes--;
			}
#endif  // DEBUG
		}
		else
		{
			// 00: Reserved for future use by ISO/IEC
			OV_ASSERT2(false);
		}

		process_size += MPEGTS_MAX_PACKET_SIZE;
		current += MPEGTS_MAX_PACKET_SIZE;
		remained -= MPEGTS_MAX_PACKET_SIZE;
	}

	return process_size;
}

bool MpegTsChunkStream::ParseAdaptationHeader(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	// adaptation field length: 8 bits
	packet->adaptation_field.length = parse_data->ReadByte();

	bool has_adaptation = OV_GET_BIT(packet->adaptation_field_control, 1);
	auto skip_bytes = packet->adaptation_field.length;

	if (has_adaptation)
	{
		if (packet->adaptation_field.length > 0)
		{
			// parse indicators
			packet->adaptation_field.discontinuity_indicator = parse_data->ReadBoolBit();
			packet->adaptation_field.random_access_indicator = parse_data->ReadBoolBit();
			packet->adaptation_field.elementary_stream_priority_indicator = parse_data->ReadBoolBit();

			// 5 flags
			packet->adaptation_field.pcr_flag = parse_data->ReadBoolBit();
			packet->adaptation_field.opcr_flag = parse_data->ReadBoolBit();
			packet->adaptation_field.splicing_point_flag = parse_data->ReadBoolBit();
			packet->adaptation_field.transport_private_data_flag = parse_data->ReadBoolBit();
			packet->adaptation_field.adaptation_field_extension_flag = parse_data->ReadBoolBit();

			skip_bytes--;
		}
		else
		{
			// no adaption fields
		}
	}
	else
	{
		// adaptation field is not available
	}

#if 0
	if (packet->adaptation_field.pcr_flag)
	{
		// TODO(dimiden): Need to parse PCR (48 bits)
		parse_data->Skip(6);
	}

	if (packet->adaptation_field.opcr_flag)
	{
		// TODO(dimiden): Need to parse OPCR (48 bits)
		parse_data->Skip(6);
	}

	if (packet->adaptation_field.splicing_point_flag)
	{
		// TODO(dimiden): Need to parse Splice countdown fields (8 bits)
		parse_data->Skip(1);
	}

	if (packet->adaptation_field.transport_private_data_flag)
	{
		// TODO(dimiden): Need to parse transport private data
		packet->adaptation_field.transport_private_data.length = parse_data->ReadByte();
	}

	if (packet->adaptation_field.adaptation_field_extension_flag)
	{
		packet->adaptation_field.extension.length = parse_data->ReadByte();
	}
#endif

	return parse_data->Skip(skip_bytes);
}

bool MpegTsChunkStream::ParsePayload(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	return packet->HasPSI() ? ParsePsi(packet, parse_data) : ParsePes(packet, parse_data);
}

bool MpegTsChunkStream::ParseService(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsService *service)
{
	if (parse_data->IsRemained(5) == false)
	{
		logte("Length must be at least %u byte(s) to parse service, but %zu", 5, parse_data->GetRemained());
		return false;
	}

	service->service_id = parse_data->Read2Bytes();

	//  76543210  76543210  76543210
	// [------sp][rrrfllll][llllllll]...
	// -: reserved
	service->reserved_future_use = parse_data->Read(6);
	// s: eit_schedule_flag
	service->eit_schedule_flag = parse_data->ReadBoolBit();
	// p: eit_present_follwing_flag
	service->eit_present_follwing_flag = parse_data->ReadBoolBit();
	// r: running_status
	service->running_status = parse_data->Read(3);
	// f: free_CA_mode
	service->free_ca_mode = parse_data->ReadBoolBit();
	// l: descriptors_loop_length
	service->descriptors_loop_length = parse_data->ReadBits16(12);

	if (parse_data->IsRemained(service->descriptors_loop_length) == false)
	{
		logte("Length must be at least %u byte(s) to parse service descriptors, but %zu", service->descriptors_loop_length, parse_data->GetRemained());
		return false;
	}

	auto descriptors_loop_length = service->descriptors_loop_length;

	while (descriptors_loop_length > 0)
	{
		MpegTsServiceDescriptor descriptor;

		if (ParseServiceDescriptor(packet, parse_data, &descriptor) == false)
		{
			return false;
		}

		if (descriptors_loop_length < descriptor.length)
		{
			logte("descriptors_loop_length is less than parsed bytes: %u < %u", descriptors_loop_length, descriptor.length);
			return false;
		}

		descriptors_loop_length -= descriptor.length;

		service->descriptors.push_back(std::move(descriptor));
	}

	return true;
}

bool MpegTsChunkStream::ParseServiceDescriptor(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsServiceDescriptor *descriptor)
{
	if (parse_data->IsRemained(5) == false)
	{
		logte("Length must be at least 5 bytes to parse service descriptor, but  %zu", parse_data->GetRemained());
		return false;
	}

	descriptor->descriptor_tag = parse_data->ReadByte();
	descriptor->descriptor_length = parse_data->ReadByte();
	descriptor->service_type = parse_data->ReadByte();
	descriptor->service_provider_name_length = parse_data->ReadByte();

	// +1: size of service_name_length
	if (parse_data->IsRemained(descriptor->service_provider_name_length + 1) == false)
	{
		logte("Length must be at least %u byte(s) to parse service provider name, but %zu", (descriptor->service_provider_name_length + 1), parse_data->GetRemained());
		return false;
	}

	descriptor->service_provider_name = std::move(parse_data->ReadString(descriptor->service_provider_name_length));
	descriptor->service_name_length = parse_data->ReadByte();

	if (parse_data->IsRemained(descriptor->service_name_length) == false)
	{
		logte("Length must be at least %u byte(s) to parse service name, but %zu", descriptor->service_name_length, parse_data->GetRemained());
		return false;
	}

	descriptor->service_name = std::move(parse_data->ReadString(descriptor->service_name_length));

	descriptor->length = 5 + descriptor->service_provider_name_length + descriptor->service_name_length;

	return true;
}

bool MpegTsChunkStream::ParseProgram(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsProgram *program)
{
	if (parse_data->IsRemained(4) == false)
	{
		logte("Length must be at least 4 bytes to parse program, but %u", parse_data->GetRemained());
		return false;
	}

	//  76543210  76543210  76543210  76543210
	// [pppppppp][pppppppp][---PPPPP][PPPPPPPP]...
	// -: reserved

	// p: program_number
	program->program_number = parse_data->Read2Bytes();
	program->reserved = parse_data->Read(3);

	// Both of codes are same (because network_pid and program_map_pid are union)
	//
	// if (program->program_number == 0)
	// {
	// 	program->network_pid = OV_GET_BITS(uint8_t, byte, 5, 3) << 8;
	// 	program->network_pid |= *(current++);
	// }
	// else
	// {
	// 	program->program_map_pid = OV_GET_BITS(uint8_t, byte, 5, 3) << 8;
	// 	program->program_map_pid |= *(current++);
	// }
	program->pid = parse_data->ReadBits16(5 + 8);

	return true;
}

bool MpegTsChunkStream::ParseCommonTableHeader(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsTableCommon *table)
{
	//  76543210  76543210  76543210  76543210  76543210
	// [iiiiiiii][s0--llll][llllllll][tttttttt][tttttttt]...
	// 0, -: hard coded/reserved
	// i: table_id
	table->table_id = parse_data->ReadByte();
	// s: section_syntax_indicator
	table->section_syntax_indicator = parse_data->Read1();
	table->zero = parse_data->Read1();
	table->reserved0 = parse_data->Read(2);
	// section_length: 12 bits
	table->section_length = parse_data->ReadBits16(12);

	if (parse_data->IsRemained(table->section_length) == false)
	{
		logte("Invalid section length: %u, must be greater than %zu", table->section_length, parse_data->GetRemained());
		return false;
	}

	return true;
}

bool MpegTsChunkStream::ParsePsi(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	if (packet->payload_unit_start_indicator)
	{
		packet->psi.pointer_field = parse_data->ReadByte();
	}

	switch (packet->packet_identifier)
	{
		case MPEGTS_PID_PAT:
			return ParsePat(packet, parse_data);

		case MPEGTS_PID_CAT:
			// TODO(dimiden): Need to parse TSDT
			OV_ASSERT2(false);
			return parse_data->SkipAll();

		case MPEGTS_PID_TSDT:
			// TODO(dimiden): Need to parse TSDT
			OV_ASSERT2(false);
			return parse_data->SkipAll();

		case MPEGTS_PID_PMT:
			return ParsePmt(packet, parse_data);

		case MPEGTS_PID_SDT:
			return ParseSdt(packet, parse_data);

		default:
			// Unknown table
			logte("Unknown PID: %u", packet->packet_identifier);
			OV_ASSERT2(false);
			break;
	}

	return false;
}

bool MpegTsChunkStream::ParsePat(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	auto &pat = packet->psi.pat;

	if (ParseCommonTableHeader(packet, parse_data, &pat) == false)
	{
		logte("Could not parse common table header");
		return false;
	}

	OV_ASSERT(pat.zero == 0, "Invalid reserved field value: %d, Expected: 0", pat.zero);

	MpegTsParseData pat_parse_data(parse_data->GetCurrent(), pat.section_length);
	parse_data->Skip(pat.section_length);

	// transport_stream_id : 16 bits
	pat.transport_stream_id = pat_parse_data.Read2Bytes();

	//  76543210  76543210  76543210
	// [--vvvvvc][ssssssss][llllllll]...
	// -: reserved
	pat.reserved1 = pat_parse_data.Read(2);
	// v: version_number
	pat.version_number = pat_parse_data.Read(5);
	// c: current_next_indicator
	pat.current_next_indicator = pat_parse_data.Read1();
	// s: section_number
	pat.section_number = pat_parse_data.ReadByte();
	// l: last_section_number
	pat.last_section_number = pat_parse_data.ReadByte();

	while (pat_parse_data.GetRemained() > 4)
	{
		MpegTsProgram program;

		if (ParseProgram(packet, &pat_parse_data, &program) == false)
		{
			logte("Could not parse program");
			return false;
		}

		pat.programs.push_back(std::move(program));
	}

	if (pat_parse_data.IsRemained(4) == false)
	{
		logte("Not enough data");
		return false;
	}

	pat.crc_32 = pat_parse_data.Read4Bytes();

	return true;
}

bool MpegTsChunkStream::ParsePmt(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	auto &pmt = packet->psi.pmt;

	if (ParseCommonTableHeader(packet, parse_data, &pmt) == false)
	{
		logte("Could not parse common table header");
		return false;
	}

	OV_ASSERT(pmt.zero == 0, "Invalid reserved field value: %d, Expected: 0", pmt.zero);

	// TODO(dimiden): Need to implement PMT parser
	parse_data->SkipAll();

	return true;
}

bool MpegTsChunkStream::ParseSdt(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	auto &sdt = packet->psi.sdt;

	if (ParseCommonTableHeader(packet, parse_data, &sdt) == false)
	{
		logte("Could not parse common table header");
		return false;
	}

	MpegTsParseData sdt_parse_data(parse_data->GetCurrent(), sdt.section_length);
	parse_data->Skip(sdt.section_length);

	// transport_stream_id : 16 bits
	sdt.transport_stream_id = sdt_parse_data.Read2Bytes();

	//  76543210  76543210  76543210
	// [--vvvvvc][ssssssss][llllllll]...
	// -: reserved
	sdt.reserved1 = sdt_parse_data.Read(2);
	// v: version_number
	sdt.version_number = sdt_parse_data.Read(5);
	// c: current_next_indicator
	sdt.current_next_indicator = sdt_parse_data.Read1();
	// s: section_number
	sdt.section_number = sdt_parse_data.ReadByte();
	// l: last_section_number
	sdt.last_section_number = sdt_parse_data.ReadByte();

	sdt.original_network_id = sdt_parse_data.Read2Bytes();
	sdt.reserved_future_use = sdt_parse_data.ReadByte();

	while (sdt_parse_data.GetRemained() > 4)
	{
		MpegTsService service;

		if (ParseService(packet, &sdt_parse_data, &service) == false)
		{
			logte("Could not parse service");
			return false;
		}

		sdt.services.push_back(std::move(service));
	}

	OV_ASSERT2(sdt_parse_data.GetRemained() == 4);

	sdt.crc_32 = sdt_parse_data.Read4Bytes();

	return true;
}

bool MpegTsChunkStream::ParsePes(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	//
	// PES (Packetized Elementary Stream) header structure (ISO/IEC 13818-1)
	//
	// packet start code prefix: 24 bits, 0x00 0x00 0x01   ---+
	// stream id: 8 bits                                      |----  Start code
	//     Audio streams: 0xC0-0xDF (0b110xxxxx)              |
	//     Video streams: 0xE0-0xEF (0b1110xxxx)           ---+
	// PES packet length: 16 bits
	//     - Specifies the number of bytes remaining in the packet after this field.
	//     - Can be zero.
	//     - If the PES packet length is set to zero, the PES packet can be of any length.
	//     - A value of zero for the PES packet length can be used only when the PES packet payload is a video elementary stream.
	// optional PES HEADER
	//     - not present in case of Padding stream & Private stream 2 (navigation data)
	//     '10': 2 bits
	//     PES scrambling control: 2 bits
	//     PES priority: 1 bit
	//     data alignment indicator: 1 bit
	//     copyright: 1
	//     original or copy: 1
	//     7 flags (*): 8
	//     PES header data length: 8
	//     optional fields (*)
	//         PTS/DTS: 33 bits
	//         ESCR: 42 bits
	//         ES rate: 22 bits
	//         DSM trick mode: 8 bits
	//         additional copy info: 7 bits
	//         previous PES CRC: 16 bits
	//         PES extension
	//             5 flags (**)
	//             optional fields (**)
	//                 PES private data: 128 bits
	//                 pack header field: 8 bits
	//                 program packet seq cntr: 8 bits
	//                 P-STD buffer: 16 bits
	//                 PES extension field length: 7 bits
	//                 PES extension field data: (variable)
	//     stuffing bytes (0xFF): m * 8
	// PES packet data abytes
	//     - See elementary stream. In the case of private streams the first byte of the payload is the sub-stream number.
	//
	//  00 | 00 00 01 E0 03 D6 84 C0 0A 37 44 2D CD D5 17 44 | .........7D-...D
	//  10 | 2D B6 5F 00 00 00 01 09 F0 00 00 00 01 01 9F AD | -._.............
	//  20 | 6A 41 0F 00 2C FC F8 54 B8 28 19 80 7E 11 33 F4 | jA..,..T.(..~.3.
	//  30 | EB E8 92 EB 09 99 93 BC 4F 13 10 E5 C6 4B C7 3F | ........O....K.?
	//

	if (packet->payload_unit_start_indicator == 1)
	{
		if ((parse_data->ReadByte() == 0x00) && (parse_data->ReadByte() == 0x00) && (parse_data->ReadByte() == 0x01))
		{
			// Skip packet_start_code_prefix bytes
			auto &pes = packet->pes;

			pes.stream_id = parse_data->ReadByte();
			pes.length = parse_data->Read2Bytes();

			bool parsed = true;

			switch (pes.stream_id)
			{
				case MPEGTS_STREAM_ID_PROGRAM_STREAM_MAP:
				case MPEGTS_STREAM_ID_PADDING_STREAM:
				case MPEGTS_STREAM_ID_PRIVATE_STREAM_2:
				case MPEGTS_STREAM_ID_ECM_STREAM:
				case MPEGTS_STREAM_ID_EMM_STREAM:
				case MPEGTS_STREAM_ID_PROGRAM_STREAM_DIRECTORY:
				case MPEGTS_STREAM_ID_DSMCC:
				case MPEGTS_STREAM_ID_H_222_1_TYPE_E:
					parsed = ParsePesPayload(packet, parse_data) && ParsePesPaddingBytes(packet, parse_data, &pes);
					break;

				default:
					parsed = ParsePesOptionalHeader(packet, parse_data, &pes) && ParsePesPayload(packet, parse_data);
					break;
			}

			return parsed;
		}

		// This is not a PES packet
		logte("This it not a PES packet");
		return false;
	}

	if (ParsePesPayload(packet, parse_data) == false)
	{
		logte("Could not parse PES payload");
		return false;
	}

	return true;
}

bool MpegTsChunkStream::ParsePesOptionalHeader(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsPacket::PesPacket *pes)
{
	if ((pes->IsAudioStream() == false) && (pes->IsVideoStream() == false))
	{
		// Unknown stream id
		logte("Unknown stream id: %u (0x%04x)", pes->stream_id);
		return false;
	}

	//  76543210
	// [10sspdco]

	// '10'
	pes->one_zero = parse_data->Read(2);
	OV_ASSERT2(pes->one_zero == 0b10);
	// s: PES_scrambling_control
	pes->pes_scrambling_control = parse_data->Read(2);
	// p: PES_priority
	pes->pes_priority = parse_data->Read1();
	// d: data_alignment_indicator
	pes->data_alignment_indicator = parse_data->Read1();
	// c: copyright
	pes->copyright = parse_data->Read1();
	// o: original_or_copy
	pes->original_or_copy = parse_data->Read1();

	//  76543210
	// [DDEemaRx]
	// D: PTS_DTS_flags
	pes->pts_dts_flags = parse_data->Read(2);
	// E: ESCR_flag
	pes->escr_flag = parse_data->Read1();
	// e: ES_rate_flag
	pes->es_rate_flag = parse_data->Read1();
	// m: DSM_trick_mode_flag
	pes->dsm_trick_mode_flag = parse_data->Read1();
	// a: additional_copy_info_flag
	pes->additional_copy_info_flag = parse_data->Read1();
	// R: PES_CRC_flag
	pes->pes_crc_flag = parse_data->Read1();
	// x: PES_extension_flag
	pes->pes_extension_flag = parse_data->Read1();

	pes->pes_header_data_length = parse_data->ReadByte();
	uint8_t needed_bytes = (pes->pts_dts_flags == 0b10) ? 5 : 10;
	if (pes->pes_header_data_length < needed_bytes)
	{
		logte("Invalid pes_header_data_length: %u, must be greater than %d", pes->pes_header_data_length, needed_bytes);
		return false;
	}

	switch (pes->pts_dts_flags)
	{
		case 0b10:
			// PTS only
			if (ParseTimestamp(packet, parse_data, 0b0010, &(pes->pts)) == false)
			{
				logte("Could not parse PTS");
				return false;
			}

			pes->dts = pes->pts;

			break;

		case 0b11:
			// PTS+DTS
			if (
				(ParseTimestamp(packet, parse_data, 0b0011, &(pes->pts)) == false) ||
				(ParseTimestamp(packet, parse_data, 0b0001, &(pes->dts)) == false))
			{
				logte("Could not parse PTS & DTS");
				return false;
			}

			break;

		default:
			logte("Unknown pts_dts_flags: %d", pes->pts_dts_flags);
			return false;
	}

	// TODO(dimiden): Need to check stuffing bytes - stuffing bytes cannot be processed until an optional fields parser such as ESCR, ES rate, etc. is implemented

	return true;
}

bool MpegTsChunkStream::ParsePesPayload(MpegTsPacket *packet, MpegTsParseData *parse_data)
{
	if (packet->payload_unit_start_indicator)
	{
		auto &pes = packet->pes;

		// Process pes header & payload
		if (pes.IsAudioStream())
		{
			if (_audio_data->GetData()->GetLength() > 0ULL)
			{
				ProcessAudioMessage();
				_audio_data->Clear();
			}

			return _audio_data->Add(packet, parse_data);
		}
		else if (pes.IsVideoStream())
		{
			if (_video_data->GetData()->GetLength() > 0ULL)
			{
				ProcessVideoMessage();
				_video_data->Clear();
			}

			return _video_data->Add(packet, parse_data);
		}

		logte("Unknown stream: %u", packet->packet_identifier);
	}
	else
	{
		if (_media_info->is_streaming)
		{
			// Process payload only
			if (packet->packet_identifier == _audio_data->GetIdentifier())
			{
				return _audio_data->Add(nullptr, parse_data);
			}
			else if (packet->packet_identifier == _video_data->GetIdentifier())
			{
				return _video_data->Add(nullptr, parse_data);
			}

			logte("Unknown packet identifier: %u", packet->packet_identifier);
		}
		else
		{
			// skip
			logtd("Streaming is not started. Ignoring PES data...");
			return parse_data->SkipAll();
		}
	}

	return false;
}

bool MpegTsChunkStream::ParsePesPaddingBytes(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsPacket::PesPacket *pes)
{
	return true;
}

bool MpegTsChunkStream::ParseTimestamp(MpegTsPacket *packet, MpegTsParseData *parse_data, uint8_t start_bits, int64_t *timestamp)
{
	//  76543210  76543210  76543210  76543210  76543210
	// [ssssTTTm][TTTTTTTT][TTTTTTTm][TTTTTTTT][TTTTTTTm]
	//
	//      TTT   TTTTTTTT  TTTTTTT   TTTTTTTT  TTTTTTT
	//      210   98765432  1098765   43210987  6543210
	//        3              2            1           0
	// s: start_bits
	// m: marker_bit (should be 1)
	// T: timestamp

	auto prefix = parse_data->Read(4);
	if (prefix != start_bits)
	{
		logte("Invalid PTS_DTS start bits: %d (%02X), expected: %d (%02X)", prefix, prefix, start_bits, start_bits);
		return false;
	}

	int64_t ts = parse_data->Read(3) << 30;

	// Check the first marker bit of the first byte
	if (parse_data->Read1() != 0b1)
	{
		logte("Invalid marker bit of the first byte");
		return false;
	}

	ts |= parse_data->ReadByte() << 22;
	ts |= parse_data->Read(7) << 15;

	// Check the second marker bit of the third byte
	if (parse_data->Read1() != 0b1)
	{
		logte("Invalid marker bit of the third byte");
		return false;
	}

	ts |= parse_data->ReadByte() << 7;
	ts |= parse_data->Read(7);

	// Check the third marker bit of the fifth byte
	if (parse_data->Read1() != 0b1)
	{
		logte("Invalid marker bit of the third byte");
		return false;
	}

	*timestamp = ts;

	return true;
}

bool MpegTsChunkStream::ProcessVideoMessage()
{
	if (_video_data->GetFrameType() == MpegTsFrameType::VideoIFrame)
	{
		_media_info->is_video_available = true;

		_media_info->video_width = _video_data->GetWidth();
		_media_info->video_height = _video_data->GetHeight();
		_media_info->video_framerate = _video_data->GetFramerateAsFloat();
	}

	bool result = false;

	if (_media_info->is_video_available)
	{
		bool result = _stream_interface->OnChunkStreamVideoData(_stream_info, this->GetSharedPtr(), _video_data->GetPts(), _video_data->GetDts(), _video_data->GetFrameType(), _video_data->GetData());
	}

	_video_data->Clear();

	return result;
}

bool MpegTsChunkStream::ProcessAudioMessage()
{
	_media_info->is_audio_available = true;

	auto data = _audio_data->GetData();
	auto length = data->GetLength();
	int adts_start = 0;

	while (adts_start < length)
	{
		if (data->At(adts_start) != 0xFF)
		{
			// Invalid audio data
			logte("Invalid audio data: %d", data->At(adts_start));
			_audio_data->Clear();
			return false;
		}

		uint32_t frame_length = ((data->At(adts_start + 3) & 0x03) << 11 | (data->At(adts_start + 4) & 0xFF) << 3 | data->At(adts_start + 5) >> 5);

		if (frame_length > length)
		{
			// TODO(dimiden): Rewrite these code
			return false;
		}
		else
		{
			auto buffer = data->Subdata(adts_start, frame_length);

			if (_stream_interface->OnChunkStreamAudioData(_stream_info, this->GetSharedPtr(), _audio_data->GetPts(), _audio_data->GetDts(), _audio_data->GetFrameType(), buffer) == false)
			{
				_audio_data->Clear();
				return false;
			}

			adts_start += frame_length;
		}
	}

	_audio_data->Clear();

	return true;
}
