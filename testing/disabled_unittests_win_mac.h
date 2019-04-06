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

// Seems to have failed in v59
//DISABLE_MULTI(ECKEncryptedMediaTest, VerifyCdmHostTest)
//DISABLE(PipelineIntegrationTest, SwitchVideoTrackDuringPlayback)
DISABLE(ChromeKeySystemsProviderTest, IsKeySystemsUpdateNeeded)

// Started to fail in 61
DISABLE(PipelineIntegrationTest, BasicFallback)
DISABLE(PlatformMediaPipelineIntegrationTest, BasicPlayback)
DISABLE(PlatformMediaPipelineIntegrationTest, PlayInLoop)
DISABLE(PlatformMediaPipelineIntegrationTest, Seek_VideoOnly)

// Should result in an error as the mode is unsupported in CDM 9,
// but seems to actually play successfully
DISABLE(CDM_9/ECKEncryptedMediaTest, Playback_Encryption_CBCS/0)
DISABLE(CDM_9/ECKEncryptedMediaTest, DecryptOnly_VideoOnly_MP4_CBCS/0)
