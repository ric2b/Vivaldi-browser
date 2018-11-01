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
