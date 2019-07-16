//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_server.h"
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "MpegTsProvider"

//====================================================================================================
// ~MpegTsServer
//====================================================================================================
MpegTsServer::~MpegTsServer()
{
}

//====================================================================================================
// Start
//====================================================================================================
bool MpegTsServer::Start(const ov::SocketAddress &address)
{
    logtd("MpegTsServer Start");

    if(_physical_port != nullptr)
    {
        logtw("MpegTsServer running fail");
        return false;
    }

    _physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Udp, address);

    if(_physical_port != nullptr)
    {
        _physical_port->AddObserver(this);
    }

    return _physical_port != nullptr;
}

//====================================================================================================
// Stop
//====================================================================================================
bool MpegTsServer::Stop()
{
    if(_physical_port == nullptr)
    {
        return false;
    }

    _physical_port->RemoveObserver(this);
    PhysicalPortManager::Instance()->DeletePort(_physical_port);
    _physical_port = nullptr;

    return true;
}

//====================================================================================================
// AddObserver
// - MpegTsOvserver 등록
//====================================================================================================
bool MpegTsServer::AddObserver(const std::shared_ptr<MpegTsObserver> &observer)
{
    logtd("MpegTsServer AddObserver");
    // 기존에 등록된 observer가 있는지 확인
    for(const auto &item : _observers)
    {
        if(item == observer)
        {
            // 기존에 등록되어 있음
            logtw("%p is already observer of MpegTsServer", observer.get());
            return false;
        }
    }

    _observers.push_back(observer);

    return true;
}


//====================================================================================================
// RemoveObserver
// - MpegTsObserver 제거
//====================================================================================================
bool MpegTsServer::RemoveObserver(const std::shared_ptr<MpegTsObserver> &observer)
{
    auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<MpegTsObserver> const &value) -> bool
    {
        return value == observer;
    });

    if(item == _observers.end())
    {
        // 기존에 등록되어 있지 않음
        logtw("%p is not registered observer", observer.get());
        return false;
    }

    _observers.erase(item);

    return true;
}

//====================================================================================================
// OnDataReceived
// - 데이터 수신
//====================================================================================================
void MpegTsServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
                                  const ov::SocketAddress &address,
                                  const std::shared_ptr<const ov::Data> &data)
{
    ov::String client_ipaddr = address.GetIpAddress().CStr();

    if(_chunk_stream_list.empty())
    {
        _chunk_stream_list[client_ipaddr.CStr()] = std::make_shared<MpegTsChunkStream>(client_ipaddr, this);
    }
    else
    {
        if (_chunk_stream_list.find(client_ipaddr.CStr()) == _chunk_stream_list.end())
        {
            // streaming already
            return;
        }
    }

    std::unique_lock<std::recursive_mutex> lock(_chunk_stream_list_mutex);


    auto item = _chunk_stream_list.find(client_ipaddr.CStr());

    // clinet 세션 확인
    if(item != _chunk_stream_list.end())
    {
        // 데이터 전달
        auto process_data = std::make_unique<std::vector<uint8_t>>((uint8_t *)data->GetData(),
                                                                   (uint8_t *)data->GetData() + data->GetLength());

        if(item->second->OnDataReceived(std::move(process_data)) < 0)
        {
            return;
        }
    }
}


//====================================================================================================
// OnDisconnected
// - client(Encoder) 문제로 접속 종료 이벤트 발생
// - socket 세션은 호출한 ServerSocket에서 스스로 정리
//====================================================================================================
void MpegTsServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
                                  PhysicalPortDisconnectReason reason,
                                  const std::shared_ptr<const ov::Error> &error)
{
}


//====================================================================================================
// OnChunkStreamVideoData
// - IMpegTsChunkStream 구현
// - Video 스트림 전달
//====================================================================================================
bool MpegTsServer::OnChunkStreamVideoData(ov::ClientSocket *remote,
                                          info::application_id_t application_id,
                                          uint32_t stream_id,
                                          uint32_t timestamp,
                                          MpegTsFrameType frame_type,
                                          std::shared_ptr<std::vector<uint8_t>> &data)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnVideoData(application_id, stream_id, timestamp, frame_type, data))
        {
            logte("MpegTs input stream video data fail - id(%u/%u)",
                  application_id,
                  stream_id);
            return false;
        }
    }

    return true;
}


//====================================================================================================
// OnChunkStreamReadyComplete
// - IMpegTsChunkStream 구현
// - 스트림 준비 완료
// - app/stream id 획득 및 저장
//====================================================================================================
bool MpegTsServer::OnChunkStreamReadyComplete(ov::String &client_ipaddr,
                                              ov::String &app_name,
                                              ov::String &stream_name,
                                              std::shared_ptr<MpegTsMediaInfo> &media_info,
                                              info::application_id_t &application_id,
                                              uint32_t &stream_id)
{
    for(auto &observer : _observers)
    {
        if(!observer->OnStreamReadyComplete(app_name, stream_name, media_info, application_id, stream_id))
        {
            logte("MpegTs input stream ready fail - app(%s) stream(%s) remote(%s)",
                  app_name.CStr(),
                  stream_name.CStr(),
                  client_ipaddr.CStr());
            return false;
        }
    }

    return true;
}


//====================================================================================================
// OnChunkStreamAudioData
// - IMpegTsChunkStream 구현
// - Audio 스트림 전달
//====================================================================================================
bool MpegTsServer::OnChunkStreamAudioData(ov::ClientSocket *remote,
                                          info::application_id_t application_id,
                                          uint32_t stream_id,
                                          uint32_t timestamp,
                                          MpegTsFrameType frame_type,
                                          std::shared_ptr<std::vector<uint8_t>> &data)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnAudioData(application_id, stream_id, timestamp, frame_type, data))
        {
            logte("MpegTs input stream audio data fail - id(%u/%u) remote(%s)",
                  application_id,
                  stream_id,
                  remote->ToString().CStr());
            return false;
        }
    }

    return true;
}


//====================================================================================================
// OnDeleteStream
// - IMpegTsChunkStream 구현
// - 스트림만 삭제
// - 세션 연결은 호출한 곳에서 처리되거나 Garbge Check에서 처리
//====================================================================================================
bool MpegTsServer::OnDeleteStream(ov::ClientSocket *remote,
                                  ov::String &app_name,
                                  ov::String &stream_name,
                                  info::application_id_t application_id,
                                  uint32_t stream_id)
{
    // observer들에게 알림
    for(auto &observer : _observers)
    {
        if(!observer->OnDeleteStream(application_id, stream_id))
        {
            logte("MpegTs input stream delete fail - stream(%s/%s) id(%u/%u)",
                  app_name.CStr(),
                  stream_name.CStr(),
                  application_id,
                  stream_id);
            return false;
        }
    }

    logtd("MpegTs input stream delete - stream(%s/%s) id(%u/%u) remote(%s)",
          app_name.CStr(),
          stream_name.CStr(),
          application_id,
          stream_id);

    return true;
}