//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "encode.h"

namespace cfg
{
	struct Encodes : public Item
	{
		const std::vector<Encode> &GetEncodes() const
		{
			return _encode_list;
		}

		const bool &GetHwAccel() const
		{
			return _hw_accel;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Encode", &_encode_list);
			RegisterValue<Optional>("HardwareAcceleration", &_hw_accel);
		}

		std::vector<Encode> _encode_list;
		bool _hw_accel;
	};
}