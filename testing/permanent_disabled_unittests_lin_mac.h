// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac and Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Disabled video tests on linux and mac, until the tests can be fixed
  DISABLE(MediaSourceTest, Playback_Video_WEBM_Audio_MP4)
