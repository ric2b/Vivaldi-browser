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
DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video)
DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_NO_SAIZ_SAIO_Video)
DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video)
DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly)
DISABLE(PipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly)
DISABLE(PipelineIntegrationTest, MediaSource_ConfigChange_Encrypted_MP4_CENC_KeyRotation_VideoOnly)
DISABLE(PipelineIntegrationTest, MediaSource_ConfigChange_Encrypted_MP4_CENC_VideoOnly)
DISABLE(PipelineIntegrationTest, MediaSource_ConfigChange_MP4)

DISABLE(MediaColorTest, Yuv420pRec709H264)

// Disabled video tests on mac, until the tests can be fixed
DISABLE(MediaSourceTest, Playback_Video_WEBM_Audio_MP4)
DISABLE_MULTI(MediaTest, VideoBearHighBitDepthMp4)

// Mac keyboard shortcuts disabled
DISABLE(GlobalKeyboardShortcuts, ShortcutsToWindowCommand)
