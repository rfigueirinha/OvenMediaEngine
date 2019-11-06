//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <string.h>
#include <vector>
#include <map>
#include <memory>

#define bytes_to_u16(MSB,LSB) (((unsigned int) ((unsigned char) MSB)) & 255)<<8 | (((unsigned char) LSB)&255)

#define MPEGTS_MAX_PACKET_SIZE                  188
#define MPEGTS_HEADER_SIZE                      4
#define MPEGTS_STREAM_VIDEO                     0xE0
#define MPEGTS_STREAM_AUDIO                     0xC0
#define MPEGTS_SYNC_BYTE                        0x47

#define TS_UNIT_START_INDICATOR_MASK            0x40
#define TS_TRANSPORT_ERROR_INDICATOR_MASK       0x80
#define TS_TRANSPORT_PRIORITY_MASK              0x20
#define TS_ADAPTATION_FIELD_MASK                0x30
#define TS_CONTINUITY_COUNTER_MASK              0x0F
#define TS_PACKET_IDENTIFIER_MASK               0x1FFF

#define PES_HEADER_SIZE                         9
#define PES_START_SIZE                          6
#define PES_PTS_MASK                            0x80
#define PES_PTS_DTS_MASK                        0xC0

#define MPEGTS_ADTS_HEADER_SIZE                 7
#define MPEGTS_ALIVE_TIME                       1

enum class MpegTsFrameType : int32_t {
    VideoIFrame = 'I', // VIDEO I Frame
    VideoPFrame = 'P', // VIDEO P Frame
    AudioFrame = 'A', // AUDIO Frame
};

//===============================================================================================
// 스트림 정보
//===============================================================================================
struct MpegTsMediaInfo {
public :
    MpegTsMediaInfo() {
        video_streaming = false;
        audio_streaming = false;
        start_streaming = false;

        // 비디오 정보
        video_width = 0;
        video_height = 0;
        video_framerate = 0;
        video_bitrate = 0;

        // 오디오 정보
        audio_channels = 0;
        audio_bits = 0;
        audio_samplerate = 0;
        audio_sampleindex = 0;
        audio_bitrate = 0;
    }

public :
    bool video_streaming;
    bool audio_streaming;
    bool start_streaming;

    // 비디오 정보
    int video_width;
    int video_height;
    float video_framerate;
    int video_bitrate;

    // 오디오 정보
    int audio_channels;
    int audio_bits;
    int audio_samplerate;
    int audio_sampleindex;
    int audio_bitrate;
};
