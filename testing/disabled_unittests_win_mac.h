// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows and Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// TODO: failing after Chromium 51 upgrade
DISABLE(PlatformMediaPipelineIntegrationTest, TruncatedMedia)

// Broken for media after 57
DISABLE_MULTI(MediaTest, VideoBearMp4Vp9)
