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
			: _port(port),
			  _default_type(default_type)
		{
		}

		explicit RangedPort(const char *port)
			: _port(port)
		{
		}

		std::vector<int> GetPortList() const
		{
			return ov::Converter::ToInt32(_port);
		}

	protected:
		using Port::GetPort;

		void MakeParseList() const override
		{
			RegisterValue<ValueType::Text>(nullptr, &_port);
		}

		ov::String _port;

		ov::SocketType _default_type = ov::SocketType::Tcp;
	};
}  // namespace cfg