// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // In Vivaldi this will always break since the relevant code has been modified
  DISABLE(BrowserOptionsWebUITest, testOpenBrowserOptions)

  // In Vivaldi this will always break since the relevant code has been modified
  DISABLE(BrowserOptionsOverlayWebUITest, testNavigationInBackground)
  // ******

  // In Vivaldi this will always break since the relevant code has been modified
  DISABLE(ExtensionApiTest, StubsApp)

  // internal HTML page modified, breaking the test
  DISABLE(HelpPageWebUITest, testOpenHelpPage)

  // tomas@vivaldi.com: disabled this test due to incognito work in progress.
  // VB-7149
  DISABLE(RenderViewContextMenuPrefsTest,
          DisableOpenInIncognitoWindowWhenIncognitoIsDisabled)
  // ******

  DISABLE(UberUIBrowserTest, EnableMdExtensionsHidesExtensions)
  DISABLE(UberUIBrowserTest, EnableMdHistoryHidesHistory)
  DISABLE(UberUIBrowserTest, EnableSettingsWindowHidesSettingsAndHelp)

  DISABLE(WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings,
          testCanIgnoreSelectors)
  DISABLE(WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings,
          testWithAuditFailuresAndExpectA11yOk)

  DISABLE(WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture,
          testRunningAuditManually_noErrors)
  DISABLE(WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture,
          testRunningAuditManuallySeveralTimes)

  // In Vivaldi this will always break since the relevant code has been modified
  DISABLE(WebViewTest, Shim_TestRendererNavigationRedirectWhileUnattached)
