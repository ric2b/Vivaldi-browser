// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(AbortsPageLoadMetricsObserverTest)

  // Disabled because the key playing sends wrong keys
  DISABLE(BrowserKeyEventsTest, FocusMenuBarByAltKey)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(BackgroundTracingManagerBrowserTest)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(ChromeTracingDelegateBrowserTest)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(ChromeTracingDelegateBrowserTestOnStartup)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(CorePageLoadMetricsObserverTest)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE(CookieMonsterTest, DeleteAllForHost)

  DISABLE(KeyboardAccessTest, TestAltMenuKeyboardAccess)

  DISABLE(MachineIdProviderTest, GetId)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE(MetricsWebContentsObserverTest, DontLogIrrelevantNavigation)
  DISABLE(MetricsWebContentsObserverTest, LogAbortChains)

  // Fails on tester, works on dev PC; assume it is the timing issue
  DISABLE(NotificationPermissionContextTest, TestDenyInIncognitoAfterDelay)
