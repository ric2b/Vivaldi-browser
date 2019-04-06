// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

DISABLE(DumpAccessibilityTreeTest, AccessibilityInputTime)

// Broke in v52
//DISABLE(BrowserWindowControllerTest, FullscreenResizeFlags)
//DISABLE(ExtensionInstallUIBrowserTest, TestInstallThemeInFullScreen)
//DISABLE(FullscreenControllerTest, FullscreenOnFileURL)

// Broke in v53
//DISABLE(BrowserWindowControllerTest, FullscreenToolbarExposedForTabstripChanges)

// //DISABLE(ExtensionWindowCreateTest,AcceptState)
//DISABLE_MULTI(SavePageOriginalVsSavedComparisonTest, ObjectElementsViaFile)

// Broke in v55
DISABLE(ServiceProcessControlBrowserTest, LaunchAndReconnect)
//DISABLE(FindBarBrowserTest, EscapeKey)

// Broke in v56
//DISABLE(WebstoreInlineInstallerTest, BlockInlineInstallFromFullscreenForBrowser)
//DISABLE(WebstoreInlineInstallerTest, BlockInlineInstallFromFullscreenForTab)

// Broke in v57
//DISABLE(WebstoreInlineInstallerTest, AllowInlineInstallFromFullscreenForBrowser)

// Broke due to features::kNativeNotifications being enabled by us
//DISABLE_ALL(NotificationsTest)
//DISABLE(PlatformNotificationServiceTest, DisplayPageNotificationMatches)
//DISABLE(PlatformNotificationServiceTest, DisplayPersistentNotificationMatches)
//DISABLE(PlatformNotificationServiceTest, PersistentNotificationDisplay)

// Broke in v57
//DISABLE(BrowserWindowControllerTest, FullscreenToolbarIsVisibleAccordingToPrefs)
//DISABLE(SessionRestoreTest, TabWithDownloadDoesNotGetRestored)
//DISABLE(SBNavigationObserverBrowserTest, DownloadViaHTML5FileApiWithUserGesture)

// Broke in v58 for some Macs (depends on OS Root certificate configuration)
//DISABLE(TrustStoreMacTest, SystemCerts)

// Seems to have broken in v58
//DISABLE(BrowserActionButtonUiTest, MediaRouterActionContextMenuInOverflow)
//DISABLE(BrowserActionButtonUiTest, OpenMenuOnDisabledActionWithMouseOrKeyboard)

// Media failures
DISABLE(DataRequestHandlerTest, DataSourceSizeSmallerThanRequested)

// Touchbar failures
//DISABLE(BrowserWindowTouchBarUnitTest, TouchBarItems)
//DISABLE(BrowserWindowTouchBarUnitTest, ReloadOrStopTouchBarItem)

//DISABLE(WebContentsImplBrowserTest, RenderWidgetDeletedWhileMouseLockPending)

// Broke in v60
DISABLE(InputMethodMacTest, MonitorCompositionRangeForActiveWidget)
//DISABLE(ExtensionApiTabTest, TabOpenerCraziness)

// Broke in v61
//DISABLE(DownloadExtensionTest,
//        DownloadExtensionTest_OnDeterminingFilename_CurDirInvalid)
//DISABLE(DownloadExtensionTest,
//        DownloadExtensionTest_OnDeterminingFilename_DangerousOverride)
//DISABLE(DownloadExtensionTest,
//        DownloadExtensionTest_OnDeterminingFilename_IncognitoSplit)
//DISABLE(DownloadExtensionTest,
//        DownloadExtensionTest_OnDeterminingFilename_NoChange)
//DISABLE(DownloadExtensionTest,
//        DownloadExtensionTest_OnDeterminingFilename_ReferencesParentInvalid)
//DISABLE(DownloadExtensionTest, DownloadExtensionTest_Download_ConflictAction)
//DISABLE(DownloadExtensionTest, DownloadExtensionTest_Open)
//DISABLE(DownloadExtensionTest, DownloadExtensionTest_SearchIdAndFilename)
//DISABLE(DownloadExtensionTest, DownloadsDragFunction)
//DISABLE(ExtensionWebstorePrivateApiTest, IconUrl)
//DISABLE(SSLClientCertificateSelectorCocoaTest, WorkaroundCrashySierra)
//DISABLE(CaptureScreenshotTest, CaptureScreenshot)

