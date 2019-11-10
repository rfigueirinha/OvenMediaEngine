//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "mpegts_server.h"
#include "mpegts_private.h"

#include <base/ovlibrary/ovlibrary.h>

MpegTsServer::~MpegTsServer()
{
}

bool MpegTsServer::Start(const ov::SocketAddress &address)
{
	if (_physical_port != nullptr)
	{
		logtw("MPEG-TS server is already running");
		return false;
	}

	logti("Trying to start MPEG-TS server on %s...", address.ToString().CStr());

	_physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Udp, address);

	if (_physical_port == nullptr)
	{
		logte("Could not initialize phyiscal port for MPEG-TS server: %s", address.ToString().CStr());

		return false;
	}

	_physical_port->AddObserver(this);

	return true;
}

bool MpegTsServer::Stop()
{
	if (_physical_port == nullptr)
	{
		logtw("MPEG-TS server is not running");
		return false;
	}

	_physical_port->RemoveObserver(this);
	PhysicalPortManager::Instance()->DeletePort(_physical_port);
	_physical_port = nullptr;

	return true;
}

bool MpegTsServer::AddObserver(const std::shared_ptr<MpegTsObserver> &observer)
{
	logtd("Trying to add an observer %p to the MPEG-TS server", observer.get());

	// Verify that the observer is already registered
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<MpegTsObserver> const &value) -> bool {
		return value == observer;
	});

	if (item != _observers.end())
	{
		logtw("The observer %p is already registered with MPEG-TS server", observer.get());
		return false;
	}

	_observers.push_back(observer);

	return true;
}

bool MpegTsServer::RemoveObserver(const std::shared_ptr<MpegTsObserver> &observer)
{
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<MpegTsObserver> const &value) -> bool {
		return value == observer;
	});

	if (item == _observers.end())
	{
		logtw("The observer %p is not registered with MPEG-TS server", observer.get());
		return false;
	}

	_observers.erase(item);

	return true;
}

bool MpegTsServer::Disconnect(const ov::String &app_name, uint32_t stream_id)
{
	// TODO(dimiden): remove the stream in the stream list
	OV_ASSERT2(false);

	return true;
}

void MpegTsServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	std::lock_guard<std::recursive_mutex> lock(_chunk_stream_list_mutex);

	ov::String client_ipaddr = address.GetIpAddress().CStr();

	if (_chunk_stream_list.empty())
	{
		_chunk_stream_list[client_ipaddr.CStr()] = std::make_shared<MpegTsChunkStream>(client_ipaddr, this);
	}
	else
	{
		if (_chunk_stream_list.find(client_ipaddr.CStr()) == _chunk_stream_list.end())
		{
			// Already streaming
			return;
		}
	}

	auto item = _chunk_stream_list.find(client_ipaddr.CStr());

	if (item != _chunk_stream_list.end())
	{
		// auto process_data = std::make_unique<std::vector<uint8_t>>((uint8_t *)data->GetData(),
		// 														   (uint8_t *)data->GetData() + data->GetLength());
		auto process_data = data->Clone();

		if (item->second->OnDataReceived(std::move(process_data)) < 0)
		{
			return;
		}
	}
}

void MpegTsServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
								  PhysicalPortDisconnectReason reason,
								  const std::shared_ptr<const ov::Error> &error)
{
}

bool MpegTsServer::OnChunkStreamReady(ov::String &client_ipaddr,
									  ov::String &app_name, ov::String &stream_name,
									  std::shared_ptr<MpegTsMediaInfo> &media_info,
									  info::application_id_t &application_id, uint32_t &stream_id)
{
	for (auto &observer : _observers)
	{
		if (observer->OnStreamReadyComplete(app_name, stream_name, media_info, application_id, stream_id) == false)
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

bool MpegTsServer::OnChunkStreamVideoData(ov::ClientSocket *remote,
										  info::application_id_t application_id, uint32_t stream_id,
										  uint32_t timestamp,
										  MpegTsFrameType frame_type,
										  const std::shared_ptr<const ov::Data> &data)
{
	for (auto &observer : _observers)
	{
		if (observer->OnVideoData(application_id, stream_id, timestamp, frame_type, data) == false)
		{
			logte("Could not send video data to observer %p: (%u/%u), remote: %s",
				  observer.get(),
				  application_id, stream_id,
				  remote->ToString().CStr());
			return false;
		}
	}

	return true;
}

bool MpegTsServer::OnChunkStreamAudioData(ov::ClientSocket *remote,
										  info::application_id_t application_id, uint32_t stream_id,
										  uint32_t timestamp,
										  MpegTsFrameType frame_type,
										  const std::shared_ptr<const ov::Data> &data)
{
	// Notify the audio data to the observers
	for (auto &observer : _observers)
	{
		if (observer->OnAudioData(application_id, stream_id, timestamp, frame_type, data) == false)
		{
			logte("Could not send audio data to observer %p: (%u/%u)",
				  observer.get(),
				  application_id, stream_id);
			return false;
		}
	}

	return true;
}

bool MpegTsServer::OnDeleteStream(ov::ClientSocket *remote,
								  ov::String &app_name, ov::String &stream_name,
								  info::application_id_t application_id, uint32_t stream_id)
{
	for (auto &observer : _observers)
	{
		if (observer->OnDeleteStream(application_id, stream_id) == false)
		{
			logte("MpegTs input stream delete fail - stream(%s/%s) id(%u/%u)",
				  app_name.CStr(), stream_name.CStr(),
				  application_id, stream_id);
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