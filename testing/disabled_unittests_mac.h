// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

DISABLE(DumpAccessibilityTreeTest, AccessibilityInputTime)

DISABLE(ExtensionApiTest, Bookmarks)

// Broke in v52
DISABLE(BrowserWindowControllerTest, FullscreenResizeFlags)
DISABLE(ExtensionInstallUIBrowserTest, TestInstallThemeInFullScreen)
DISABLE(FullscreenControllerTest, FullscreenOnFileURL)

// Broke in v53
DISABLE(BrowserWindowControllerTest, FullscreenToolbarExposedForTabstripChanges)

// DISABLE(ExtensionWindowCreateTest,AcceptState)
DISABLE_MULTI(SavePageOriginalVsSavedComparisonTest, ObjectElementsViaFile)

// Broke in v55
DISABLE(ServiceProcessControlBrowserTest, LaunchAndReconnect)
DISABLE(FindBarBrowserTest, EscapeKey)

// Broke in v56
DISABLE(WebstoreInlineInstallerTest, BlockInlineInstallFromFullscreenForBrowser)
DISABLE(WebstoreInlineInstallerTest, BlockInlineInstallFromFullscreenForTab)

// Broke in v57
DISABLE(WebstoreInlineInstallerTest, AllowInlineInstallFromFullscreenForBrowser)

// Broke due to features::kNativeNotifications being enabled by us
DISABLE_ALL(NotificationsTest)
DISABLE(PlatformNotificationServiceTest, DisplayPageNotificationMatches)
DISABLE(PlatformNotificationServiceTest, DisplayPersistentNotificationMatches)
DISABLE(PlatformNotificationServiceTest, PersistentNotificationDisplay)

// Broke in v57
DISABLE(BrowserWindowControllerTest, FullscreenToolbarIsVisibleAccordingToPrefs)
DISABLE(SessionRestoreTest, TabWithDownloadDoesNotGetRestored)
DISABLE(SBNavigationObserverBrowserTest, DownloadViaHTML5FileApiWithUserGesture)

// Broke in v58 for some Macs (depends on OS Root certificate configuration)
DISABLE(TrustStoreMacTest, SystemCerts)

// Seems to have broken in v58
DISABLE(BrowserActionButtonUiTest, MediaRouterActionContextMenuInOverflow)
DISABLE(BrowserActionButtonUiTest, OpenMenuOnDisabledActionWithMouseOrKeyboard)

// Media failures
DISABLE(DataRequestHandlerTest, DataSourceSizeSmallerThanRequested)

// Touchbar failures
DISABLE(BrowserWindowTouchBarUnitTest, TouchBarItems)
DISABLE(BrowserWindowTouchBarUnitTest, ReloadOrStopTouchBarItem)

DISABLE(WebContentsImplBrowserTest, RenderWidgetDeletedWhileMouseLockPending)

// Broke in v60
DISABLE(InputMethodMacTest, MonitorCompositionRangeForActiveWidget)
DISABLE(ExtensionApiTabTest, TabOpenerCraziness)

// Broke in v61
DISABLE(DownloadExtensionTest,
        DownloadExtensionTest_OnDeterminingFilename_CurDirInvalid)
DISABLE(DownloadExtensionTest,
        DownloadExtensionTest_OnDeterminingFilename_DangerousOverride)
DISABLE(DownloadExtensionTest,
        DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit)
DISABLE(DownloadExtensionTest,
        DownloadExtensionTest_OnDeterminingFilename_NoChange)
DISABLE(DownloadExtensionTest,
        DownloadExtensionTest_OnDeterminingFilename_ReferencesParentInvalid)
DISABLE(DownloadExtensionTest, DownloadExtensionTest_Download_ConflictAction)
DISABLE(DownloadExtensionTest, DownloadExtensionTest_Open)
DISABLE(DownloadExtensionTest, DownloadExtensionTest_SearchIdAndFilename)
DISABLE(DownloadExtensionTest, DownloadsDragFunction)
DISABLE(ExtensionWebstorePrivateApiTest, IconUrl)
DISABLE(SSLClientCertificateSelectorCocoaTest, WorkaroundCrashySierra)
DISABLE(CaptureScreenshotTest, CaptureScreenshot)

// Broke in v62
DISABLE(FullscreenControllerTest, MouseLockBubbleHideCallbackLockThenFullscreen)
DISABLE(GlobalKeyboardShortcuts, ShortcutsToWindowCommand)
DISABLE(GlobalKeyboardShortcutsTest, SwitchTabsMac)
DISABLE(PermissionBubbleInteractiveUITest, SwitchTabs/Views)
DISABLE(PermissionBubbleInteractiveUITest, SwitchTabs/Cocoa)
