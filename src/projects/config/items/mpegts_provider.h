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

		const MpegTsStreamMap &GetStreamMap() const
		{
			return _stream_map;
		}

	protected:
		void MakeParseList() const override
		{
			Provider::MakeParseList();

			RegisterValue("StreamMap", &_stream_map);
		}
		
		MpegTsStreamMap _stream_map;
	};
}  // namespace cfg