//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/common_types.h"

#include "base/provider/application.h"
#include "base/provider/stream.h"

using namespace pvd;

class MpegTsApplication : public Application
{
public:
	static std::shared_ptr<MpegTsApplication> Create(const info::Application *application_info);

	explicit MpegTsApplication(const info::Application *info);
	~MpegTsApplication() override = default;

public:
	std::shared_ptr<Stream> OnCreateStream() override;

private:
};