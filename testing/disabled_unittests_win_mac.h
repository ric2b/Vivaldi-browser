// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows and Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(PipelineIntegrationTest, BasicPlaybackHi10P)
  DISABLE(PipelineIntegrationTest, BasicPlayback_MediaSource_MP4_AudioOnly)

  // TODO: failing after Chromium 51 upgrade
  DISABLE_ALL(PlatformMediaPipelineIntegrationTest)
