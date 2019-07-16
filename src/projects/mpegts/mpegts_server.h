//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <map>
#include "mpegts_chunk_stream.h"
#include <base/ovsocket/ovsocket.h>
#include <physical_port/physical_port_manager.h>
#include "mpegts_observer.h"

//====================================================================================================
// MpegTsServer
//====================================================================================================
class MpegTsServer : protected PhysicalPortObserver, public IMpegTsChunkStream
{
public:
    MpegTsServer() = default;

    virtual ~MpegTsServer();

public:
    bool Start(const ov::SocketAddress &address);

    bool Stop();

    bool AddObserver(const std::shared_ptr<MpegTsObserver> &observer);

    bool RemoveObserver(const std::shared_ptr<MpegTsObserver> &observer);

    bool Disconnect(const ov::String &app_name, uint32_t stream_id);

protected:
    //--------------------------------------------------------------------
    // Implementation of PhysicalPortObserver
    //--------------------------------------------------------------------

    void OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
                        const ov::SocketAddress &address,
                        const std::shared_ptr<const ov::Data> &data) override;

    void OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
                        PhysicalPortDisconnectReason reason,
                        const std::shared_ptr<const ov::Error> &error) override;

    //--------------------------------------------------------------------
    // Implementation of IMpegTsChunkStream
    //--------------------------------------------------------------------
    bool OnChunkStreamReadyComplete(ov::String &client_ipaddr,
                                    ov::String &app_name,
                                    ov::String &stream_name,
                                    std::shared_ptr<MpegTsMediaInfo> &media_info,
                                    info::application_id_t &application_id,
                                    uint32_t &stream_id) override;

    bool OnChunkStreamVideoData(ov::ClientSocket *remote,
                                info::application_id_t application_id,
                                uint32_t stream_id,
                                uint32_t timestamp,
                                MpegTsFrameType frame_type,
                                std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool OnChunkStreamAudioData(ov::ClientSocket *remote,
                                info::application_id_t application_id,
                                uint32_t stream_id,
                                uint32_t timestamp,
                                MpegTsFrameType frame_type,
                                std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool OnDeleteStream(ov::ClientSocket *remote,
                        ov::String &app_name,
                        ov::String &stream_name,
                        info::application_id_t application_id,
                        uint32_t stream_id) override;

//    void OnGarbageCheck();

private :
    std::shared_ptr<PhysicalPort> _physical_port;
    std::map<std::string, std::shared_ptr<MpegTsChunkStream>> _chunk_stream_list;
    std::vector<std::shared_ptr<MpegTsObserver>> _observers;

//    ov::DelayQueue _garbage_check_timer;
    std::recursive_mutex _chunk_stream_list_mutex;

};
