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
#include "base/provider/stream.h"

using namespace pvd;

class MpegTsStream : public Stream
{
public:
	static std::shared_ptr<MpegTsStream> Create();

public:
	explicit MpegTsStream();
	~MpegTsStream() final;

private:

};