// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(DumpAccessibilityTreeTest, AccessibilityInputTime)

  //DISABLE(SearchProviderTest, TestIsSearchProviderInstalled)

  DISABLE(ExtensionApiTest, Bookmarks)

  //DISABLE(ExtensionLoadingTest, RuntimeValidWhileDevToolsOpen)

  //DISABLE(FullscreenControllerTest, PermissionContentSettings)

  //DISABLE_MULTI(NativeAppWindowCocoaBrowserTest, Minimize)
  //DISABLE_MULTI(NativeAppWindowCocoaBrowserTest, MinimizeMaximize)

  //DISABLE(PlatformAppBrowserTest, WindowsApiProperties)

  //DISABLE(SpeechViewTest, ClickMicButton)

  // Broke in v52
  DISABLE(BrowserWindowControllerTest, FullscreenResizeFlags)
  DISABLE(ExtensionInstallUIBrowserTest, TestInstallThemeInFullScreen)
  DISABLE(FullscreenControllerTest, FullscreenOnFileURL)

  // Broke in v53
  DISABLE(BrowserWindowControllerTest,
          FullscreenToolbarExposedForTabstripChanges)

  //DISABLE(ExtensionWindowCreateTest,AcceptState)
  DISABLE_MULTI(SavePageOriginalVsSavedComparisonTest, ObjectElementsViaFile)

  // Broke in v55
  DISABLE(ServiceProcessControlBrowserTest, LaunchAndReconnect)
  DISABLE(FindBarBrowserTest, EscapeKey)

  // Broke in v56
  DISABLE(WebstoreInlineInstallerTest,
          BlockInlineInstallFromFullscreenForBrowser)
  DISABLE(WebstoreInlineInstallerTest, BlockInlineInstallFromFullscreenForTab)

  // Broke due to features::kNativeNotifications being enabled by us
  DISABLE_ALL(NotificationsTest)
  DISABLE(PlatformNotificationServiceTest, DisplayPageNotificationMatches)
  DISABLE(PlatformNotificationServiceTest, DisplayPersistentNotificationMatches)
  DISABLE(PlatformNotificationServiceTest, PersistentNotificationDisplay)

  // Flaky in v56?
  //DISABLE(AppShimMenuControllerUITest, WindowCycling)

  // Broke in v57
  DISABLE(BrowserWindowControllerTest,
          FullscreenToolbarIsVisibleAccordingToPrefs)
  DISABLE(SessionRestoreTest, TabWithDownloadDoesNotGetRestored)
  DISABLE(SBNavigationObserverBrowserTest,
          DownloadViaHTML5FileApiWithUserGesture)
