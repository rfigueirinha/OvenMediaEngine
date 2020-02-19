//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "bind/bind.h"
#include "virtual_hosts/virtual_hosts.h"

namespace cfg
{
	enum class ServerType
	{
		Unknown,
		Origin,
		Edge
	};

	struct Server : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetVersion, _version)

		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

		CFG_DECLARE_REF_GETTER_OF(GetPassphrase, _passphrase)

		CFG_DECLARE_GETTER_OF(GetDisableHLS, _disableHLS)

		CFG_DECLARE_GETTER_OF(GetDisableDASH, _disableDASH)

		CFG_DECLARE_GETTER_OF(GetDisableLLDASH, _disableLLDASH)

		CFG_DECLARE_REF_GETTER_OF(GetTypeName, _typeName)
		CFG_DECLARE_GETTER_OF(GetType, _type)

		CFG_DECLARE_REF_GETTER_OF(GetIp, _ip)
		CFG_DECLARE_REF_GETTER_OF(GetBind, _bind)

		CFG_DECLARE_REF_GETTER_OF(GetVirtualHostList, _virtual_hosts.GetVirtualHostList())

	protected:
		void MakeParseList() override
		{
			RegisterValue<ValueType::Attribute>("version", &_version);

			RegisterValue<Optional>("Name", &_name);

			RegisterValue<Optional>("Passphrase", &_passphrase);

			RegisterValue<Optional>("DisableHLS", &_disableHLS);

			RegisterValue<Optional>("DisableDASH", &_disableDASH);
			
			RegisterValue<Optional>("DisableLLDASH", &_disableLLDASH);

			RegisterValue("Type", &_typeName, nullptr, [this]() -> bool {
				_type = ServerType::Unknown;

				if (_typeName == "origin")
				{
					_type = ServerType::Origin;
				}
				else if (_typeName == "edge")
				{
					_type = ServerType::Edge;
				}

				return _type != ServerType::Unknown;
			});

			RegisterValue("IP", &_ip);
			RegisterValue("Bind", &_bind);

			RegisterValue<Optional>("VirtualHosts", &_virtual_hosts);
		}

		ov::String _version;

		ov::String _name;

		ov::String _passphrase;

		bool _disableHLS = false;

		bool _disableDASH = false;

		bool _disableLLDASH = false;

		ov::String _typeName;
		ServerType _type;

		ov::String _ip;
		Bind _bind;

		VirtualHosts _virtual_hosts;
	};
}  // namespace cfg