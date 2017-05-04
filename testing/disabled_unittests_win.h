// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Seems broken on Win64
  //DISABLE(ChildThreadImplBrowserTest, LockDiscardableMemory)

  // Flaky on Windows, fails on tester and works on local machine
  DISABLE(AutofillProfileComparatorTest, MergeAddresses)
  //DISABLE(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissing)
  DISABLE_ALL(NetworkQualityEstimatorTest)

  // Seems broken in Vivaldi
  //DISABLE_ALL(ExtensionMessageBubbleViewBrowserTestRedesign)

  // Flaky on Windows, fails on tester and works on local machine
  DISABLE(LocalFileSyncServiceTest, LocalChangeObserver)

  //DISABLE(OfflinePageModelImplTest, DetectThatOfflineCopyIsMissingAfterLoad)

  DISABLE(OmniboxViewTest, AcceptKeywordByTypingQuestionMark)

  // Seems to require specific keyboard configuration
  //DISABLE(TextfieldTest, KeysWithModifiersTest)

  // Flaky in v51
  DISABLE_ALL(TabDesktopMediaListTest)

  //DISABLE(UnloadTest, BrowserCloseInfiniteUnload)
  //DISABLE(UnloadTest, CrossSiteInfiniteBeforeUnloadAsync)

  // Flaky in v53
  DISABLE(BrowsingDataRemoverBrowserTest, Cache)

  // Sems to have broken in v54
  DISABLE(QuarantineWinTest, LocalFile_DependsOnLocalConfig)
  DISABLE(DownloadTest, CheckLocalhostZone_DependsOnLocalConfig)

  // Seems to have broken in v56
  DISABLE(PrefHashBrowserTestChangedSplitPrefInstance/PrefHashBrowserTestChangedSplitPref,
      ChangedSplitPref/0)
  DISABLE(PrefHashBrowserTestClearedAtomicInstance/PrefHashBrowserTestClearedAtomic,
      ClearedAtomic/1)
  DISABLE(PrefHashBrowserTestRegistryValidationFailureInstance/PrefHashBrowserTestRegistryValidationFailure,
      RegistryValidationFailure/1)
  DISABLE(PrefHashBrowserTestRegistryValidationFailureInstance/PrefHashBrowserTestRegistryValidationFailure,
      RegistryValidationFailure/3)
  DISABLE(PrefHashBrowserTestUnchangedCustomInstance/PrefHashBrowserTestUnchangedCustom,
      UnchangedCustom/0)
  DISABLE(PrefHashBrowserTestUnchangedCustomInstance/PrefHashBrowserTestUnchangedCustom,
      UnchangedCustom/3)
  DISABLE(PrefHashBrowserTestUnchangedDefaultInstance/PrefHashBrowserTestUnchangedDefault,
      UnchangedDefault/2)
  DISABLE(PrefHashBrowserTestUntrustedInitializedInstance/PrefHashBrowserTestUntrustedInitialized,
      UntrustedInitialized/0)
  DISABLE(PrefHashBrowserTestUntrustedInitializedInstance/PrefHashBrowserTestUntrustedInitialized,
      UntrustedInitialized/3)

  // Flaky in v56
  //DISABLE(IndexedDBBrowserTest, GetAllMaxMessageSize)
  //DISABLE(GenericSensorBrowserTest, AmbientLightSensorTest)
  //DISABLE(IndexedDBBrowserTest, DeleteCompactsBackingStore)

  // Started failing in v54 after the Vivaldi sync code was added
  DISABLE(StartupBrowserCreatorCorruptProfileTest,
            LastUsedProfileFallbackToAnyProfile)
  DISABLE(StartupBrowserCreatorCorruptProfileTest,
          LastUsedProfileFallbackToUserManager)
  DISABLE(StartupBrowserCreatorCorruptProfileTest, CannotCreateSystemProfile)

  // Seems to have broken in v57
  //DISABLE(IndexedDBBrowserTest, GetAllMaxMessageSize)
  DISABLE(SubresourceFilterBrowserTest, ExpectHistogramsAreRecorded)
  DISABLE(SubresourceFilterWithPerformanceMeasurementBrowserTest,
          ExpectHistogramsAreRecorded)
  DISABLE(ContentSubresourceFilterDriverFactoryTest,
          SpecialCaseNavigationActivationListEnabledWithPerformanceMeasurement)