// Broke in v62
//DISABLE(FullscreenControllerTest, MouseLockBubbleHideCallbackLockThenFullscreen)
//DISABLE(GlobalKeyboardShortcuts, ShortcutsToWindowCommand)
//DISABLE(GlobalKeyboardShortcutsTest, SwitchTabsMac)
//DISABLE(PermissionBubbleInteractiveUITest, SwitchTabs/Views)
//DISABLE(PermissionBubbleInteractiveUITest, SwitchTabs/Cocoa)


// Broke in v65
DISABLE(AppShimInteractiveTest, Launch)
DISABLE(AppShimInteractiveTest, ShowWindow)
DISABLE(InputMethodMacTest, FinishComposingText)
DISABLE(InputMethodMacTest, InsertText)
DISABLE(InputMethodMacTest, SetMarkedText)
DISABLE(InputMethodMacTest, UnmarkText)

// ===============================================================
//                      TEMPORARY - MEDIA
// ===============================================================

// content_browsertests - temp - debug these

DISABLE(MediaTest, LoadManyVideos)
DISABLE(MediaTest, VideoBearRotated270)
DISABLE(MediaTest, VideoBearRotated90)

// browser_tests

DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_EncryptedVideo_CENC_EncryptedAudio_CBCS/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_EncryptedVideo_CENC_EncryptedAudio_CBCS/0)
//DISABLE(MSE_ExternalClearKey_Mojo/EncryptedMediaTest, Playback_ClearVideo_WEBM_EncryptedAudio_MP4/0)

DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_EncryptedVideo_CBCS_EncryptedAudio_CENC/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_EncryptedVideo_CBCS_EncryptedAudio_CENC/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_EncryptedVideo_CBCS_EncryptedAudio_CENC/0)
DISABLE(MSE_ExternalClearKey/EncryptedMediaTest, Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CBCS/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CBCS_Video_CENC_Audio/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CENC/0)
DISABLE(MSE_ClearKey/EncryptedMediaTest, Playback_Encryption_CENC_Video_CBCS_Audio/0)
DISABLE_MULTI(ECKEncryptedMediaTest, Playback_Encryption_CENC)

DISABLE_MULTI(ECKEncryptedMediaTest, Playback_Encryption_CBCS)

// media_unittests

// MSE - LegacyByDts
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, MP3/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_KeyRotation_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_MP4/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_NO_SAIZ_SAIO_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_MDAT_Video/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_ClearThenEncrypted_WebM/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, BasicPlayback_VideoOnly_MP4_AVC3/0)
DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_EncryptedThenClear_MP4_CENC/0)

// MSE - LegacyByDts
DISABLE(NewByPts/MSEPipelineIntegrationTest, MP3/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_KeyRotation_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_Encrypted_MP4_CENC_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_MP4/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_NO_SAIZ_SAIO_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_MDAT_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_ClearThenEncrypted_WebM/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, BasicPlayback_VideoOnly_MP4_AVC3/0)
DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_EncryptedThenClear_MP4_CENC/0)

//11 tests failed:
DISABLE(PipelineIntegrationTest, BasicPlaybackHashed_ADTS)
//DISABLE(PipelineIntegrationTest, Rotated_Metadata_180)
//DISABLE(PipelineIntegrationTest, Rotated_Metadata_270)
//DISABLE(PipelineIntegrationTest, Rotated_Metadata_90)
//DISABLE(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_180)
//DISABLE(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_270)
//DISABLE(PlatformMediaPipelineIntegrationTest, Rotated_Metadata_90)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, LegacyByDts_PlayToEnd/3)
DISABLE(ProprietaryCodecs/BasicMSEPlaybackTest, NewByPts_PlayToEnd/3)
//16 tests timed out:
//2 tests crashed:

