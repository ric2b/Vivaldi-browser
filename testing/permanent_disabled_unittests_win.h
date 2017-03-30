// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(AbortsPageLoadMetricsObserverTest)

  // Seem to not work in VS2013 at least, even in Chrome
  DISABLE_ALL(BlameContextTest)

  // Disabled because the key playing sends wrong keys
  DISABLE(BrowserKeyEventsTest, FocusMenuBarByAltKey)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(BackgroundTracingManagerBrowserTest)

  // Nacl is disabled in Vivaldi
  DISABLE_ALL(ChromeServiceWorkerFetchTest)

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
  DISABLE(CookieMonsterTest, PredicateSeesAllCookies)

  DISABLE(KeyboardAccessTest, TestAltMenuKeyboardAccess)

  DISABLE(MachineIdProviderTest, GetId)

  // Disabled because high performance timing support (contant_tsc)
  // is not detected by chromium on windows WM testers.
  DISABLE_ALL(MetricsWebContentsObserverTest)
  DISABLE_ALL(DocumentWritePageLoadMetricsObserverTest)
  DISABLE_ALL(FromGWSPageLoadMetricsObserverTest)
  DISABLE(RenderWidgetUnittest, TouchStartDuringOrOutsideFlingUmaMetrics)

  // Fails on tester, works on dev PC; assume it is the timing issue
  DISABLE(NotificationPermissionContextTest, TestDenyInIncognitoAfterDelay)

#if defined(_MSC_VER) && _MSC_VER<1900
  // Seems to rely on VS2015 specific format string argument
  DISABLE(SafeBrowsingProtocolParsingTest, TestGetHash)
#endif
  // Seem to not work in VS2013 at least, even in Chrome
  DISABLE_ALL(TraceEventAnalyzerTest)

  // Seems broken in chromium, too
  DISABLE(WebRtcGetUserMediaBrowserTest,
          TraceVideoCaptureControllerPerformanceDuringGetUserMedia)