//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_chunk_stream.h"
#include "mpegts_observer.h"

#include <map>

#include <base/ovsocket/ovsocket.h>
#include <physical_port/physical_port_manager.h>

class MpegTsServer : protected PhysicalPortObserver, public IMpegTsChunkStream
{
public:
	MpegTsServer() = default;
	~MpegTsServer() override = default;

	bool Start(const std::map<ov::port_t, ov::SocketAddress> &address_list);
	bool Stop();

	bool AddObserver(const std::shared_ptr<MpegTsObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<MpegTsObserver> &observer);

	bool Disconnect(const std::shared_ptr<const MpegTsStreamInfo> &stream_info);

protected:
	bool ParseAddressList(const ov::String &ip, const cfg::RangedPort &ranged_port, std::map<int, ov::SocketAddress> *address_list);
	std::shared_ptr<PhysicalPort> CreatePhysicalPort(const ov::SocketAddress &address);
	std::shared_ptr<const MpegTsStreamInfo> QueryStreamInfo(uint16_t port_num, const ov::SocketAddress *address);

	bool CreateStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsMediaInfo> &media_info);
	bool DeleteStream(const std::shared_ptr<const MpegTsStreamInfo> &stream_info);

	bool CreateStreamIfNeeded(const std::shared_ptr<MpegTsChunkStream> &chunk_stream);

	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
						const ov::SocketAddress &address,
						const std::shared_ptr<const ov::Data> &data) override;

	//--------------------------------------------------------------------
	// Implementation of IMpegTsChunkStream
	//--------------------------------------------------------------------
	bool OnChunkStreamVideoData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsChunkStream> &chunk_stream,
								int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) override;

	bool OnChunkStreamAudioData(const std::shared_ptr<const MpegTsStreamInfo> &stream_info, const std::shared_ptr<MpegTsChunkStream> &chunk_stream,
								int64_t pts, int64_t dts, MpegTsFrameType frame_type, const std::shared_ptr<const ov::Data> &data) override;

	std::vector<std::shared_ptr<PhysicalPort>> _physical_port_list;
	std::recursive_mutex _physical_port_list_mutex;

	std::vector<std::shared_ptr<MpegTsObserver>> _observers;

	std::recursive_mutex _chunk_stream_list_mutex;
	std::map<uint32_t, std::shared_ptr<MpegTsChunkStream>> _chunk_stream_list;
};
