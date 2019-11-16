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
	struct RangedPort : public Port
	{
		RangedPort(const char *port, ov::SocketType default_type)
			: Port(port, default_type)
		{
		}

		explicit RangedPort(const char *port)
			: Port(port)
		{
		}

		virtual ov::String GetRangedPort() const
		{
			if(_port.IsEmpty() == false)
			{
				return _port.Split("/")[0];
			}

			return _port;
		}

	protected:
		using Port::GetPort;

		void MakeParseList() const override
		{
			Port::MakeParseList();
		}
	};
}  // namespace cfg