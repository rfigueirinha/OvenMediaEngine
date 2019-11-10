//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_stream.h"

namespace cfg
{
	struct MpegTsStreamMap : public Item
	{
		const std::vector<MpegTsStream> &GetStreamList() const
		{
			return _stream_list;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Stream", &_stream_list);
		}

		std::vector<MpegTsStream> _stream_list;
	};
}  // namespace cfg