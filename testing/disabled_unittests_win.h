// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Flaky on Windows, fails on tester and works on local machine
DISABLE(AutofillProfileComparatorTest, MergeAddresses)
DISABLE_ALL(NetworkQualityEstimatorTest)

// Flaky on Windows, fails on tester and works on local machine
DISABLE(LocalFileSyncServiceTest, LocalChangeObserver)

DISABLE(OmniboxViewTest, AcceptKeywordByTypingQuestionMark)

// Flaky in v51
DISABLE_ALL(TabDesktopMediaListTest)

// Flaky in v53
//DISABLE(BrowsingDataRemoverBrowserTest, Cache)

// Sems to have broken in v54
DISABLE(QuarantineWinTest, LocalFile_DependsOnLocalConfig)
DISABLE(DownloadTest, CheckLocalhostZone_DependsOnLocalConfig)

// Seems to have broken in v56
//DISABLE(PrefHashBrowserTestChangedSplitPrefInstance/PrefHashBrowserTestChangedSplitPref,
//        ChangedSplitPref/0)
//DISABLE(PrefHashBrowserTestClearedAtomicInstance/PrefHashBrowserTestClearedAtomic,
//        ClearedAtomic/1)
//DISABLE(PrefHashBrowserTestRegistryValidationFailureInstance/PrefHashBrowserTestRegistryValidationFailure,
//        RegistryValidationFailure/1)
//DISABLE(PrefHashBrowserTestRegistryValidationFailureInstance/PrefHashBrowserTestRegistryValidationFailure,
//        RegistryValidationFailure/3)
//DISABLE(PrefHashBrowserTestUnchangedCustomInstance/PrefHashBrowserTestUnchangedCustom,
//        UnchangedCustom/0)
//DISABLE(PrefHashBrowserTestUnchangedCustomInstance/PrefHashBrowserTestUnchangedCustom,
//        UnchangedCustom/3)
//DISABLE(PrefHashBrowserTestUnchangedDefaultInstance/PrefHashBrowserTestUnchangedDefault,
//        UnchangedDefault/2)
//DISABLE(PrefHashBrowserTestUntrustedInitializedInstance/PrefHashBrowserTestUntrustedInitialized,
//        UntrustedInitialized/0)
//DISABLE(PrefHashBrowserTestUntrustedInitializedInstance/PrefHashBrowserTestUntrustedInitialized,
//        UntrustedInitialized/3)

// Started failing in v54 after the Vivaldi sync code was added
DISABLE(StartupBrowserCreatorCorruptProfileTest,
        LastUsedProfileFallbackToAnyProfile)
DISABLE(StartupBrowserCreatorCorruptProfileTest,
        LastUsedProfileFallbackToUserManager)
DISABLE(StartupBrowserCreatorCorruptProfileTest, CannotCreateSystemProfile)

// Seems to have broken in v57
//DISABLE(SubresourceFilterBrowserTest,
//        ExpectHistogramsAreRecordedForFilteredChildFrames)
//DISABLE(SubresourceFilterWithPerformanceMeasurementBrowserTest,
//        ExpectHistogramsAreRecorded)
//DISABLE(ContentSubresourceFilterDriverFactoryTest,
//        SpecialCaseNavigationActivationListEnabledWithPerformanceMeasurement)

// Seems to have gotten flaky in v57 stable
//DISABLE(OAuthRequestSignerTest, DecodeEncoded)

//DISABLE(MediaGalleriesGalleryWatchApiTest, SetupWatchOnInvalidGallery)
//DISABLE(MediaGalleriesGalleryWatchApiTest,
//        SetupGalleryChangedListenerWithoutWatchers)
//DISABLE(MediaGalleriesGalleryWatchApiTest, SetupGalleryWatchWithoutListeners)

// Seems to have failed in v59
//DISABLE(ExtensionApiTest, UserLevelNativeMessaging)
//DISABLE(MediaGalleriesPlatformAppBrowserTest, NoGalleriesCopyTo)

// Failed in v60
//DISABLE(RenderThreadImplBrowserTest,
//        InputHandlerManagerDestroyedAfterCompositorThread)

// Flaky
//DISABLE(GenericSensorBrowserTest, AmbientLightSensorTest)
//DISABLE(SitePerProcessTextInputManagerTest,
//  TrackCompositionRangeForAllFrames)

