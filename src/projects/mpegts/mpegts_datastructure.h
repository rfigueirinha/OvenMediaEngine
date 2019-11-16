//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <libavutil/intreadwrite.h>

#include <base/application/application.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

#define MPEGTS_PID_NULL_PACKET 0xFFFF
// Program Association Table
#define MPEGTS_PID_PAT 0x0000
// Conditional Access Table
#define MPEGTS_PID_CAT 0x0001
// Transport Stream Description Table
#define MPEGTS_PID_TSDT 0x0002
// Program Map Table
#define MPEGTS_PID_PMT 0x1000
// Service Description Table
#define MPEGTS_PID_SDT 0x0011

enum TimeStampType
{
	PTS,
	DTS,
};

class MpegTsProvider;

struct MpegTsStreamInfo
{
protected:
	// This ensures that MpegTsStreamInfo is always created by MpegTsProvider only
	friend class MpegTsProvider;
	struct PrivateToken
	{
	};

public:
	MpegTsStreamInfo(const PrivateToken &token, ov::SocketAddress address, ov::String app_name, ov::String stream_name, info::application_id_t app_id, uint32_t stream_id)
		: address(std::move(address)),

		  app_name(std::move(app_name)),
		  stream_name(std::move(stream_name)),

		  app_id(app_id),
		  stream_id(stream_id)
	{
	}

	ov::SocketAddress address;

	ov::String app_name;
	ov::String stream_name;

	info::application_id_t app_id = info::application_id_t();
	uint32_t stream_id = 0;

	ov::String ToString() const
	{
		return ov::String::FormatString(
			"<MpegTsStreamInfo: %p, [%s/%s] (%u/%u), %s>",
			this, app_name.CStr(), stream_name.CStr(), app_id, stream_id, address.ToString().CStr());
	}
};

struct MpegTsMediaInfo
{
	volatile bool is_video_available = false;
	volatile bool is_audio_available = false;
	volatile bool is_streaming = false;

	std::mutex create_mutex;

	// Video informations
	int video_width = 0;
	int video_height = 0;
	float video_framerate = 0;
	int video_bitrate = 0;

	// Audio informations
	int audio_channels = 0;
	int audio_bits = 0;
	int audio_samplerate = 0;
	int audio_sampleindex = 0;
	int audio_bitrate = 0;
};

// Structures of MPEG-TS specification
struct MpegTsDescriptor
{
	virtual ov::String ToString() const
	{
		return "";
	}
};

struct MpegTsServiceDescriptor : public MpegTsDescriptor
{
	uint8_t descriptor_tag = 0U;				// 8 bits
	uint8_t descriptor_length = 0U;				// 8 bits
	uint8_t service_type = 0U;					// 8 bits
	uint8_t service_provider_name_length = 0U;  // 8 bits
	ov::String service_provider_name;			// N bits
	uint8_t service_name_length = 0U;			// 8 bits
	ov::String service_name;					// N bits

	// descriptor length
	uint8_t length = 0U;

	ov::String ToString() const override
	{
		ov::String description = std::move(MpegTsDescriptor::ToString());

		description.AppendFormat("                            descriptor_tag: %u (0x%02x)\n", descriptor_tag, descriptor_tag);
		description.AppendFormat("                            descriptor_length: %u\n", descriptor_length);
		description.AppendFormat("                            service_type: %u\n", service_type);
		description.AppendFormat("                            service_provider_name_length: %u\n", service_provider_name_length);
		description.AppendFormat("                            service_provider_name: %s\n", service_provider_name.CStr());
		description.AppendFormat("                            service_name_length: %u\n", service_name_length);
		description.AppendFormat("                            service_name: %s\n", service_name.CStr());

		return std::move(description);
	}
};

struct MpegTsService
{
	uint16_t service_id = 0U;				 // 16 bits
	uint8_t reserved_future_use = 0U;		 // 6 bits
	bool eit_schedule_flag = false;			 // 1 bit
	bool eit_present_follwing_flag = false;  // 1 bit
	uint8_t running_status = 0U;			 // 3 bits
	bool free_ca_mode = false;				 // 1 bit
	uint16_t descriptors_loop_length = 0U;   // 12 bits

	std::vector<MpegTsServiceDescriptor> descriptors;

