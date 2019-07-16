//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#define _CONSOLE

#include <base/application/application.h>

#include "mpegts_elementary_stream.h"

//====================================================================================================
// Interface
//====================================================================================================
class IMpegTsChunkStream
{
public:
    virtual bool OnChunkStreamReadyComplete(ov::String &client_ipaddr,
                                            ov::String &app_name, ov::String &stream_name,
                                            std::shared_ptr<MpegTsMediaInfo> &media_info,
                                            info::application_id_t &applicaiton_id,
                                            uint32_t &stream_id) = 0;

    virtual bool OnChunkStreamVideoData(ov::ClientSocket *remote,
                                        info::application_id_t applicaiton_id,
                                        uint32_t stream_id,
                                        uint32_t timestamp,
                                        MpegTsFrameType frame_type,
                                        std::shared_ptr<std::vector<uint8_t>> &data) = 0;

    virtual bool OnChunkStreamAudioData(ov::ClientSocket *remote,
                                        info::application_id_t applicaiton_id,
                                        uint32_t stream_id,
                                        uint32_t timestamp,
                                        MpegTsFrameType frame_type,
                                        std::shared_ptr<std::vector<uint8_t>> &data) = 0;

    virtual bool OnDeleteStream(ov::ClientSocket *remote,
                                ov::String &app_name,
                                ov::String &stream_name,
                                info::application_id_t applicaiton_id,
                                uint32_t stream_id) = 0;
};

//====================================================================================================
// MpegTsChunkStream
//====================================================================================================
class MpegTsChunkStream : public ov::EnableSharedFromThis<MpegTsChunkStream>
{
public:
    MpegTsChunkStream(ov::String client_ipaddr, IMpegTsChunkStream *stream_interface);

    ~MpegTsChunkStream() override = default;

    int32_t OnDataReceived(const std::unique_ptr<std::vector<uint8_t>> &data);


protected:

    IMpegTsChunkStream* _stream_interface;

    info::application_id_t _app_id;
    ov::String _app_name;
    uint32_t _stream_id;
    ov::String _stream_name;
    std::shared_ptr<MpegTsMediaInfo> _media_info;
    ov::String _client_ipaddr;

    std::shared_ptr<MpegTsElementaryStream> _video_data;
    std::shared_ptr<MpegTsElementaryStream> _audio_data;

    std::shared_ptr<TSPacket> _packet;
    time_t _last_packet_time;

    int32_t ReceiveChunkPacket(const std::shared_ptr<const std::vector<uint8_t>> &data);

    bool ReceiveChunkMessage(const std::shared_ptr<const std::vector<uint8_t>> &chunk_message);

    bool ReceiveAudioMessage();

    bool ReceiveVideoMessage();

    bool StreamCreate();

    void CheckStreamAlive();
};