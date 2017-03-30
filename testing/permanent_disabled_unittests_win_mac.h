// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows and Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // never stop loading; disabled widevine pepper support(?)
  DISABLE(PepperContentSettingsSpecialCasesPluginsBlockedTest, WidevineCdm)

#if defined(OFFICIAL_BUILD)
  DISABLE_ALL(End2EndTest)

  DISABLE_MULTI(VideoEncoderTest, CanBeDestroyedBeforeVEAIsCreated)
  DISABLE_MULTI(VideoEncoderTest,EncodesVariedFrameSizes)
  DISABLE_MULTI(VideoEncoderTest,GeneratesKeyFrameThenOnlyDeltaFrames)
#endif // OFFICIAL_BUILD
