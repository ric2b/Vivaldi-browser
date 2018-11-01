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
