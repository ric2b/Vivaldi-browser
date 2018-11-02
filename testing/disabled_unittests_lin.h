// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Assume these fails due to switches::kExtensionActionRedesign being disabled
DISABLE(ToolbarActionViewInteractiveUITest, TestClickingOnOverflowedAction)

// VB-22258
DISABLE(ComponentFlashHintFileTest, CorruptionTest)
DISABLE(ComponentFlashHintFileTest, InstallTest)

// Seems to have broken in v57
DISABLE(RenderTextHarfBuzzTest, GetSubstringBoundsMultiline/HarfBuzz)

// Flaky
DISABLE(SitePerProcessBrowserTest, PopupMenuTest)

DISABLE(PointerLockBrowserTest, PointerLockEventRouting)
DISABLE(PointerLockBrowserTest, PointerLockWheelEventRouting)
DISABLE(LayerTreeHostTilesTestPartialInvalidation,
        PartialRaster_MultiThread_OneCopy)
DISABLE(SitePerProcessBrowserTest, SubframeGestureEventRouting)
DISABLE_MULTI(WebViewInteractiveTest, EditCommandsNoMenu)
