// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the browser-specific base::FeatureList features that are
// not shared with other process types.

#ifndef CHROME_BROWSER_BROWSER_FEATURES_H_
#define CHROME_BROWSER_BROWSER_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAllowUnmutedAutoplayForTWA);
#endif  // BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAutocompleteActionPredictorConfidenceCutoff);
BASE_DECLARE_FEATURE(kBookmarksTreeView);
BASE_DECLARE_FEATURE(kBookmarkTriggerForPrerender2);
BASE_DECLARE_FEATURE(kCertificateTransparencyAskBeforeEnabling);
BASE_DECLARE_FEATURE(kCertVerificationNetworkTime);
BASE_DECLARE_FEATURE(kClosedTabCache);

#if BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kDbusSecretPortal);
#endif

BASE_DECLARE_FEATURE(kDestroyProfileOnBrowserClose);
BASE_DECLARE_FEATURE(kDestroySystemProfiles);

BASE_DECLARE_FEATURE(kDevToolsConsoleInsights);
extern const base::FeatureParam<std::string> kDevToolsConsoleInsightsModelId;
extern const base::FeatureParam<double> kDevToolsConsoleInsightsTemperature;
extern const base::FeatureParam<bool> kDevToolsConsoleInsightsOptIn;

BASE_DECLARE_FEATURE(kDevToolsFreestylerDogfood);
extern const base::FeatureParam<std::string> kDevToolsFreestylerDogfoodModelId;
extern const base::FeatureParam<double> kDevToolsFreestylerDogfoodTemperature;

enum class DevToolsFreestylerUserTier {
    // Users who are internal testers or validators.
    // In future, the data from these users will be excluded from training data when logging is enabled.
    kTesters,
    // Users in the general public.
    kPublic
};

extern const base::FeatureParam<DevToolsFreestylerUserTier> kDevToolsFreestylerDogfoodUserTier;

BASE_DECLARE_FEATURE(kDevToolsExplainThisResourceDogfood);
extern const base::FeatureParam<std::string>
    kDevToolsExplainThisResourceDogfoodModelId;
extern const base::FeatureParam<double>
    kDevToolsExplainThisResourceDogfoodTemperature;

BASE_DECLARE_FEATURE(kDevToolsSharedProcessInfobar);
BASE_DECLARE_FEATURE(kDevToolsTabTarget);
BASE_DECLARE_FEATURE(kDevToolsVeLogging);
extern const base::FeatureParam<bool> kDevToolsVeLoggingTesting;

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kDoubleTapToZoomInTabletMode);
#endif

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kRegisterAppBoundEncryptionProvider);
BASE_DECLARE_FEATURE(kUseAppBoundEncryptionProviderForEncryption);
#endif

BASE_DECLARE_FEATURE(kFlexOrgManagementDisclosure);
BASE_DECLARE_FEATURE(kIncomingCallNotifications);
BASE_DECLARE_FEATURE(kKeyPinningComponentUpdater);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kLockProfileCookieDatabase);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kMuteNotificationSnoozeAction);
#endif

BASE_DECLARE_FEATURE(kNetworkAnnotationMonitoring);
BASE_DECLARE_FEATURE(kNewTabPageTriggerForPrerender2);
// This parameter is used to set a time threshold for triggering onMouseHover
// prerender. For example, if the value is 300, the New Tab Page prerender
// will start after 300ms after mouseHover duration is over 300ms.
const base::FeatureParam<int>
    kNewTabPagePrerenderStartDelayOnMouseHoverByMiliSeconds{
        &features::kNewTabPageTriggerForPrerender2,
        "prerender_start_delay_on_mouse_hover_ms", 300};
const base::FeatureParam<int>
    kNewTabPagePreconnectStartDelayOnMouseHoverByMiliSeconds{
        &features::kNewTabPageTriggerForPrerender2,
        "preconnect_start_delay_on_mouse_hover_ms", 100};
const base::FeatureParam<bool> kPrerenderNewTabPageOnMousePressedTrigger{
    &features::kNewTabPageTriggerForPrerender2,
    "prerender_new_tab_page_on_mouse_pressed_trigger", true};
// The hover trigger is not enabled as we're aware that this negatively
// affects other navigations like Omnibox search.
const base::FeatureParam<bool> kPrerenderNewTabPageOnMouseHoverTrigger{
    &features::kNewTabPageTriggerForPrerender2,
    "prerender_new_tab_page_on_mouse_hover_trigger", false};

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kNoPreReadMainDll);
#endif

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kNotificationOneTapUnsubscribe);
extern base::FeatureParam<bool>
    kNotificationOneTapUnsubscribeUseServiceIntentParam;
#endif

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kPlatformKeysAesEncryption);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_DECLARE_FEATURE(kPrerenderDSEHoldback);
BASE_DECLARE_FEATURE(kPromoBrowserCommands);
extern const char kBrowserCommandIdParam[];

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_DECLARE_FEATURE(kQuickSettingsPWANotifications);
#endif

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kReadAnythingPermanentAccessibility);
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
BASE_DECLARE_FEATURE(kRegisterOsUpdateHandlerWin);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

BASE_DECLARE_FEATURE(kRestartNetworkServiceUnsandboxedForFailedLaunch);
BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlocked);
BASE_DECLARE_FEATURE(kSandboxExternalProtocolBlockedWarning);

#if BUILDFLAG(IS_LINUX)
BASE_DECLARE_FEATURE(kSecretPortalKeyProviderUseForEncryption);
#endif

BASE_DECLARE_FEATURE(kSupportSearchSuggestionForPrerender2);

#if !BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kTaskManagerDesktopRefresh);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_DECLARE_FEATURE(kTriggerNetworkDataMigration);

#if BUILDFLAG(IS_CHROMEOS)
BASE_DECLARE_FEATURE(kTabCaptureBlueBorderCrOS);
#endif

BASE_DECLARE_FEATURE(kUseOsCryptAsyncForCookieEncryption);
BASE_DECLARE_FEATURE(kWebUsbDeviceDetection);

#if BUILDFLAG(IS_WIN)
BASE_DECLARE_FEATURE(kBrowserDynamicCodeDisabled);
#endif

BASE_DECLARE_FEATURE(kReportPakFileIntegrity);

BASE_DECLARE_FEATURE(kRemovalOfIWAsFromTabCapture);

}  // namespace features

#endif  // CHROME_BROWSER_BROWSER_FEATURES_H_
