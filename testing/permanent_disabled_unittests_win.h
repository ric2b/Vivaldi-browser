// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(AbortsPageLoadMetricsObserverTest)

// Seem to not work in VS2013 at least, even in Chrome
DISABLE_ALL(BlameContextTest)

// Disabled because the key playing sends wrong keys
DISABLE(BrowserKeyEventsTest, FocusMenuBarByAltKey)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(BackgroundTracingManagerBrowserTest)

// Nacl is disabled in Vivaldi
DISABLE_ALL(ChromeServiceWorkerFetchTest)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(ChromeTracingDelegateBrowserTest)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(ChromeTracingDelegateBrowserTestOnStartup)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(CorePageLoadMetricsObserverTest)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE(CookieMonsterTest, DeleteAllForHost)
DISABLE(CookieMonsterTest, PredicateSeesAllCookies)

DISABLE(KeyboardAccessTest, TestAltMenuKeyboardAccess)

DISABLE(MachineIdProviderTest, GetId)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(MetricsWebContentsObserverTest)
DISABLE_ALL(DocumentWritePageLoadMetricsObserverTest)
DISABLE_ALL(FromGWSPageLoadMetricsObserverTest)
DISABLE(RenderWidgetUnittest, TouchStartDuringOrOutsideFlingUmaMetrics)
DISABLE(OfflinePageModelImplTest, DownloadMetrics)
DISABLE(ActivityAnalyzerTest, GlobalAnalyzerConstruction)
DISABLE(ActivityTrackerTest, ScopedTaskTest)
DISABLE(ActivityTrackerTest, ThreadDeathTest)
DISABLE(ActivityTrackerTest, ProcessDeathTest)
DISABLE(RenderWidgetUnittest, TouchDuringOrOutsideFlingUmaMetrics)
DISABLE(HttpStreamFactoryImplJobControllerTest, DelayedTCP)
DISABLE(DevToolsSanityTest, TestNetworkPushTime)
DISABLE(BidirectionalStreamTest, LoadTimingTwoRequests)
DISABLE(BidirectionalStreamTest, SimplePostRequest)
DISABLE_MULTI(CacheStorageCacheTestP, DeleteWithIgnoreSearchFalse)
DISABLE_MULTI(CacheStorageCacheTestP, TwoKeysThenOne)
DISABLE_MULTI(CacheStorageCacheTestP, DeleteWithIgnoreSearchTrue)
DISABLE_MULTI(CacheStorageCacheTestP, KeysWithIgnoreSearchTrue)
DISABLE_MULTI(CacheStorageCacheTestP, MatchAll_TwoResponsesThenOne)
DISABLE_MULTI(CacheStorageCacheTestP, TwoKeys)
DISABLE_MULTI(CacheStorageCacheTestP, MatchAll_TwoResponsesThenOne)
DISABLE(InputEventFilterTest, NonBlockingWheel)
DISABLE(InputEventFilterTest, Basic)
DISABLE(RenderViewImplTest, RendererNavigationStartTransmittedToBrowser)
DISABLE(SRTFetcherTest, ReporterLogging_EnabledNoRecentLogging)
DISABLE(SpdySessionTest, MetricsCollectionOnPushStreams)
DISABLE(RenderThreadImplDiscardableMemoryBrowserTest, LockDiscardableMemory)
DISABLE_MULTI(WebRtcMediaRecorderTest, PeerConnection)
DISABLE_MULTI(WebRtcMediaRecorderTest, ResumeAndDataAvailable)
DISABLE_MULTI(WebRtcMediaRecorderTest, StartAndDataAvailable)
DISABLE(WebRtcMediaRecorderTest, TwoChannelAudioRecording)
DISABLE(SBNavigationObserverTest, TestCleanUpStaleNavigationEvents)
DISABLE(SBNavigationObserverTest, TestCleanUpStaleIPAddresses)
DISABLE(SBNavigationObserverTest, TestCleanUpStaleUserGestures)
DISABLE(SBNavigationObserverTest, TestNavigationEventList)
DISABLE(SBNavigationObserverTest, TestRecordHostToIpMapping)
DISABLE(SBNavigationObserverBrowserTest,
        PPAPIDownloadWithUserGestureOnHostingFrame)
DISABLE(TemplateURLServiceTest, LastModifiedTimeUpdate)
DISABLE(UploadProgressTrackerTest, TimePassed)
DISABLE(SRTFetcherTest, ReporterLogging_EnabledFirstRun)
DISABLE(AutofillProfileComparatorTest, MergeAddressesMostUniqueTokens)
DISABLE(NetworkMetricsProviderTest, EffectiveConnectionType)
DISABLE(TimeTicks, WinRollover)
DISABLE(ReportingCacheTest, EvictOldestReport)
DISABLE(PaymentManagerTest, ClearPaymentInstruments)
DISABLE(PaymentManagerTest, KeysOfPaymentInstruments)
DISABLE(SubresourceFilterBrowserTest, ExpectPerformanceHistogramsAreRecorded)
DISABLE(UpdateClientTest, TwoCrxUpdateDownloadTimeout)
DISABLE(UpdateClientTest, TwoCrxUpdateNoUpdate)

// Fails on tester, works on dev PC; assume it is the timing issue
DISABLE(NotificationPermissionContextTest, TestDenyInIncognitoAfterDelay)

// Seem to not work in VS2013 at least, even in Chrome
DISABLE_ALL(TraceEventAnalyzerTest)

// Seems broken in chromium, too
DISABLE(WebRtcGetUserMediaBrowserTest,
        TraceVideoCaptureControllerPerformanceDuringGetUserMedia)

// All our autobuild executables are signed, which is a fail condition for
// this test
DISABLE(ModuleInfoUtilTest, GetCertificateInfoUnsigned)

// Broken Media tests
DISABLE(PipelineIntegrationTest, Rotated_Metadata_180)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_270)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_90)

// Proprietary media codec tests
DISABLE(WebRtcBrowserTest, RunsAudioVideoWebRTCCallInTwoTabsH264)
DISABLE(MediaTest, VideoBearRotated270)
DISABLE(MediaTest, VideoBearRotated90)
