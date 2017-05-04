// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Seems to have broken on the Windows and Linux testers
  //DISABLE(NavigatingExtensionPopupBrowserTest, DownloadViaPost)

  // Broke in 55
  //DISABLE(PageLoadMetricsBrowserTest, IgnoreDownloads)

