// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(InlineInstallPrivateApiTestApp, BackgroundInstall)

  // Disabled for v50; reverted code broke the test
  //DISABLE(PasswordManagerBrowserTestBase,
  //        SkipZeroClickToggledAfterSuccessfulSubmissionWithAPI)

  DISABLE(NTPSnippetsServiceWithSyncTest, SyncStateCompatibility)

  // Disabled in v51, seems an upstream revert broke the test
  DISABLE(PictureLayerImplTest, DontAddLowResForSmallLayers)

  //DISABLE(ThemeServiceMaterialDesignTest, SeparatorColor)

  // Failing media tests since proprietary media code was imported
  DISABLE(AudioVideoMetadataExtractorTest, AndroidRotatedMP4Video)
  DISABLE(AudioVideoMetadataExtractorTest, AudioMP3)
  DISABLE(MediaGalleriesPlatformAppBrowserTest, GetMetadata)
  DISABLE_MULTI(MediaTest, VideoBearMovPcmS16be)
  DISABLE_MULTI(MediaTest, VideoBearMovPcmS24be)
  DISABLE_MULTI(MediaTest, VideoBearMp4)
  DISABLE_MULTI(MediaTest, VideoBearSilentMp4)
  DISABLE_MULTI(MediaTest, VideoBearHighBitDepthMp4)
  DISABLE(MediaTest, VideoBearRotated0)
  DISABLE(MediaTest, VideoBearRotated180)

  DISABLE(WebViewContextMenuInteractiveTest,
              ContextMenuParamsAfterCSSTransforms)

  // Toolbar tests that started failing in v52
  DISABLE_ALL(ComponentToolbarActionsBrowserTest)
  DISABLE_ALL(ShowPageActionWithoutPageActionRedesignTest)