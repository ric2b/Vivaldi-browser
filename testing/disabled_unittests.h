// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

DISABLE(HistoryCounterTest, Synced)

DISABLE(InlineInstallPrivateApiTestApp, BackgroundInstall)

DISABLE_ALL(EncryptedMediaSupportedTypesWidevineTest)

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
DISABLE_MULTI(ShowPageActionWithoutPageActionTest, Test)
DISABLE(ToolbarViewInteractiveUITest, TestAppMenuOpensOnDrag)
DISABLE(ExtensionContextMenuModelTest, ExtensionContextMenuShowAndHide)

// DevTools related.
DISABLE(BrowserViewTest, DevToolsUpdatesBrowserWindow)
DISABLE(ChromeRenderProcessHostTest, DevToolsOnSelfInOwnProcess)
DISABLE(ChromeRenderProcessHostTest, DevToolsOnSelfInOwnProcessPPT)
DISABLE(DevToolsSanityTest, TestToolboxLoadedUndocked)
DISABLE(DevToolsTagTest, TagsManagerRecordsATag)
DISABLE(PolicyTest, DeveloperToolsDisabled)
DISABLE(TaskManagerBrowserTest, DevToolsNewUndockedWindow)
DISABLE(TaskManagerBrowserTest, DevToolsOldUndockedWindow)

// times out
DISABLE(WebViewTests/WebViewTest, WebViewInBackgroundPage/1)

// Seems to have failed in v59
DISABLE(AutofillInteractiveTest, CrossSitePaymentForms)
DISABLE(CommandsApiTest, PageAction)
DISABLE(CommandsApiTest, PageActionKeyUpdated)
DISABLE(ExtensionBrowserTest, PageAction)
DISABLE(ExtensionBrowserTest, PageActionCrash25562)
DISABLE(ExtensionBrowserTest, PageActionInPageNavigation)
DISABLE(ExtensionBrowserTest, PageActionRefreshCrash)
DISABLE(ExtensionBrowserTest, RSSMultiRelLink)
DISABLE(ExtensionBrowserTest, TitleLocalizationPageAction)
DISABLE(ExtensionBrowserTest, UnloadPageAction)
DISABLE(LazyBackgroundPageApiTest, BroadcastEvent)
DISABLE(PageActionApiTest, TestTriggerPageAction)
DISABLE(MediaRouterUIBrowserTest, UpdateActionLocation)
DISABLE(SitePerProcessTextInputManagerTest, TrackSelectionBoundsForAllFrames)
DISABLE(SitePerProcessTextInputManagerTest, TrackTextSelectionForAllFrames)

// Seems to have failed in v60
DISABLE(SitePerProcessTextInputManagerTest, ResetStateAfterFrameDetached)
DISABLE(SitePerProcessTextInputManagerTest,
        ResetTextInputStateOnActiveWidgetChange)
DISABLE(SitePerProcessTextInputManagerTest, TrackCompositionRangeForAllFrames)
DISABLE(PipelineIntegrationTest,
        MediaSource_ConfigChange_ClearThenEncrypted_WebM)
DISABLE(FFmpegGlueContainerTest, AAC)
DISABLE(FFmpegGlueContainerTest, MOV)
DISABLE(FFmpegGlueContainerTest, MP3)

// Flaky
DISABLE(SitePerProcessTextInputManagerTest, StopTrackingCrashedChildFrame)

// broke in v61
DISABLE(PasswordGenerationInteractiveTest, PopupShownAndPasswordSelected)

// Will be broken until we start using the TemplateURlService for searches
// because TemplateURLService::OnHistoryURLVisited is disabled until then.
DISABLE(TemplateURLServiceTest, GenerateVisitOnKeyword)

// Fails in raw chromium 64, too
DISABLE(PasswordManagerBrowserTestBase, DeleteFrameBeforeSubmit)

// Flaky in v64
DISABLE(PasswordGenerationInteractiveTest, AutoSavingGeneratedPassword)
DISABLE(WebViewContextMenuInteractiveTest, ContextMenuParamCoordinates)
DISABLE_MULTI(ParamaterizedFullscreenControllerInteractiveTest,
              MouseLockSilentAfterTargetUnlock)
DISABLE(SitePerProcessTextInputManagerTest,
        TrackStateWhenSwitchingFocusedFrames)
