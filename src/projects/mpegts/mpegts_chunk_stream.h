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
#include "mpegts_elementary_stream.h"
#include "mpegts_utilities.h"

class MpegTsChunkStream;

class IMpegTsChunkStream
{
public:
	virtual bool OnChunkStreamVideoData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsChunkStream> &chunk_stream,
										int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) = 0;

	virtual bool OnChunkStreamAudioData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsChunkStream> &chunk_stream,
										int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) = 0;
};

class MpegTsChunkStream : public ov::EnableSharedFromThis<MpegTsChunkStream>
{
public:
	MpegTsChunkStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, IMpegTsChunkStream *stream_interface);
	~MpegTsChunkStream() override = default;

	int32_t OnDataReceived(const std::shared_ptr<const ov::Data> &data);

	const std::shared_ptr<const MpegTsStreamInfo> &GetStreamInfo() const
	{
		return _stream_info;
	}

	std::shared_ptr<const MpegTsMediaInfo> GetMediaInfo() const
	{
		return _media_info;
	}

	const std::shared_ptr<MpegTsMediaInfo> &GetMediaInfo()
	{
		return _media_info;
	}

protected:
	int32_t ParseTsHeader(const std::shared_ptr<const ov::Data> &data);

	bool ParseAdaptationHeader(MpegTsPacket *packet, MpegTsParseData *parse_data);
	bool ParsePayload(MpegTsPacket *packet, MpegTsParseData *parse_data);
	bool ParseService(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsService *service);
	bool ParseServiceDescriptor(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsServiceDescriptor *descriptor);
	bool ParseProgram(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsProgram *program);
	bool ParseCommonTableHeader(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsTableCommon *table);
	// Parse Program Specific Information
	bool ParsePsi(MpegTsPacket *packet, MpegTsParseData *parse_data);
	// Parse Program Association Table
	bool ParsePat(MpegTsPacket *packet, MpegTsParseData *parse_data);
	// Parse Program Map Table
	bool ParsePmt(MpegTsPacket *packet, MpegTsParseData *parse_data);
	// Parse Service Description Table
	bool ParseSdt(MpegTsPacket *packet, MpegTsParseData *parse_data);

	bool ParsePes(MpegTsPacket *packet, MpegTsParseData *parse_data);
	bool ParsePesOptionalHeader(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsPacket::PesPacket *pes);
	bool ParsePesPayload(MpegTsPacket *packet, MpegTsParseData *parse_data);
	bool ParsePesPaddingBytes(MpegTsPacket *packet, MpegTsParseData *parse_data, MpegTsPacket::PesPacket *pes);
	bool ParseTimestamp(MpegTsPacket *packet, MpegTsParseData *parse_data, uint8_t start_bits, int64_t *timestamp);

	bool ProcessAudioMessage();
	bool ProcessVideoMessage();
	bool StreamCreate();
	void CheckStreamAlive();

	std::shared_ptr<const MpegTsStreamInfo> _stream_info;

	IMpegTsChunkStream *_stream_interface = nullptr;

	std::shared_ptr<MpegTsMediaInfo> _media_info = std::make_shared<MpegTsMediaInfo>();

	// TODO(dimiden): Need to support multiple audio/video streams
	std::shared_ptr<MpegTsElementaryStream> _video_data = std::make_shared<MpegTsElementaryStream>(common::MediaType::Video);
	std::shared_ptr<MpegTsElementaryStream> _audio_data = std::make_shared<MpegTsElementaryStream>(common::MediaType::Audio);

	int16_t _last_continuity_counter = -1;

	time_t _last_packet_time = 0ULL;
};