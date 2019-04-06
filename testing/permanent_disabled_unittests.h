// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for all platforms  by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// In Vivaldi this will always break since the relevant code has been modified
DISABLE(ExtensionApiTest, StubsApp)

// never stop loading; disabled widevine pepper support(?)
DISABLE(PepperContentSettingsSpecialCasesJavaScriptBlockedTest, WidevineCdm)

//DISABLE(WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings,
//        testCanIgnoreSelectors)
DISABLE(WebUIAccessibilityAuditBrowserTest_IssuesAreWarnings,
        testWithAuditFailuresAndExpectA11yOk)

DISABLE(WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture,
        testRunningAuditManually_noErrors)
DISABLE(WebUIAccessibilityAuditBrowserTest_TestsDisabledInFixture,
        testRunningAuditManuallySeveralTimes)

// Found flaky when looking at VB-13454. The order of downloads requested is
// not necessarily the same as they get processed. See bug for log.
DISABLE_MULTI(WebViewTest, DownloadPermission)

// In Vivaldi this will always break since the relevant code has been modified
//DISABLE(WebViewTest, Shim_TestRendererNavigationRedirectWhileUnattached)
//DISABLE(BrowsingHistoryHandlerTest, ObservingWebHistoryDeletions)

// Suspect that this can cause addition of processes that stick around,
// causing lots of processes to start on boot, in case of one or more crashes
//DISABLE(ServiceProcessStateTest, AutoRun)

// Feature we do not use
DISABLE_ALL(WebHistoryServiceTest)
DISABLE(HistoryApiTest, Delete)

// We are using a different geolocation API URL and key
//DISABLE(GeolocationBrowserTest, UrlWithApiKey)
DISABLE(GeolocationNetworkProviderTest, EmptyApiKey)

// We have changed values
DISABLE(TopSitesImplTest, SetForcedTopSites)

DISABLE(AudioFileReaderTest, AAC)
DISABLE(AudioFileReaderTest, AAC_SinglePacket)
DISABLE(AudioFileReaderTest, AAC_ADTS)

// Media
// TODO(wdzierzanowski): Fix and enable again (DNA-30573).
DISABLE(PlatformMediaPipelineIntegrationTest, DecodingError)

// Disabled because we disabled Feature safe_browsing::kCanShowScoutOptIn
// in order to prevent users accidentally submitting info to Google
DISABLE(TriggerManagerTest, AdSamplerTrigger)
DISABLE(TriggerManagerTest, StartAndFinishCollectingThreatDetails)
DISABLE(TriggerManagerTest, UserOptsInToSBER_DataCollected_ReportSent)
DISABLE(TriggerManagerTest, NoCollectionWhenOutOfQuota)
DISABLE(NotificationImageReporterTest, ImageDownscaling)
DISABLE(NotificationImageReporterTest, MaxReportsPerDay)
DISABLE(NotificationImageReporterTest, ReportSuccess)
DISABLE(PolicyTest, SafeBrowsingExtendedReportingPolicyManaged)
DISABLE_ALL(TrialComparisonCertVerifierTest)
DISABLE(SuspiciousSiteTriggerTest, MonitorMode_SuspiciousHitDuringLoad)

// Fix for VB-33324 broke this
DISABLE(ExtensionBrowserTest, DevToolsMainFrameIsCached)

// We have changed the configuration tested by this
DISABLE(RulesRegistryServiceTest, DefaultRulesRegistryRegistered)
