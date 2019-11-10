//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_stream_map.h"
#include "provider.h"

namespace cfg
{
	struct MpegTsProvider : public Provider
	{
		ProviderType GetType() const override
		{
			return ProviderType::MpegTs;
		}

		bool IsBlockDuplicateStreamName() const
		{
			return _is_block_duplicate_stream_name;
		}

		const MpegTsStreamMap &GetStreamMap() const
		{
			return _stream_map;
		}

	protected:
		void MakeParseList() const override
		{
			Provider::MakeParseList();

			RegisterValue<Optional>("BlockDuplicateStreamName", &_is_block_duplicate_stream_name);
			RegisterValue("StreamMap", &_stream_map);
		}

		// Whether to accept new data when it is already streaming
		// 	true: block the new stream
		// 	false: disconnect the previous connection
		bool _is_block_duplicate_stream_name = true;
		MpegTsStreamMap _stream_map;
	};
}  // namespace cfg