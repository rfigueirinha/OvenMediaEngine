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

bool MpegTsServer::ParseAddressList(const ov::String &ip, const cfg::RangedPort &ranged_port, std::map<int, ov::SocketAddress> *address_list)
{
	auto ports = ranged_port.GetRangedPort().Split(",");

	for (auto port : ports)
	{
		port = port.Trim();

		auto range = port.Split("-");
		int start = 0;
		int end = 0;

		switch (range.size())
		{
			case 1:
				// single port number
				start = ov::Converter::ToInt32(port);
				end = start;
				break;

			case 2:
				// port range
				start = ov::Converter::ToInt32(range[0]);
				end = ov::Converter::ToInt32(range[1]);
				break;

			default:
				logte("Invalid port range: %s (%zu tokens found)", port.CStr(), range.size());
				return false;
		}

		if (end < start)
		{
			logte("Invalid port range: %s (end port MUST be greater than start port)", port.CStr());
			return false;
		}

		for (int port_num = start; port_num <= end; port_num++)
		{
			if (address_list->find(port_num) != address_list->end())
			{
				logte("The port number is in conflict: %d", port_num);
				return false;
			}

			address_list->emplace(port_num, std::move(ov::SocketAddress(ip, port_num)));
		}
	}

	return true;
}

std::shared_ptr<PhysicalPort> MpegTsServer::CreatePhysicalPort(const ov::SocketAddress &address)
{
	auto physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Udp, address);

	if (physical_port != nullptr)
	{
		if (physical_port->AddObserver(this))
		{
			return physical_port;
		}

		logte("Cannot add a observer %p to %p", this, physical_port.get());

		PhysicalPortManager::Instance()->DeletePort(physical_port);
	}
	else
	{
		logte("Cannot create physical port for %s/udp", address.ToString().CStr());
	}

	return nullptr;
}

std::shared_ptr<const MpegTsStreamInfo> MpegTsServer::QueryStreamInfo(uint16_t port_num, const ov::SocketAddress *address)
{
	for (auto observer : _observers)
	{
		auto stream_info = observer->OnQueryStreamInfo(port_num, address);

		if (stream_info != nullptr)
		{
			return std::move(stream_info);
		}
	}

	return nullptr;
}

bool MpegTsServer::Start(const std::map<ov::port_t, ov::SocketAddress> &address_list)
{
	std::lock_guard<__decltype(_physical_port_list_mutex)> lock_guard(_physical_port_list_mutex);

	if (_physical_port_list.size() > 0)
	{
		logtw("MPEG-TS server is already running");
		return false;
	}

	for (auto &item : address_list)
	{
		auto &address = item.second;

		auto physical_port = CreatePhysicalPort(address);

		if (physical_port != nullptr)
		{
			auto stream_info = QueryStreamInfo(address.Port(), nullptr);

			if (stream_info != nullptr)
			{
				logti("MPEG-TS Provider is listening on %s for [%s/%s]", address.ToString().CStr(), stream_info->app_name.CStr(), stream_info->stream_name.CStr());
			}
			else
			{
				logti("MPEG-TS Provider is listening on %s", address.ToString().CStr());
			}

			_physical_port_list.push_back(std::move(physical_port));
		}
		else
		{
			logte("Could not create physical port: %s", address.ToString().CStr());
			_physical_port_list.clear();
			return false;
		}
	}

	return true;
}

bool MpegTsServer::Stop()
{
	std::lock_guard<__decltype(_physical_port_list_mutex)> lock_guard(_physical_port_list_mutex);

	if (_physical_port_list.size() == 0)
	{
		logtw("There is no physical port to stop MPEG-TS server");
		return false;
	}

	for (auto &physical_port : _physical_port_list)
	{
		physical_port->RemoveObserver(this);
		PhysicalPortManager::Instance()->DeletePort(physical_port);
	}

	_physical_port_list.clear();

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

bool MpegTsServer::Disconnect(const std::shared_ptr<const MpegTsStreamInfo> &stream_info)
{
	// TODO(dimiden): remove the stream in the stream list
	OV_ASSERT2(false);

	return true;
}

bool MpegTsServer::CreateStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsMediaInfo> &media_info)
{
	for (auto &observer : _observers)
	{
		if (observer->OnStreamReady(stream_info, media_info) == false)
		{
			logte("Could not ready for MPEG-TS input stream: %s", stream_info->ToString().CStr());
			return false;
		}
	}

	return true;
}

