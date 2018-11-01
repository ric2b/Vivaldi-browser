// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Failing due to media patch
DISABLE(PipelineIntegrationTest, BasicPlaybackHi10P)
DISABLE(PipelineIntegrationTest,
        MediaSource_ConfigChange_EncryptedThenClear_MP4_CENC)

DISABLE(MediaColorTest, Yuv420pRec709H264)

// Disabled video tests on mac, until the tests can be fixed
DISABLE(MediaSourceTest, Playback_Video_WEBM_Audio_MP4)
DISABLE_MULTI(MediaTest, VideoBearHighBitDepthMp4)