	ov::String ToString() const
	{
		ov::String description;

		description.AppendFormat("                    service_id: %u\n", service_id);
		description.AppendFormat("                    reserved_future_use: %u\n", reserved_future_use);
		description.AppendFormat("                    eit_schedule_flag: %s\n", eit_schedule_flag ? "true" : "false");
		description.AppendFormat("                    eit_present_follwing_flag: %s\n", eit_present_follwing_flag ? "true" : "false");
		description.AppendFormat("                    running_status: %u\n", running_status);
		description.AppendFormat("                    free_ca_mode: %s\n", free_ca_mode ? "true" : "false");
		description.AppendFormat("                    descriptors_loop_length: %u\n", descriptors_loop_length);

		description.AppendFormat("                    Descriptors (%zu)\n", descriptors.size());
		int index = 0;
		for (auto &descriptor : descriptors)
		{
			description.AppendFormat("                        Descriptor #%d\n", index);
			description.Append(std::move(descriptor.ToString()));

			index++;
		}

		return std::move(description);
	}
};

struct MpegTsTableCommon
{
	uint8_t table_id = 0U;					// 8 bits
	uint8_t section_syntax_indicator = 0U;  // 1 bit

	union
	{
		// This field should be 0 when PID == PAT/PMT/CAT
		uint8_t zero;  // 1 bit
		// This field will be used when PID == SDT
		uint8_t reserved_future_use;  // 1 bit
	};

	uint8_t reserved0 = 0U;		   // 2 bits
	uint16_t section_length = 0U;  // 12 bits

	virtual ov::String ToString() const
	{
		ov::String description;

		description.AppendFormat("            table_id: %u\n", table_id);
		description.AppendFormat("            section_syntax_indicator: %s\n", section_syntax_indicator ? "true" : "false");
		description.AppendFormat("            zero: %u\n", zero);
		description.AppendFormat("            reserved: %u\n", reserved0);
		description.AppendFormat("            section_length: %u\n", section_length);

		return std::move(description);
	}
};

struct MpegTsProgram
{
	uint16_t program_number = 0U;  // 16 bits
	uint8_t reserved = 0U;		   // 3 bits

	union
	{
		// Variables that make it easier to access other PID variables, such as network_pid, program_map_pid
		uint16_t pid;

		// Only available when program_number == 0
		uint16_t network_pid;  // 13 bits
		// Only available when program_number != 0
		uint16_t program_map_pid;  // 13 bits
	};

	virtual ov::String ToString() const
	{
		ov::String description;

		description.AppendFormat("                    program_number: %u\n", program_number);
		description.AppendFormat("                    reserved: %u\n", reserved);

		if (program_number == 0)
		{
			description.AppendFormat("                    network_pid: %u\n", network_pid);
		}
		else
		{
			description.AppendFormat("                    program_map_pid: %u (0x%04x)\n", program_map_pid, program_map_pid);
		}

		return std::move(description);
	}
};

struct MpegTsPAT : public MpegTsTableCommon
{
	uint16_t transport_stream_id = 0U;	// 16 bits
	uint8_t reserved1 = 0U;				  // 2 bits
	uint8_t version_number = 0U;		  // 5 bits
	uint8_t current_next_indicator = 0U;  // 1 bit
	uint8_t section_number = 0U;		  // 8 bits
	uint8_t last_section_number = 0U;	 // 8 bits
	std::vector<MpegTsProgram> programs;
	uint32_t crc_32 = 0U;  // 32bits

	ov::String ToString() const override
	{
		auto description = std::move(MpegTsTableCommon::ToString());

		description.AppendFormat("            transport_stream_id: %u\n", transport_stream_id);
		description.AppendFormat("            reserved: %u\n", reserved1);
		description.AppendFormat("            version_number: %u\n", version_number);
		description.AppendFormat("            current_next_indicator: %u\n", current_next_indicator);
		description.AppendFormat("            section_number: %u\n", section_number);
		description.AppendFormat("            last_section_number: %u\n", last_section_number);
		description.AppendFormat("            Programs (%zu)\n", programs.size());
		int index = 0;
		for (auto &program : programs)
		{
			description.AppendFormat("                Program #%d\n", index);
			description.Append(std::move(program.ToString()));

			index++;
		}

		description.AppendFormat("            crc_32: 0x%08X\n", crc_32);

		return std::move(description);
	}
};

struct MpegTsPMT : public MpegTsTableCommon
{
	uint16_t program_number = 0U;		  // 16 bits
	uint8_t reserved1 = 0U;				  // 2 bits
	uint8_t version_number = 0U;		  // 5 bits
	bool current_next_indicator = false;  // 1 bit
	uint8_t section_number = 0U;		  // 8 bits
	uint8_t last_section_number = 0U;	 // 8 bits
	uint8_t reserved2 = 0U;				  // 3 bits
	uint16_t pcr_pid = 0U;				  // 13 bits
	uint8_t reserved3 = 0U;				  // 4 bits
	uint16_t program_info_length = 0U;	// 12 bits
	std::vector<MpegTsDescriptor> descriptors;
	// std::vector<MpegTsStream> streams;
	uint32_t crc_32 = 0U;  // 32bits

