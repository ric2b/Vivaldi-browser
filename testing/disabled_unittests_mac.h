// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

/*
  // Disabled in v51 due to flakiness
  DISABLE(BrowserWindowControllerTest,
          FullscreenToolbarIsVisibleAccordingToPrefs)
*/
  DISABLE(DumpAccessibilityTreeTest, AccessibilityInputTime)

  DISABLE(SearchProviderTest, TestIsSearchProviderInstalled)

//  DISABLE(WebRtcAudioDebugRecordingsBrowserTest, CallWithAudioDebugRecordings)

  DISABLE(ExtensionApiTest, Bookmarks)

  DISABLE(ExtensionLoadingTest, RuntimeValidWhileDevToolsOpen)

  DISABLE(FullscreenControllerTest, PermissionContentSettings)

  DISABLE_MULTI(NativeAppWindowCocoaBrowserTest, Minimize)
  DISABLE_MULTI(NativeAppWindowCocoaBrowserTest, MinimizeMaximize)

  DISABLE(PlatformAppBrowserTest, WindowsApiProperties)

  DISABLE(PipelineIntegrationTest, BasicPlayback_MediaSource_VideoOnly_MP4_AVC3)
  DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video)
  DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_SENC_Video)
  DISABLE(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly)
  DISABLE(PipelineIntegrationTest,
          EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly)
  DISABLE(MediaColorTest, Yuv420pRec709H264)

  DISABLE(SpeechViewTest, ClickMicButton)

  // Broke in v52
  //DISABLE(WebViewPopupInteractiveTest, PopupPositioningBasic)
  //DISABLE(WebViewPopupInteractiveTest, PopupPositioningMoved)
  DISABLE(BrowserWindowControllerTest, FullscreenResizeFlags)
  DISABLE(ExtensionInstallUIBrowserTest, TestInstallThemeInFullScreen)
  DISABLE(FullscreenControllerTest, FullscreenOnFileURL)

  // Broke in v53
  DISABLE(BrowserWindowControllerTest,
          FullscreenToolbarExposedForTabstripChanges)

  DISABLE(ExtensionWindowCreateTest,AcceptState)
  DISABLE_MULTI(SavePageOriginalVsSavedComparisonTest, ObjectElementsViaFile)

  // Broke in v55
  DISABLE(ServiceProcessControlBrowserTest, LaunchAndReconnect)
  DISABLE(FindBarBrowserTest, EscapeKey)
