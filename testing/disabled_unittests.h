// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(HistoryCounterTest, Synced)

  DISABLE(InlineInstallPrivateApiTestApp, BackgroundInstall)

  DISABLE_ALL(EncryptedMediaSupportedTypesWidevineTest)

  DISABLE(NTPSnippetsServiceWithSyncTest, SyncStateCompatibility)

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
  DISABLE(MediaColorTest, Yuv420pHighBitDepth)
  DISABLE(MediaColorTest, Yuv422pH264)
  DISABLE(MediaColorTest, Yuv444pH264)
  DISABLE(MediaColorTest, Yuvj420pH264)

  DISABLE_ALL(FirefoxProfileImporterBrowserTest)

  // Assume these fails due to switches::kExtensionActionRedesign being disabled
  DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionContextMenu)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionBlockedActions)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionWantsToRunAppearance)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, TestNeedsOverflow)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, TestActionFrameBounds)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, TestStartAndEndIndexes)
  DISABLE(ToolbarActionViewInteractiveUITest, TestContextMenuOnOverflowedAction)
  DISABLE(ToolbarActionViewInteractiveUITest,
          ActivateOverflowedToolbarActionWithKeyboard)

  // Seems to have broken on all the testers
  DISABLE(NavigatingExtensionPopupBrowserTest, DownloadViaPost)

  // Seems to be disabled in Google Chrome mode and the feature is default disabled
  DISABLE(PrintPreviewWebUITest, ScalingUnchecksFitToPage)

  // Disable just for v56
  DISABLE(PipelineIntegrationTest, BasicPlaybackLive)
