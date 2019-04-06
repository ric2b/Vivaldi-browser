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
//DISABLE_ALL(BlameContextTest)

// Disabled because the key playing sends wrong keys
//DISABLE(BrowserKeyEventsTest, FocusMenuBarByAltKey)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
DISABLE_ALL(BackgroundTracingManagerBrowserTest)

// Nacl is disabled in Vivaldi
//DISABLE_ALL(ChromeServiceWorkerFetchTest)

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
//DISABLE(CookieMonsterTest, DeleteAllForHost)
DISABLE(CookieMonsterTest, PredicateSeesAllCookies)

//DISABLE(KeyboardAccessTest, TestAltMenuKeyboardAccess)

DISABLE(MachineIdProviderTest, GetId)

// Disabled because high performance timing support (contant_tsc)
// is not detected by chromium on windows WM testers.
//DISABLE_ALL(MetricsWebContentsObserverTest)
//DISABLE_ALL(DocumentWritePageLoadMetricsObserverTest)
DISABLE_ALL(FromGWSPageLoadMetricsObserverTest)
//DISABLE(RenderWidgetUnittest, TouchStartDuringOrOutsideFlingUmaMetrics)
//DISABLE(ActivityAnalyzerTest, GlobalAnalyzerConstruction)
//DISABLE(ActivityTrackerTest, ScopedTaskTest)
//DISABLE(ActivityTrackerTest, ThreadDeathTest)
//DISABLE(ActivityTrackerTest, ProcessDeathTest)
//DISABLE(RenderWidgetUnittest, TouchDuringOrOutsideFlingUmaMetrics)
//DISABLE(HttpStreamFactoryImplJobControllerTest, DelayedTCP)
//DISABLE(DevToolsSanityTest, TestNetworkPushTime)
DISABLE(BidirectionalStreamTest, LoadTimingTwoRequests)
DISABLE(BidirectionalStreamTest, SimplePostRequest)
DISABLE_MULTI(CacheStorageCacheTestP, DeleteWithIgnoreSearchFalse)
DISABLE_MULTI(CacheStorageCacheTestP, TwoKeysThenOne)
DISABLE_MULTI(CacheStorageCacheTestP, DeleteWithIgnoreSearchTrue)
DISABLE_MULTI(CacheStorageCacheTestP, KeysWithIgnoreSearchTrue)
DISABLE_MULTI(CacheStorageCacheTestP, MatchAll_TwoResponsesThenOne)
DISABLE_MULTI(CacheStorageCacheTestP, TwoKeys)
//DISABLE(InputEventFilterTest, NonBlockingWheel)
//DISABLE(InputEventFilterTest, Basic)
//DISABLE(RenderViewImplTest, RendererNavigationStartTransmittedToBrowser)
//DISABLE(SpdySessionTest, MetricsCollectionOnPushStreams)
DISABLE(RenderThreadImplDiscardableMemoryBrowserTest, LockDiscardableMemory)
DISABLE(RenderThreadImplDiscardableMemoryBrowserTest, ReleaseFreeMemory)
DISABLE_MULTI(WebRtcMediaRecorderTest, PeerConnection)
DISABLE_MULTI(WebRtcMediaRecorderTest, ResumeAndDataAvailable)
DISABLE_MULTI(WebRtcMediaRecorderTest, StartAndDataAvailable)
DISABLE(WebRtcMediaRecorderTest, TwoChannelAudioRecording)
//DISABLE(SBNavigationObserverTest, TestCleanUpStaleNavigationEvents)
DISABLE(SBNavigationObserverTest, TestCleanUpStaleIPAddresses)
DISABLE(SBNavigationObserverTest, TestCleanUpStaleUserGestures)
DISABLE(SBNavigationObserverTest, TestNavigationEventList)
DISABLE(SBNavigationObserverTest, TestRecordHostToIpMapping)
//DISABLE(SBNavigationObserverBrowserTest,
//        PPAPIDownloadWithUserGestureOnHostingFrame)
DISABLE(TemplateURLServiceTest, LastModifiedTimeUpdate)
//DISABLE(UploadProgressTrackerTest, TimePassed)
DISABLE(AutofillProfileComparatorTest, MergeAddressesMostUniqueTokens)
//DISABLE(NetworkMetricsProviderTest, EffectiveConnectionType)
DISABLE(TimeTicks, WinRollover)
DISABLE(ReportingCacheTest, EvictOldestReport)
DISABLE(PaymentManagerTest, ClearPaymentInstruments)
DISABLE(PaymentManagerTest, KeysOfPaymentInstruments)
//DISABLE(SubresourceFilterBrowserTest, ExpectPerformanceHistogramsAreRecorded)
DISABLE(UpdateClientTest, TwoCrxUpdateDownloadTimeout)
//DISABLE(UpdateClientTest, TwoCrxUpdateNoUpdate)
//DISABLE(UpdateClientTest, TwoCrxUpdate)
DISABLE(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming)
DISABLE(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming_BrowserStartup)
DISABLE(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming_WaitForRenderer)
DISABLE(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming_NegativeLatency)
DISABLE(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming_NegativeLatencyForStartIPC)
DISABLE(ServiceWorkerMetricsTest, EmbeddedWorkerStartTiming_NegativeLatencyForStartedIPC)
DISABLE(ServiceWorkerNewScriptLoaderTest, UpdateViaCache_All)
DISABLE(ServiceWorkerNewScriptLoaderTest, UpdateViaCache_Imports)
DISABLE(ServiceWorkerURLRequestJobTest,
        NavPreloadMetrics_NavPreloadFirst_MainFrame)