	ov::String ToString() const override
	{
		auto description = std::move(MpegTsTableCommon::ToString());

		description.AppendFormat("            program_number: %u\n", program_number);
		description.AppendFormat("            reserved: %u\n", reserved0);
		description.AppendFormat("            version_number: %u\n", version_number);
		description.AppendFormat("            current_next_indicator: %s\n", current_next_indicator ? "true" : "false");
		description.AppendFormat("            section_number: %u\n", section_number);
		description.AppendFormat("            last_section_number: %u\n", last_section_number);
		description.AppendFormat("            reserved: %u\n", reserved1);
		description.AppendFormat("            pcr_pid: %u\n", pcr_pid);
		description.AppendFormat("            reserved: %u\n", reserved2);
		description.AppendFormat("            program_info_length: %u\n", program_info_length);

		description.AppendFormat("            Descriptors (%zu)\n", descriptors.size());
		int index = 0;
		for (auto &descriptor : descriptors)
		{
			description.AppendFormat("            Descriptor #%d\n", index);
			description.Append(std::move(descriptor.ToString()));

			index++;
		}

		// description.AppendFormat("            Streams (%zu)\n", streams.size());
		// index = 0;
		// for (auto &stream : streams)
		// {
		// 	description.AppendFormat("            Stream #%d\n", index);
		// 	description.Append(std::move(stream.ToString()));

		// 	index++;
		// }

		description.AppendFormat("            crc_32: 0x%08X\n", crc_32);

		return std::move(description);
	}
};

struct MpegTsSDT : public MpegTsTableCommon
{
	uint16_t transport_stream_id = 0U;	// 16 bits
	uint8_t reserved1 = 0U;				  // 2 bits
	uint8_t version_number = 0U;		  // 5 bits
	uint8_t current_next_indicator = 0U;  // 1 bit
	uint8_t section_number = 0U;		  // 8 bits
	uint8_t last_section_number = 0U;	 // 8 bits
	uint16_t original_network_id;		  // 16 bits
	uint8_t reserved_future_use;		  // 8 bits
	std::vector<MpegTsService> services;
	uint32_t crc_32 = 0U;  // 32bits

	ov::String ToString() const override
	{
		auto description = std::move(MpegTsTableCommon::ToString());

		description.AppendFormat("            transport_stream_id: %u\n", transport_stream_id);
		description.AppendFormat("            reserved: %u\n", reserved1);
		description.AppendFormat("            version_number: %u\n", version_number);
		description.AppendFormat("            current_next_indicator: %u\n", current_next_indicator);
		description.AppendFormat("            section_number: %u\n", section_number);
		description.AppendFormat("            last_section_number: %u\n", last_section_number);
		description.AppendFormat("            original_network_id: %u\n", original_network_id);
		description.AppendFormat("            reserved_future_use: %u (0x%02x)\n", reserved_future_use, reserved_future_use);

		description.AppendFormat("            Services (%zu)\n", services.size());
		int index = 0;
		for (auto &service : services)
		{
			description.AppendFormat("                Service #%d\n", index);
			description.Append(std::move(service.ToString()));

			index++;
		}

		description.AppendFormat("            crc_32: 0x%08X\n", crc_32);

		return std::move(description);
	}
};

