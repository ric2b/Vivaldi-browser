// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Seems broken on Win64
  DISABLE(ChildThreadImplBrowserTest, LockDiscardableMemory)

  // Flaky on Windows, fails on tester and works on local machine
  DISABLE(AutofillProfileComparatorTest, MergeAddresses)
  DISABLE(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissing)
  DISABLE_ALL(NetworkQualityEstimatorTest)

  // Seems broken in Vivaldi
  DISABLE_ALL(ExtensionMessageBubbleViewBrowserTestRedesign)

  // Flaky on Windows, fails on tester and works on local machine
  DISABLE(LocalFileSyncServiceTest, LocalChangeObserver)

  // Flaky on Windows
  DISABLE(NetworkQualityEstimatorTest, TestExternalEstimateProviderMergeEstimates)

  DISABLE(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissingAfterLoad)

  DISABLE(OmniboxViewTest, AcceptKeywordByTypingQuestionMark)

  // Seems to require specific keyboard configuration
  DISABLE(TextfieldTest, KeysWithModifiersTest)

  // Flaky in v51
  DISABLE_ALL(TabDesktopMediaListTest)

  //DISABLE(UnloadTest, BrowserCloseInfiniteUnload)
  DISABLE(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync)

  // Flaky in v53
  DISABLE(BrowsingDataRemoverBrowserTest, Cache)

  DISABLE(HistoryCounterTest, Synced)
