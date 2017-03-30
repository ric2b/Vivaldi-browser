// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows and Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // never stop loading; disabled widevine pepper support(?)
  DISABLE(PepperContentSettingsSpecialCasesPluginsBlockedTest, WidevineCdm)

  // never stop loading; disabled widevine pepper support(?)
  DISABLE(PepperContentSettingsSpecialCasesJavaScriptBlockedTest, WidevineCdm)

#if defined(OFFICIAL_BUILD)
  DISABLE(End2EndTest, BasicFakeSoftwareVideo)
  DISABLE(End2EndTest, EvilNetwork)
  DISABLE(End2EndTest, OldPacketNetwork)
  DISABLE(End2EndTest, ReceiverClockFast)
  DISABLE(End2EndTest, ReceiverClockSlow)
  DISABLE(End2EndTest, ShoveHighFrameRateDownYerThroat)
  DISABLE(End2EndTest, SmoothPlayoutWithFivePercentClockRateSkew)
  DISABLE(End2EndTest, TestSetPlayoutDelay)

  DISABLE_MULTI(VideoEncoderTest, CanBeDestroyedBeforeVEAIsCreated)
  DISABLE_MULTI(VideoEncoderTest,EncodesVariedFrameSizes)
  DISABLE_MULTI(VideoEncoderTest,GeneratesKeyFrameThenOnlyDeltaFrames)
#endif // OFFICIAL_BUILD