//
// TS (Transport Stream) header structure (ISO13818-1)
//
// sync byte: 8 bits
// transport error indicator: 1 bit
// payload unit start indicator: 1 bit
// transport priority: 1 bit
// PID (packet identifier): 13 bits
// transport scrambling control: 2 bits
// adaptation field control: 2 bits
// continuity counter: 4 bits
// adaptation field: (variable)
//     adaptation field length: 8 bits
//     discontinuity indicator: 1 bit
//     random access indicator: 1 bit
//     elementary stream priority indicator: 1 bit
//     5 flags (*): 5 bits
//     optional fields (*)
//         PCR: 42 bits (actually, it's 48 (33 + 6 + 9) bytes)
//         OPCR: 42 bits (actually, it's 48 (33 + 6 + 9) bytes)
//         splice countdown: 8 bits
//         transport private data length: 8 bits
//         transport private data: (variable)
//         adaptation field extension length: 8 bits
//         3 flags: 3 bits (**)
//         optional fields (**)
//             ltw_valid flag: 1 bit
//             ltw offset: 15 bits
//             -: 2 bits
//             piecewise rate: 22 bits
//             splice type: 4 bits
//             DTS_next_au: 33 bits
// ...
//
// (Total: 188 byte)
struct MpegTsPacket
{
	uint8_t sync_byte = 0U;						// 8 bits
	bool transport_error_indicator = false;		// 1 bit
	bool payload_unit_start_indicator = false;  // 1 bit
	uint8_t transport_priority = 0U;			// 1 bit
	// Value				Description
	// 0x0000				Program Association Table
	// 0x0001				Conditional Access Table
	// 0x0002				Transport Stream Description Table
	// 0x0003-0x000F		Reserved
	// 0x00010...0x1FFE		May be assigned as network_PID, Program_map_PID, elementary_PID, or for other purposes
	// 0x1FFF				Null packet
	//
	// NOTE - The transport packets with PID values 0x0000, 0x0001, and 0x0010-0x1FFE are allowed to carry a PCR.
	uint16_t packet_identifier = 0U;  // 13 bits
	// 00	Not scrambled
	// 01	User-defined
	// 10	User-defined
	// 11	User-defined
	uint8_t transport_scrambling_control = 0U;  // 2 bits
	// 00	Reserved for future use by ISO/IEC
	// 01	No adaptation_field, payload only
	// 10	Adaptation_field only, no payload
	// 11	Adaptation_field followed by payload
	uint8_t adaptation_field_control = 0U;  // 2 bits
	uint8_t continuity_counter = 0U;		// 4 bits

	struct
	{
		// adaptation field length
		uint8_t length = 0U;

		// indicators
		bool discontinuity_indicator = false;
		bool random_access_indicator = false;
		bool elementary_stream_priority_indicator = false;

		// 5 flags
		bool pcr_flag = false;
		bool opcr_flag = false;
		bool splicing_point_flag = false;
		bool transport_private_data_flag = false;
		bool adaptation_field_extension_flag = false;

		// Program Clock Reference
		struct
		{
			uint64_t base = 0U;  // 33 bits
			// uint8_t reserved = 0U;	// 6 bits
			uint16_t extension = 0U;  // 9 bits
		} pcr;

		// Original Program Clock Reference
		struct
		{
			uint64_t base = 0U;  // 33 bits
			// uint8_t reserved = 0U;	// 6 bits
			uint16_t extension = 0U;  // 9 bits
		} opcr;

		// Splice countdown
		struct
		{
			uint8_t splice_countdown = 0U;  // 8 bits
		} splice;

		// Transport private data
		struct
		{
			uint8_t length = 0U;  // 8 bits
		} transport_private_data;

		// Adaptation field extension
		struct
		{
			uint8_t length = 0U;  // 8 bits
			bool ltw_flag = false;
			bool piecewise_rate_flag = false;
			bool seamless_splice_flag = false;
			// uint8_t reserved = 0U;  // 5 bits

			struct
			{
				bool valid_flag = false;
				uint16_t offset = 0U;  // 15 bits
			} ltw;

			struct
			{
				// uint8_t reserved = 0U;  // 2 bits
				uint32_t piecewise_rate;  // 22 bits
			} piecewise_rate;

			struct
			{
				uint8_t splice_type = 0U;  // 4 bits
				// DTS_next_AU[32..30]
				uint8_t dts_next_au0 = 0U;  // 3 bits
				uint8_t marker_bit0 = 0U;   // 1 bit
				// DTS_next_AU[29..15]
				uint8_t dts_next_au1 = 0U;  // 15 bits
				uint8_t marker_bit1 = 0U;   // 1 bit
				// DTS_next_AU[14..0]
				uint8_t dts_next_au2 = 0U;  // 15 bits
				uint8_t marker_bit2 = 0U;   // 1 bit
			} seamless_splice;
		} extension;
	} adaptation_field;

	struct
	{
		uint8_t pointer_field = 0U;  // 8 bits

		// Only available when PID == PAT
		MpegTsPAT pat;
		// Only available when PID == SDT
		MpegTsSDT sdt;
		// Only available when PID == PMT
		MpegTsPMT pmt;
	} psi;

	struct PesPacket
	{
		uint8_t stream_id = 0U;					 // 8 bits
		uint16_t length = 0U;					 // 16 bits
		uint8_t one_zero = 0U;					 // 2 bits
		uint8_t pes_scrambling_control = 0U;	 // 2 bits
		uint8_t pes_priority = 0U;				 // 1 bit
		uint8_t data_alignment_indicator = 0U;   // 1 bit
		uint8_t copyright = 0U;					 // 1 bit
		uint8_t original_or_copy = 0U;			 // 1 bit
		uint8_t pts_dts_flags = 0U;				 // 2 bits
		uint8_t escr_flag = 0U;					 // 1 bit
		uint8_t es_rate_flag = 0U;				 // 1 bit
		uint8_t dsm_trick_mode_flag = 0U;		 // 1 bit
		uint8_t additional_copy_info_flag = 0U;  // 1 bit
		uint8_t pes_crc_flag = 0U;				 // 1 bit
		uint8_t pes_extension_flag = 0U;		 // 1 bit
		uint8_t pes_header_data_length = 0U;	 // 8 bits

