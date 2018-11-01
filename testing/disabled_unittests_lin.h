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

// Seems broken on Linux 386
#if defined(ARCH_CPU_32_BITS)
DISABLE(PointerLockBrowserTest, PointerLockEventRouting)
#endif
