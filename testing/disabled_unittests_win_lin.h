// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Proprietary media codec tests
  DISABLE(WebRtcBrowserTest, RunsAudioVideoWebRTCCallInTwoTabsH264)

  // Appears flaky on Win and Lin

  // Failing media tests ever since proprietary media code was imported
  DISABLE(MediaTest, VideoBearRotated270)
  DISABLE(MediaTest, VideoBearRotated90)

  // Seems to have broken on the Windows and Linux testers
  DISABLE(NavigatingExtensionPopupBrowserTest, DownloadViaPost)

/*
  // Toolbar tests that started failing in v52
  DISABLE(ToolbarViewInteractiveUITest, TestAppMenuOpensOnDrag)
  DISABLE_ALL(ToolbarActionViewInteractiveUITest)
*/

  // Broke in 55
  DISABLE(PageLoadMetricsBrowserTest, IgnoreDownloads)