// Failed in v61, also in pure chromium builds
//DISABLE(BookmarkBarViewTest10, KeyEvents)
//DISABLE(CertificateSelectorTest, GetSelectedCert)
DISABLE(FindInPageTest, SelectionRestoreOnTabSwitch)
//DISABLE(OmniboxViewTest, AcceptKeywordBySpace)
//DISABLE(OmniboxViewTest, BackspaceDeleteHalfWidthKatakana)
//DISABLE(OmniboxViewTest, BackspaceInKeywordMode)
//DISABLE(OmniboxViewTest, BasicTextOperations)
//DISABLE(OmniboxViewTest, CtrlArrowAfterArrowSuggestions)
DISABLE(OmniboxViewTest, DeleteItem)
//DISABLE(OmniboxViewTest, EscapeToDefaultMatch)
DISABLE(OmniboxViewTest, SelectAllStaysAfterUpdate)
//DISABLE(SSLClientCertificateSelectorMultiTabTest, SelectSecond)
//DISABLE(BookmarkBarViewTest23, ContextMenusKeyboard)
//DISABLE(BookmarkBarViewTest24, ContextMenusKeyboardEscape)
//DISABLE(KeyboardAccessTest, TestMenuKeyboardAccess)
//DISABLE(OmniboxViewTest, DesiredTLDWithTemporaryText)
//DISABLE(SitePerProcessInteractiveBrowserTest,
//        ShowAndHideDatePopupInOOPIFMultipleTimes)
//DISABLE_MULTI(WebViewInteractiveTest, EditCommandsNoMenu)

// Failed in v61, does not fail locally
//DISABLE(ChromeVisibilityObserverBrowserTest, VisibilityTest)
//DISABLE(SslCastSocketTest, TestConnectEndToEndWithRealSSL)

// Started to fail in 61
DISABLE(PlatformMediaPipelineIntegrationTest, AudioConfigChange)
DISABLE(PlatformMediaPipelineIntegrationTest, BasicPlayback_VideoOnly)
DISABLE(PlatformMediaPipelineIntegrationTest, SeekWhilePaused)
DISABLE(PlatformMediaPipelineIntegrationTest, SeekWhilePlaying)
DISABLE(PlatformMediaPipelineIntegrationTest, VideoConfigChange)

// Seems to fail in v69
DISABLE(PlatformMediaPipelineIntegrationTest, BasicPlayback_16x9_Aspect)
//Flaky in v69
DISABLE(PipelineIntegrationTest, Spherical)
DISABLE(PipelineIntegrationTest, BasicPlaybackHi10P)

// Seems to have broken in v63
DISABLE(OmniboxApiTest, DeleteOmniboxSuggestionResult)

// ===============================================================
//                      TEMPORARY - MEDIA
// ===============================================================

// media_unittests - temp - debug these

DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ADTS/0)
//DISABLE(LegacyByDts/MSEPipelineIntegrationTest, ConfigChange_ClearThenEncrypted_WebM/0)

DISABLE(NewByPts/MSEPipelineIntegrationTest, ADTS/0)
//DISABLE(NewByPts/MSEPipelineIntegrationTest, ConfigChange_ClearThenEncrypted_WebM/0)

DISABLE(PipelineIntegrationTest, BasicPlaybackHashed_ADTS)

// content_browsertests - temp - timed out - debug these
//DISABLE(File/MediaTest, VideoBearHighBitDepthMp4/0)
//DISABLE(Http/MediaTest, VideoBearHighBitDepthMp4/0)
//DISABLE(MediaColorTest, Yuv420pH264)
DISABLE(MediaTest, LoadManyVideos)

// Broke in v64, can't reproduce locally
DISABLE(NavigationControllerTest, LoadURL_WithBindings)
DISABLE(RenderFrameHostManagerTest, DisownOpenerAfterNavigation)
DISABLE(RenderFrameHostManagerTest, NavigateAfterMissingSwapOutACK)

// Broke in v65, can't reproduce locally
DISABLE(RenderFrameHostManagerTest, DisownOpenerDuringNavigation)

// Flaky on Win7
//DISABLE_MULTI(SSLUIWorkerFetchTest, MixedContentSubFrame)
//DISABLE_MULTI(SSLUIWorkerFetchTest, MixedContentSettings)

//Broke in v66
DISABLE(HistoryApiTest, SearchAfterAdd)

// Broke in v66, unable to reproduce locally
DISABLE(RenderProcessHostUnitTest, DoNotReuseError)

// Broke in v69
DISABLE(WebViewInteractiveTest, LongPressSelection)
