//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_stream.h"

std::shared_ptr<MpegTsStream> MpegTsStream::Create()
{
    auto stream = std::make_shared<MpegTsStream>();
    return stream;
}

MpegTsStream::MpegTsStream()
{
	
}


MpegTsStream::~MpegTsStream()
{
}
