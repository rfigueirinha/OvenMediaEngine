//==============================================================================
//
//  MpegTsProvider
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <unistd.h>
#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include <base/ovlibrary/ovlibrary.h>

#include "base/provider/provider.h"
#include "base/provider/application.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"

#include "mpegts_server.h"
#include "mpegts_observer.h"

class MpegTsProvider : public pvd::Provider , public MpegTsObserver
{
    // class TranscodeApplication;
public:
    static std::shared_ptr<MpegTsProvider>
    Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    explicit MpegTsProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);

    ~MpegTsProvider() override;

    cfg::ProviderType GetProviderType() override
    {
        return cfg::ProviderType::MpegTs;
    }

    bool Start() override;

    bool Stop() override;

    std::shared_ptr<pvd::Application> OnCreateApplication(const info::Application *application_info) override;

    //--------------------------------------------------------------------
    // Implementation of MpegTsObserver
    //--------------------------------------------------------------------
    bool OnStreamReadyComplete(const ov::String &app_name,
                               const ov::String &stream_name,
                               std::shared_ptr<MpegTsMediaInfo> &media_info,
                               info::application_id_t &application_id,
                               uint32_t &stream_id) override;

    bool OnVideoData(info::application_id_t application_id,
                     uint32_t stream_id,
                     uint32_t timestamp,
                     MpegTsFrameType frame_type,
                     std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool OnAudioData(info::application_id_t application_id,
                     uint32_t stream_id,
                     uint32_t timestamp,
                     MpegTsFrameType frame_type,
                     std::shared_ptr<std::vector<uint8_t>> &data) override;

    bool OnDeleteStream(info::application_id_t application_id, uint32_t stream_id) override;

private:
    const cfg::MpegTsProvider *_provider_info;

    std::shared_ptr<MpegTsServer> _mpegts_server;
};