		int64_t pts = -1LL;
		int64_t dts = 1LL;

		inline bool IsAudioStream() const
		{
			// Audio stream: 0b110xxxxx
			return (stream_id & 0b11100000) == 0b11000000;
		}

		inline bool IsVideoStream() const
		{
			// Video stream: 0b1110xxxx
			return (stream_id & 0b11110000) == 0b11100000;
		}
	} pes;

	// TODO(dimiden): Extract this method to MPEG-TS utility class
	bool HasPSI() const
	{
		switch (packet_identifier)
		{
			case MPEGTS_PID_PAT:
			case MPEGTS_PID_CAT:
			case MPEGTS_PID_TSDT:
			case MPEGTS_PID_PMT:
			case MPEGTS_PID_SDT:
				return true;
		}
		return false;
	}

	// TODO(dimiden): Extract this method to MPEG-TS utility class
	ov::String GetSectionName() const
	{
		switch (packet_identifier)
		{
			case MPEGTS_PID_PAT:
				return "PAT";

			case MPEGTS_PID_PMT:
				return "PMT";

			case MPEGTS_PID_SDT:
				return "SDT";
		}

		return "?";
	}

	ov::String ToString() const
	{
		ov::String description = ov::String::FormatString("<TsPacket: %p,\n");

		description.AppendFormat("    sync_byte: %02x\n", sync_byte);
		description.AppendFormat("    transport_error_indicator: %s\n", transport_error_indicator ? "true" : "false");
		description.AppendFormat("    payload_unit_start_indicator: %s\n", payload_unit_start_indicator ? "true" : "false");
		description.AppendFormat("    transport_priority: %u\n", transport_priority);
		description.AppendFormat("    packet_identifier: %u (0x%04x)\n", packet_identifier, packet_identifier);
		description.AppendFormat("    transport_scrambling_control: %u (0x%02x)\n", transport_scrambling_control, transport_scrambling_control);
		description.AppendFormat("    adaptation_field_control: %u (0x%02x)\n", adaptation_field_control, adaptation_field_control);
		description.AppendFormat("    continuity_counter: %u\n", continuity_counter);

		bool has_adaptation = OV_GET_BIT(adaptation_field_control, 1);
		bool has_payload = OV_GET_BIT(adaptation_field_control, 0);

		if (has_adaptation)
		{
			description.AppendFormat("    Adaptation fields: (%u bytes)\n", adaptation_field.length);
			description.AppendFormat("        discontinuity_indicator: %s\n", adaptation_field.discontinuity_indicator ? "true" : "false");
			description.AppendFormat("        random_access_indicator: %s\n", adaptation_field.random_access_indicator ? "true" : "false");
			description.AppendFormat("        elementary_stream_priority_indicator: %s\n", adaptation_field.elementary_stream_priority_indicator ? "true" : "false");

			description.AppendFormat("        pcr_flag: %s\n", adaptation_field.pcr_flag ? "true" : "false");
			description.AppendFormat("        opcr_flag: %s\n", adaptation_field.opcr_flag ? "true" : "false");
			description.AppendFormat("        splicing_point_flag: %s\n", adaptation_field.splicing_point_flag ? "true" : "false");
			description.AppendFormat("        transport_private_data_flag: %s\n", adaptation_field.transport_private_data_flag ? "true" : "false");
			description.AppendFormat("        adaptation_field_extension_flag: %s\n", adaptation_field.adaptation_field_extension_flag ? "true" : "false");
		}
		else
		{
			description.AppendFormat("    (No adaptation fields)\n");
		}

		if (HasPSI())
		{
			description.AppendFormat("    Program Specific Information:\n");
			description.AppendFormat("        pointer_field: %u\n", psi.pointer_field);
			description.AppendFormat("        %s\n", GetSectionName().CStr());

			switch (packet_identifier)
			{
				case MPEGTS_PID_PAT:
					description.Append(psi.pat.ToString());
					break;

				case MPEGTS_PID_PMT:
					description.Append(psi.pmt.ToString());
					break;

				case MPEGTS_PID_SDT:
					description.Append(psi.sdt.ToString());
					break;
			}
		}

		description.Append('>');

		return std::move(description);
	}
};