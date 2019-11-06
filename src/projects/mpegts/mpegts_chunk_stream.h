//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_elementary_stream.h"

#include <base/application/application.h>

class IMpegTsChunkStream
{
public:
	virtual bool OnChunkStreamReady(ov::String &client_ipaddr,
									ov::String &app_name, ov::String &stream_name,
									std::shared_ptr<MpegTsMediaInfo> &media_info,
									info::application_id_t &application_id, uint32_t &stream_id) = 0;

	virtual bool OnChunkStreamVideoData(ov::ClientSocket *remote,
										info::application_id_t application_id, uint32_t stream_id,
										uint32_t timestamp,
										MpegTsFrameType frame_type,
										const std::shared_ptr<const ov::Data> &data) = 0;

	virtual bool OnChunkStreamAudioData(ov::ClientSocket *remote,
										info::application_id_t application_id, uint32_t stream_id,
										uint32_t timestamp,
										MpegTsFrameType frame_type,
										const std::shared_ptr<const ov::Data> &data) = 0;

	virtual bool OnDeleteStream(ov::ClientSocket *remote,
								ov::String &app_name, ov::String &stream_name,
								info::application_id_t application_id, uint32_t stream_id) = 0;
};

class MpegTsChunkStream : public ov::EnableSharedFromThis<MpegTsChunkStream>
{
public:
	MpegTsChunkStream(ov::String client_ipaddr, IMpegTsChunkStream *stream_interface);
	~MpegTsChunkStream() override = default;

	int32_t OnDataReceived(const std::shared_ptr<const ov::Data> &data);

protected:
	int32_t ReceiveChunkPacket(const std::shared_ptr<const ov::Data> &data);
	bool ReceiveChunkMessage(const std::shared_ptr<const ov::Data> &chunk_message);
	bool ReceiveAudioMessage();
	bool ReceiveVideoMessage();
	bool StreamCreate();
	void CheckStreamAlive();

	ov::String _client_ipaddr;
	IMpegTsChunkStream *_stream_interface = nullptr;

	info::application_id_t _app_id = info::application_id_t();
	uint32_t _stream_id = 0;

	ov::String _app_name = "app";
	ov::String _stream_name = "stream";

	std::shared_ptr<MpegTsMediaInfo> _media_info = std::make_shared<MpegTsMediaInfo>();

	std::shared_ptr<MpegTsElementaryStream> _video_data = std::make_shared<MpegTsElementaryStream>(common::MediaType::Video);
	std::shared_ptr<MpegTsElementaryStream> _audio_data = std::make_shared<MpegTsElementaryStream>(common::MediaType::Audio);

	std::shared_ptr<TSPacket> _packet = nullptr;
	time_t _last_packet_time = 0ULL;
};