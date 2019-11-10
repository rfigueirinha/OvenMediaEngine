//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "port.h"

namespace cfg
{
	struct MpegTsStream : public Item
	{
		ov::String GetName() const
		{
			return _name;
		}

		ov::String GetPort() const
		{
			return _port;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Port", &_port);
		}

		ov::String _name = "stream";
		ov::String _port = "7000";
	};
}  // namespace cfg