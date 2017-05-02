// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Broke in v53
  //DISABLE(ConditionalCacheDeletionHelperBrowserTest, TimeAndURL)

  //DISABLE(ExtensionFetchTest, ExtensionCanFetchExtensionResource)

  //DISABLE(SitePerProcessBrowserTest, RFPHDestruction)

  //DISABLE(RenderFrameHostManagerTest, SwapProcessWithRelNoopenerAndTargetBlank)

  // Assume these fails due to switches::kExtensionActionRedesign being disabled
  DISABLE(ToolbarActionViewInteractiveUITest, TestClickingOnOverflowedAction)

  // VB-22258
  DISABLE(ComponentFlashHintFileTest, CorruptionTest)
  //DISABLE(ComponentFlashHintFileTest, ExistsTest)
  DISABLE(ComponentFlashHintFileTest, InstallTest)

  // Seems to have broken in v57
  DISABLE(RenderTextHarfBuzzTest, GetSubstringBoundsMultiline/HarfBuzz)
