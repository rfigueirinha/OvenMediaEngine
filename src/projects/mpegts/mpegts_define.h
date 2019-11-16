//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <string.h>
#include <map>
#include <memory>
#include <vector>

#define bytes_to_u16(MSB, LSB) (((unsigned int)((unsigned char)MSB)) & 255) << 8 | (((unsigned char)LSB) & 255)

#define MPEGTS_MAX_PACKET_SIZE 188
#define MPEGTS_HEADER_SIZE 4

// stream_id				Note	stream coding
// 1011 1100				1		program_stream_map
#define MPEGTS_STREAM_ID_PROGRAM_STREAM_MAP 0b10111100
// 1011 1101				2		private_stream_1
// 1011 1110				 		padding_stream
#define MPEGTS_STREAM_ID_PADDING_STREAM 0b10111110
// 1011 1111				3		private_stream_2
#define MPEGTS_STREAM_ID_PRIVATE_STREAM_2 0b10111111
// 110x xxxx				 		ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3 audio stream number x xxxx
// 1110 xxxx				 		ITU-T Rec. H.262 | ISO/IEC 13818-2 or ISO/IEC 11172-2 or ISO/IEC 14496-2 video stream number xxxx
// 1111 0000				3		ECM_stream
#define MPEGTS_STREAM_ID_ECM_STREAM 0b11110000
// 1111 0001				3		EMM_stream
#define MPEGTS_STREAM_ID_EMM_STREAM 0b11110001
// 1111 0010				5		ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A or ISO/IEC 13818-6_DSMCC_stream
#define MPEGTS_STREAM_ID_DSMCC 0b11110010
// 1111 0011				2		ISO/IEC_13522_stream
// 1111 0100				6		ITU-T Rec. H.222.1 type A
// 1111 0101				6		ITU-T Rec. H.222.1 type B
// 1111 0110				6		ITU-T Rec. H.222.1 type C
// 1111 0111				6		ITU-T Rec. H.222.1 type D
// 1111 1000				6		ITU-T Rec. H.222.1 type E
#define MPEGTS_STREAM_ID_H_222_1_TYPE_E 0b11111000
// 1111 1001				7		ancillary_stream
// 1111 1010				 		ISO/IEC14496-1_SL-packetized_stream
// 1111 1011				 		ISO/IEC14496-1_FlexMux_stream
// 1111 1100 ... 1111 1110			reserved data stream
// 1111 1111				4		program_stream_directory
#define MPEGTS_STREAM_ID_PROGRAM_STREAM_DIRECTORY 0b11111111

#define MPEGTS_STREAM_ID_VIDEO 0xE0
#define MPEGTS_STREAM_ID_AUDIO 0xC0
#define MPEGTS_SYNC_BYTE 0x47

#define PES_HEADER_SIZE 9
#define PES_START_SIZE 6

#define MPEGTS_ADTS_HEADER_SIZE 7
#define MPEGTS_ALIVE_TIME 1

enum class MpegTsFrameType : int32_t
{
	VideoIFrame = 'I',  // VIDEO I Frame
	VideoPFrame = 'P',  // VIDEO P Frame
	AudioFrame = 'A',   // AUDIO Frame
};

enum class MpegTsAspectRatio : uint8_t
{
	Unknown = 0,
	// 1: square samples
	SquareSamples = 1,
	// 2: 4:3 display aspect ratio
	DAR_4_3 = 2,
	// 3: 16:9 display aspect ratio
	DAR_16_9 = 3,
	Unknown0 = 4,
	Unknown1 = 5,
	Unknown2 = 6,
	Unknown3 = 7,
	Unknown4 = 8,
	Unknown5 = 9,
	Unknown6 = 10,
	Unknown7 = 11,
	Unknown8 = 12,
	Unknown9 = 13,
	Unknown10 = 14,
	Unknown11 = 15
};

enum class MpegTsFrameRate : uint8_t
{
	// Forbidden
	Unknown = 0,
	// 1: 23.976 Hz
	Rate23_976 = 1,
	// 2: 24 Hz
	Rate24 = 2,
	// 3: 25 Hz
	Rate25 = 3,
	// 4: 29.97 Hz
	Rate29_97 = 4,
	// 5: 30 Hz
	Rate30 = 5,
	// 6: 50 Hz
	Rate50 = 6,
	// 7: 59.94 Hz
	Rate59_94 = 7,
	// 8: 60 Hz
	Rate60 = 8,
	Unknown0 = 9,
	Unknown1 = 10,
	Unknown2 = 11,
	Unknown3 = 12,
	Unknown4 = 13,
	Unknown5 = 14,
	Unknown6 = 15
};