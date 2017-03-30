// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Seems broken on Win64
  DISABLE(ChildThreadImplBrowserTest, LockDiscardableMemory)

  //DISABLE(ExtensionFetchTest, ExtensionCannotFetchHostedResourceWithoutHostPermissions)

  // Flaky on Windows, fails on tester and works on local machine
  //DISABLE(ExtensionSettingsApiTest, ExtensionsSchemas)

  // Seems broken in Vivaldi
  DISABLE_ALL(ExtensionMessageBubbleViewBrowserTestRedesign)

  // Flaky on Windows, fails on tester and works on local machine
  DISABLE(LocalFileSyncServiceTest, LocalChangeObserver)

  // Flaky on Windows
  DISABLE(NetworkQualityEstimatorTest, TestExternalEstimateProviderMergeEstimates)

  //DISABLE(PluginPowerSaverBrowserTest, PluginMarkedEssentialAfterPosterClicked)

  //DISABLE(SyncFileSystemTest, AuthorizationTest)

  // Flaky in v51
  DISABLE_ALL(TabDesktopMediaListTest)

  //DISABLE(UnloadTest, BrowserCloseInfiniteUnload)
  DISABLE(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync)

  // failing in 51
  DISABLE(WebRtcBrowserTest, RunsAudioVideoWebRTCCallInTwoTabsH264)
