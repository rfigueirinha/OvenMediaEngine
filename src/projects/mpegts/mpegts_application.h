//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/provider/application.h>
#include <base/provider/stream.h>

class MpegTsApplication : public pvd::Application
{
public:
	static std::shared_ptr<MpegTsApplication> Create(const info::Application *application_info);

	explicit MpegTsApplication(const info::Application *info);
	~MpegTsApplication() override = default;

public:
	std::shared_ptr<pvd::Stream> OnCreateStream() override;

private:
};