//DISABLE(ContentSubresourceFilterThrottleManagerTest, LogActivation)
DISABLE(SessionsSyncManagerTest, TrackTasksOnLocalTabModified)
//DISABLE(DataReductionProxyNetworkDelegateTest, IncrementingMainFramePageId)
//DISABLE(DataReductionProxyNetworkDelegateTest, RedirectSharePid)
//DISABLE(DataReductionProxyNetworkDelegateTest, ResetSessionResetsId)
//DISABLE(DataReductionProxyNetworkDelegateTest, SubResourceNoPageId)
DISABLE(DownloadArchivesTaskTest, MultipleLargeArchivesToDownload)
DISABLE(DownloadArchivesTaskTest, SingleArchiveToDownload)
DISABLE(DownloadArchivesTaskTest, TooManyArchivesToDownload)
//DISABLE(MarkPageAccessedTaskTest, MarkPageAccessedTwice)
DISABLE(BackFwdMenuModelTest, ChapterStops)
//DISABLE(SafeBrowsingBlockingPageTest, PageWithMultipleMalwareResourceProceedThenDontProceed)
DISABLE(SafeBrowsingBlockingPageTest, PageWithMultipleMalwareResourceDontProceed)
DISABLE(SafeBrowsingBlockingPageTest, NavigatingBackAndForth)
DISABLE(SafeBrowsingBlockingPageTest, PageWithMalwareResourceDontProceed)
//DISABLE(CookieManagerTest, GetAllCookies)
//DISABLE(CookieManagerTest, GetCookieListAccessTime)
DISABLE(ServiceWorkerNavigationLoaderTest, Basic)
DISABLE(ServiceWorkerNavigationLoaderTest, BlobResponse)
DISABLE(ServiceWorkerNavigationLoaderTest, BrokenBlobResponse)
DISABLE(ServiceWorkerNavigationLoaderTest, Redirect)
DISABLE(ServiceWorkerNavigationLoaderTest, StreamResponseAndCancel)
DISABLE(ServiceWorkerNavigationLoaderTest, StreamResponse_Abort)
DISABLE(ServiceWorkerNavigationLoaderTest, EarlyResponse)
DISABLE(ServiceWorkerNavigationLoaderTest, NavigationPreload)
DISABLE(ServiceWorkerNavigationLoaderTest, StreamResponse)
DISABLE(OverscrollNavigationOverlayTest, WithScreenshot)
DISABLE(OverscrollNavigationOverlayTest, OverlayWindowSwap)
DISABLE(OverscrollNavigationOverlayTest, Navigation_LoadingUpdate)
DISABLE(OverscrollNavigationOverlayTest, ForwardNavigation)
DISABLE(OverscrollNavigationOverlayTest, CancelAfterSuccessfulNavigation)
DISABLE(OverscrollNavigationOverlayTest, ImmediateLoadOnNavigate)
DISABLE(OverscrollNavigationOverlayTest, Navigation_PaintUpdate)
DISABLE(OverscrollNavigationOverlayTest, WithoutScreenshot)
DISABLE(OverscrollNavigationOverlayTest, ForwardNavigationCancelled)
DISABLE(WebContentsImplTest, CrossSiteNavigationBackPreempted)
DISABLE(WebContentsImplBrowserTest, ResourceLoadCompleteFromNetworkCache)
//DISABLE(SSLUITest, TestBadHTTPSDownload)
DISABLE(DnsTransactionTest, HttpsMarkHttpsBad)
// Fails on tester, works on dev PC; assume it is the timing issue
DISABLE(NotificationPermissionContextTest, TestDenyInIncognitoAfterDelay)
DISABLE_MULTI(ActivationStateComputingThrottleSubFrameTest, Speculation)
DISABLE_MULTI(ActivationStateComputingThrottleSubFrameTest, SpeculationWithDelay)
DISABLE_MULTI(CastStreamingApiTestWithPixelOutput, RtpStreamError)
DISABLE(TaskSchedulerTaskTrackerHistogramTest, TaskLatency)
DISABLE(TaskSchedulerServiceThreadIntegrationTest, HeartbeatLatencyReport)
DISABLE_MULTI(CastStreamingApiTestWithPixelOutput, EndToEnd)
DISABLE(PrerenderTest, LinkManagerCancelThenAddAgain)
DISABLE(MetricsWebContentsObserverTest, LongestInputInSubframe)
DISABLE(DeclarativeNetRequestBrowserTest, RedirectRequests_MultipleExtensions/1)
DISABLE(AutoplayMetricsBrowserTest, RecordAutoplayAttemptUkm)


// Seem to not work in VS2013 at least, even in Chrome
//DISABLE_ALL(TraceEventAnalyzerTest)

// Seems broken in chromium, too
//DISABLE(WebRtcGetUserMediaBrowserTest,
//        TraceVideoCaptureControllerPerformanceDuringGetUserMedia)

// All our autobuild executables are signed, which is a fail condition for
// this test
DISABLE(ModuleInfoUtilTest, GetCertificateInfoUnsigned)

// Broken Media tests
DISABLE(PipelineIntegrationTest, Rotated_Metadata_180)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_270)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_90)
DISABLE(PipelineIntegrationTest, Rotated_Metadata_0)
// TODO(wdzierzanowski): Fix and enable on Windows (DNA-35224).
DISABLE(PlatformMediaPipelineIntegrationTest, BasicPlaybackPositiveStartTime)

// Proprietary media codec tests
DISABLE(WebRtcBrowserTest, RunsAudioVideoWebRTCCallInTwoTabsH264)
DISABLE(MediaTest, VideoBearRotated270)
DISABLE(MediaTest, VideoBearRotated90)

// Seems to create auto run entries in the registry, killing the tester due to
// process start overload
//DISABLE(ServiceProcessControlBrowserTest, LaunchAndReconnect)