bool MpegTsServer::DeleteStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info)
{
	for (auto &observer : _observers)
	{
		if (observer->OnDeleteStream(stream_info) == false)
		{
			logte("Could not delete MPEG-TS input stream: %s", stream_info->ToString().CStr());
			return false;
		}
	}

	logtd("MPEG-TS input stream is deleted: %s", stream_info->ToString().CStr());

	return true;
}

bool MpegTsServer::CreateStreamIfNeeded(const std::shared_ptr<MpegTsChunkStream> &chunk_stream)
{
	auto &media_info = chunk_stream->GetMediaInfo();

	OV_ASSERT2(media_info != nullptr);

	if (media_info->is_streaming == false)
	{
		// DCL
		{
			std::lock_guard<__decltype(media_info->create_mutex)> lock_guard(media_info->create_mutex);

			if (
				(media_info->is_streaming == false) && media_info->is_video_available
				// (media_info->is_audio_available && media_info->is_video_available))
			)
			{
				media_info->is_streaming = CreateStream(chunk_stream->GetStreamInfo(), media_info);
			}
		}
	}

	return media_info->is_streaming;
}

void MpegTsServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	std::lock_guard<std::recursive_mutex> lock(_chunk_stream_list_mutex);

	ov::String client_ipaddr = address.GetIpAddress().CStr();
	auto local_port = remote->GetLocalAddress()->Port();

	// Check if the stream is already processing
	auto item = _chunk_stream_list.find(local_port);

	if (item != _chunk_stream_list.end())
	{
		// If the stream is already processing, verify that the ip and port of the client are same
		auto &stream_info = item->second->GetStreamInfo();

		if (stream_info->address != address)
		{
			// Ignore this packet because another client sents the packet
			logtw("Address %s != %s", stream_info->address.ToString().CStr(), address.ToString().CStr());
			return;
		}
	}
	else
	{
		// A new stream is incoming
		auto stream_info = QueryStreamInfo(local_port, &address);

		if (stream_info == nullptr)
		{
			// Could not find a stream info for the port
			return;
		}

		logtd("A new stream is being created for [%s/%s]", stream_info->app_name.CStr(), stream_info->stream_name.CStr());

		// stream_id must be 0 because the stream is before it is created
		OV_ASSERT2(stream_info->stream_id == 0);

		auto stream = std::make_shared<MpegTsChunkStream>(stream_info, this);

		auto pair = _chunk_stream_list.emplace(local_port, std::move(stream));

		if (pair.second == false)
		{
			logte("Could not process the request of the client: %s", address.ToString().CStr());
			return;
		}

		item = pair.first;
	}

	// TODO(dimiden): Check this code
	auto data2 = data->Clone();
	if (item->second->OnDataReceived(data2) < 0)
	{
		logte("Could not process the data: %s", address.ToString().CStr());
		return;
	}
}

bool MpegTsServer::OnChunkStreamVideoData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsChunkStream> &chunk_stream,
										  int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data)
{
	if (CreateStreamIfNeeded(chunk_stream))
	{
		// All streams are ready
		for (auto &observer : _observers)
		{
			if (observer->OnVideoData(stream_info, pts, dts, frame_type, data) == false)
			{
				logte("Could not send video data to the observer %p: %s", observer.get(), stream_info->ToString().CStr());
				return false;
			}
		}
	}
	else
	{
		// All streams are not ready, but it's okay
	}

	return true;
}

bool MpegTsServer::OnChunkStreamAudioData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsChunkStream> &chunk_stream,
										  int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data)
{
	return true;

	if (CreateStreamIfNeeded(chunk_stream))
	{
		// All streams are ready
		for (auto &observer : _observers)
		{
			if (observer->OnAudioData(stream_info, pts, dts, frame_type, data) == false)
			{
				logte("Could not send audio data to the observer %p: %s", observer.get(), stream_info->ToString().CStr());
				return false;
			}
		}
	}
	else
	{
		// All streams are not ready, but it's okay
	}

	return true;
}

#if 0
void MpegTsChunkStream::CheckStreamAlive()
{
	auto current = ::time(nullptr);

	if ((_last_packet_time != 0ULL) && ((current - _last_packet_time) > MPEGTS_ALIVE_TIME))
	{
		_stream_interface->OnDeleteStream(_stream_info);

		_media_info->start_streaming = false;
		_media_info->video_streaming = false;
		_media_info->audio_streaming = false;
	}

	_last_packet_time = current;
}
#endif