//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_provider.h"
#include "mpegts_provider.h"

namespace cfg
{
	struct Providers : public Item
	{
		std::vector<const Provider *> GetProviderList() const
		{
			return {
				&_rtmp_provider,
                &_mpegts_provider,
			};
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("RTMP", &_rtmp_provider);
			RegisterValue<Optional>("MPEGTS", &_mpegts_provider);
		};

		std::vector<const Provider *> _providers;

		RtmpProvider _rtmp_provider;
		MpegTsProvider _mpegts_provider;
	};
}