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
	bool OnChunkStreamReady(ov::String &client_ipaddr,
							ov::String &app_name, ov::String &stream_name,
							std::shared_ptr<MpegTsMediaInfo> &media_info,
							info::application_id_t &application_id,
							uint32_t &stream_id) override;

	bool OnChunkStreamVideoData(ov::ClientSocket *remote,
								info::application_id_t application_id, uint32_t stream_id,
								uint32_t timestamp,
								MpegTsFrameType frame_type,
								const std::shared_ptr<const ov::Data> &data) override;

	bool OnChunkStreamAudioData(ov::ClientSocket *remote,
								info::application_id_t application_id, uint32_t stream_id,
								uint32_t timestamp,
								MpegTsFrameType frame_type,
								const std::shared_ptr<const ov::Data> &data) override;

	bool OnDeleteStream(ov::ClientSocket *remote,
						ov::String &app_name, ov::String &stream_name,
						info::application_id_t application_id, uint32_t stream_id) override;

private:
	std::shared_ptr<PhysicalPort> _physical_port;

	std::vector<std::shared_ptr<MpegTsObserver>> _observers;

	std::recursive_mutex _chunk_stream_list_mutex;
	std::map<std::string, std::shared_ptr<MpegTsChunkStream>> _chunk_stream_list;
};
