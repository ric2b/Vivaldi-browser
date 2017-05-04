// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Failing due to media patch
  DISABLE(PipelineIntegrationTest,
          EncryptedPlayback_MP4_CENC_SENC_NO_SAIZ_SAIO_Video)
  DISABLE(PipelineIntegrationTest, BasicPlayback_MediaSource_VideoOnly_MP4_AVC3)
  DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video)
  DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video)
  DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly)
  DISABLE(PipelineIntegrationTest,
          EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly)
  DISABLE(MediaColorTest, Yuv420pRec709H264)
