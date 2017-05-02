// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  DISABLE(HistoryCounterTest, Synced)

  DISABLE(InlineInstallPrivateApiTestApp, BackgroundInstall)

  DISABLE_ALL(EncryptedMediaSupportedTypesWidevineTest)

  //DISABLE(NTPSnippetsServiceWithSyncTest, SyncStateCompatibility)

  // Broken for media after 57
  DISABLE_MULTI(MediaTest, VideoBearMp4Vp9)

  //DISABLE_ALL(FirefoxProfileImporterBrowserTest)

  // Assume these fails due to switches::kExtensionActionRedesign being disabled
  DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionContextMenu)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionBlockedActions)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, ExtensionActionWantsToRunAppearance)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, TestNeedsOverflow)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, TestActionFrameBounds)
  DISABLE_MULTI(ToolbarActionsBarUnitTest, TestStartAndEndIndexes)
  DISABLE(ToolbarActionViewInteractiveUITest, TestContextMenuOnOverflowedAction)
  DISABLE(ToolbarActionViewInteractiveUITest,
          ActivateOverflowedToolbarActionWithKeyboard)

  // Seems to have broken on all the testers
  //DISABLE(NavigatingExtensionPopupBrowserTest, DownloadViaPost)

  // Seems to be disabled in Google Chrome mode and the feature is default disabled
  //DISABLE(PrintPreviewWebUITest, ScalingUnchecksFitToPage)

  // Sync related tests that fail due to changes in Vivaldi sync, which will
  // not be propagated into the chromium specfic code
  //DISABLE(SyncSetupWebUITestAsync, RestoreSyncDataTypes)
