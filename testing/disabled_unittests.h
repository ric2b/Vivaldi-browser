// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(InlineInstallPrivateApiTestApp, BackgroundInstall)

  // Disabled for v50; reverted code broke the test
  DISABLE(PasswordManagerBrowserTestBase,
          SkipZeroClickToggledAfterSuccessfulSubmissionWithAPI)

  DISABLE(ThemeServiceMaterialDesignTest, SeparatorColor)

  // Found flaky when looking at VB-13454. The order of downloads requested is
  // not necessarily the same as they get processed. See bug for log.
  DISABLE_MULTI(WebViewTest, DownloadPermission)