// Seems to be broken by upgrade to OS X 10.13
//DISABLE(BrowserActionsBarIncognitoTest, IncognitoMode)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, PRE_TestSessionRestore)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestAddTabDuringShutdown)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestAddWindowDuringShutdown)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestCloseTabDuringShutdown)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestCloseWindowDuringShutdown)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestHangInBeforeUnloadMultipleTabs)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestHangInBeforeUnloadMultipleWindows)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestMultipleWindows)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestSessionRestore)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestShutdownMoreThanOnce)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestSingleTabShutdown)
//DISABLE_MULTI(BrowserCloseManagerBrowserTest, TestUnloadMultipleSlowTabs)
//DISABLE_MULTI(BrowserCloseManagerWithBackgroundModeBrowserTest, CloseAllBrowsersWithBackgroundMode)
//DISABLE_MULTI(BrowserCloseManagerWithDownloadsBrowserTest, TestWithDownloads)
//DISABLE(BrowserShutdownBrowserTest, PRE_TwoBrowsersClosingShutdownHistograms)
//DISABLE(BrowserShutdownBrowserTest, TwoBrowsersClosingShutdownHistograms)
//DISABLE(BrowserTabRestoreTest, DelegateRestoreTabDisposition)
//DISABLE(BrowserTabRestoreTest, RecentTabsMenuTabDisposition)
//DISABLE(ChromeRenderProcessHostTest, CloseAllTabsDuringProcessDied)
//DISABLE(ChromeServiceWorkerTest, CanCloseIncognitoWindowWithServiceWorkerController)
//DISABLE(DevToolsBeforeUnloadTest, TestDevToolsOnDevTools)
DISABLE(DevToolsBeforeUnloadTest, TestDockedDevToolsInspectedBrowserClose)
DISABLE(DevToolsBeforeUnloadTest, TestDockedDevToolsInspectedTabClose)
DISABLE(DevToolsBeforeUnloadTest, TestUndockedDevToolsApplicationClose)
DISABLE(DevToolsBeforeUnloadTest, TestUndockedDevToolsInspectedBrowserClose)
DISABLE(DevToolsBeforeUnloadTest, TestUndockedDevToolsInspectedTabClose)
//DISABLE(ExtensionManagementApiTest, LaunchPanelApp)
//DISABLE(FastUnloadTest, BrowserListForceCloseAfterNormalCloseWithFastUnload)
//DISABLE(FastUnloadTest, BrowserListForceCloseNoUnloadListenersWithFastUnload)
//DISABLE_MULTI(HostedAppPWAOnlyTest, OpenInChrome)
DISABLE(IndependentOTRProfileManagerTest, CallbackNotCalledAfterUnregister)
//DISABLE(IndependentOTRProfileManagerTest, DeleteWaitsForLastBrowser)
//DISABLE_MULTI(SafeBrowsingServiceMetadataTest, MalwareMainFrameInApp)
//DISABLE(PrerenderIncognitoBrowserTest, PrerenderIncognitoClosed)
//DISABLE(ProfileChooserViewExtensionsTest, LockProfile)
//DISABLE(ProfileChooserViewExtensionsTest, LockProfileBlockExtensions)
//DISABLE(ProfileHelperTest, DeleteActiveProfile)
//DISABLE(ProfileWindowBrowserTest, GuestClearsCookies)
//DISABLE(RunInBackgroundTest, RunInBackgroundBasicTest)
//DISABLE_MULTI(SSLUITest, InAppTestHTTPSExpiredCertAndProceed)
//DISABLE(StartupBrowserCreatorTest, ProfilesWithoutPagesNotLaunched)
//DISABLE(TabModalConfirmDialogTest, Quit)
//DISABLE(TabRestoreTest, RestoreFirstBrowserWhenSessionServiceEnabled)
//DISABLE(TabRestoreTest, RestoreTabFromClosedWindowByID)
//DISABLE(TabRestoreTest, RestoreWindowAndTab)
//DISABLE(UnloadTest, BrowserCloseBeforeUnloadOK)
//DISABLE(UnloadTest, BrowserCloseInfiniteBeforeUnload)
//DISABLE(UnloadTest, BrowserCloseInfiniteUnload)
//DISABLE(UnloadTest, BrowserCloseInfiniteUnloadAlert)
//DISABLE(UnloadTest, BrowserCloseNoUnloadListeners)
//DISABLE(UnloadTest, BrowserCloseTwoSecondBeforeUnloadAlert)
//DISABLE(UnloadTest, BrowserCloseTwoSecondUnloadAlert)
//DISABLE(UnloadTest, BrowserListCloseBeforeUnloadOK)
//DISABLE(UnloadTest, BrowserListDoubleCloseBeforeUnloadCancel)
//DISABLE(UnloadTest, BrowserListDoubleCloseBeforeUnloadOK)
//DISABLE(UnloadTest, BrowserListForceCloseAfterNormalClose)

// Seems to break on v65 master
DISABLE(CertVerifyProcMacTest, MacKeychainReordering)

// Flaky, disabled on chromium master
DISABLE(SitePerProcessBrowserTest, ScaledIframeRasterSize)

// Flaky
DISABLE_MULTI(WebViewSizeTest, Shim_TestResizeWebviewWithDisplayNoneResizesContent)

// Code it tests crashes undocking devtools.
DISABLE(ExtensionBrowserTest, DevToolsMainFrameIsCached)

// Broke in v69
DISABLE(AppShimMenuControllerUITest, WindowCycling)
