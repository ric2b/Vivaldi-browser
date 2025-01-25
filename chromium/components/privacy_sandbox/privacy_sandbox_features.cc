// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_features.h"

#include "base/feature_list.h"

namespace privacy_sandbox {

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kPrivacySandboxAdsNoticeCCT,
             "PrivacySandboxAdsNoticeCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPrivacySandboxAdsNoticeCCTAppIdName[] = "app-id";

const base::FeatureParam<std::string> kPrivacySandboxAdsNoticeCCTAppId{
    &kPrivacySandboxAdsNoticeCCT, kPrivacySandboxAdsNoticeCCTAppIdName, ""};

const base::FeatureParam<bool> kPrivacySandboxAdsNoticeCCTIncludeModeB{
    &kPrivacySandboxAdsNoticeCCT, "include-mode-b", false};
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPrivacySandboxSettings4,
             "PrivacySandboxSettings4",
             base::FEATURE_DISABLED_BY_DEFAULT); // Vivaldi

const char kPrivacySandboxSettings4ConsentRequiredName[] = "consent-required";
const char kPrivacySandboxSettings4NoticeRequiredName[] = "notice-required";
const char kPrivacySandboxSettings4RestrictedNoticeName[] = "restricted-notice";
const char kPrivacySandboxSettings4ForceShowConsentForTestingName[] =
    "force-show-consent-for-testing";
const char kPrivacySandboxSettings4ForceShowNoticeRowForTestingName[] =
    "force-show-notice-row-for-testing";
const char kPrivacySandboxSettings4ForceShowNoticeEeaForTestingName[] =
    "force-show-notice-eea-for-testing";
const char kPrivacySandboxSettings4ForceShowNoticeRestrictedForTestingName[] =
    "force-show-notice-restricted-for-testing";
const char kPrivacySandboxSettings4ForceRestrictedUserForTestingName[] =
    "force-restricted-user";
const char kPrivacySandboxSettings4ShowSampleDataForTestingName[] =
    "show-sample-data";

const base::FeatureParam<bool> kPrivacySandboxSettings4ConsentRequired{
    &kPrivacySandboxSettings4, kPrivacySandboxSettings4ConsentRequiredName,
    false};
const base::FeatureParam<bool> kPrivacySandboxSettings4NoticeRequired{
    &kPrivacySandboxSettings4, kPrivacySandboxSettings4NoticeRequiredName,
    false};
const base::FeatureParam<bool> kPrivacySandboxSettings4RestrictedNotice{
    &kPrivacySandboxSettings4, kPrivacySandboxSettings4RestrictedNoticeName,
    false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowConsentForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowConsentForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRowForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowNoticeRowForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeEeaForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowNoticeEeaForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceShowNoticeRestrictedForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceShowNoticeRestrictedForTestingName, false};
const base::FeatureParam<bool>
    kPrivacySandboxSettings4ForceRestrictedUserForTesting{
        &kPrivacySandboxSettings4,
        kPrivacySandboxSettings4ForceRestrictedUserForTestingName, false};
const base::FeatureParam<bool> kPrivacySandboxSettings4ShowSampleDataForTesting{
    &kPrivacySandboxSettings4,
    kPrivacySandboxSettings4ShowSampleDataForTestingName, false};

const base::FeatureParam<bool>
    kPrivacySandboxSettings4SuppressDialogForExternalAppLaunches{
        &kPrivacySandboxSettings4, "suppress-dialog-for-external-app-launches",
        true};

const base::FeatureParam<bool> kPrivacySandboxSettings4CloseAllPrompts{
    &kPrivacySandboxSettings4, "close-all-prompts", true};

BASE_FEATURE(kOverridePrivacySandboxSettingsLocalTesting,
             "OverridePrivacySandboxSettingsLocalTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePrivacySandboxPrompts,
             "DisablePrivacySandboxPrompts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxFirstPartySetsUI,
             "PrivacySandboxFirstPartySetsUI",
             base::FEATURE_DISABLED_BY_DEFAULT); // Vivaldi
const base::FeatureParam<bool> kPrivacySandboxFirstPartySetsUISampleSets{
    &kPrivacySandboxFirstPartySetsUI, "use-sample-sets", false};

BASE_FEATURE(kEnforcePrivacySandboxAttestations,
             "EnforcePrivacySandboxAttestations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultAllowPrivacySandboxAttestations,
             "DefaultAllowPrivacySandboxAttestations",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kPrivacySandboxEnrollmentOverrides[] =
    "privacy-sandbox-enrollment-overrides";

BASE_FEATURE(kPrivacySandboxAttestationsLoadPreInstalledComponent,
             "PrivacySandboxAttestationsLoadPreInstalledComponent",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxProactiveTopicsBlocking,
             "PrivacySandboxProactiveTopicsBlocking",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPrivacySandboxProactiveTopicsBlockingIncludeModeBName[] =
    "include-mode-b";

const base::FeatureParam<bool>
    kPrivacySandboxProactiveTopicsBlockingIncludeModeB{
        &kPrivacySandboxProactiveTopicsBlocking,
        kPrivacySandboxProactiveTopicsBlockingIncludeModeBName, false};

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kTrackingProtectionFullOnboardingMobileTrigger,
             "TrackingProtectionFullOnboardingMobileTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kAttributionDebugReportingCookieDeprecationTesting,
             "AttributionDebugReportingCookieDeprecationTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivateAggregationDebugReportingCookieDeprecationTesting,
             "PrivateAggregationDebugReportingCookieDeprecationTesting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxInternalsDevUI,
             "PrivacySandboxInternalsDevUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedWebsiteSetsDevUI,
             "RelatedWebsiteSetsDevUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFingerprintingProtectionSetting,
             "FingerprintingProtectionSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFingerprintingProtectionUx,
             "FingerprintingProtectionUx",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kUserBypassFingerprintingProtection{
    &kFingerprintingProtectionUx, "include-in-user-bypass", false};

BASE_FEATURE(kIpProtectionV1,
             "IpProtectionV1",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIpProtectionUx,
             "IpProtectionUx",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kUserBypassIpProtection{
    &kIpProtectionUx, "include-in-user-bypass", false};

BASE_FEATURE(kIpProtectionDogfoodDefaultOn,
             "IpProtectionDogfoodDefaultOn",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionSettingsLaunch,
             "TrackingProtectionSettingsLaunch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxRelatedWebsiteSetsUi,
             "PrivacySandboxRelatedWebsiteSetsUi",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionContentSettingInSettings,
             "TrackingProtectionContentSettingInSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionContentSettingUbControl,
             "TrackingProtectionContentSettingUbControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionContentSettingFor3pcb,
             "TrackingProtectionContentSettingFor3pcb",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kTrackingProtectionUserBypassPwa,
             "TrackingProtectionUserBypassPwa",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrackingProtectionUserBypassPwaTrigger,
             "TrackingProtectionUserBypassPwaTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPsRedesignAdPrivacyPage,
             "PsRedesignAdPrivacyPage",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<bool> kPsRedesignAdPrivacyPageEnableToggles{
    &kPsRedesignAdPrivacyPage, "enable-toggles", false};

BASE_FEATURE(kTrackingProtectionOnboarding,
             "TrackingProtectionOnboarding",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kTrackingProtectionBlock3PC{
    &kTrackingProtectionOnboarding, "block-3pc", false};

BASE_FEATURE(kTrackingProtectionReminder,
             "TrackingProtectionReminder",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kTrackingProtectionIsSilentReminder{
    &kTrackingProtectionReminder, "is-silent-reminder", false};

BASE_FEATURE(kPrivateStateTokensDevUI,
             "PrivateStateTokensDevUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kTrackingProtectionReminderDelay{
    &kTrackingProtectionReminder, "reminder-delay", base::TimeDelta::Max()};

BASE_FEATURE(kTrackingProtectionSentimentSurvey,
             "TrackingProtectionSentimentSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<base::TimeDelta> kTrackingProtectionTimeToSurvey{
    &kTrackingProtectionSentimentSurvey, "time-to-survey",
    base::TimeDelta::Max()};

const base::FeatureParam<int> kTrackingProtectionSurveyAnchor{
    &kTrackingProtectionSentimentSurvey, "survey-anchor", 0};

BASE_FEATURE(kPrivacySandboxActivityTypeStorage,
             "PrivacySandboxActivityTypeStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

const char kPrivacySandboxActivityTypeStorageLastNLaunchesName[] =
    "last-n-launches";

const base::FeatureParam<int> kPrivacySandboxActivityTypeStorageLastNLaunches{
    &kPrivacySandboxActivityTypeStorage,
    kPrivacySandboxActivityTypeStorageLastNLaunchesName, 100};

const char kPrivacySandboxActivityTypeStorageWithinXDaysName[] =
    "within-x-days";

const base::FeatureParam<int> kPrivacySandboxActivityTypeStorageWithinXDays{
    &kPrivacySandboxActivityTypeStorage,
    kPrivacySandboxActivityTypeStorageWithinXDaysName, 60};

const char kPrivacySandboxActivityTypeStorageSkipPreFirstTabName[] =
    "skip-pre-first-tab";

const base::FeatureParam<bool>
    kPrivacySandboxActivityTypeStorageSkipPreFirstTab{
        &kPrivacySandboxActivityTypeStorage,
        kPrivacySandboxActivityTypeStorageSkipPreFirstTabName, false};

BASE_FEATURE(kPrivacySandboxAdsDialogDisabledOnAll3PCBlock,
             "PrivacySandboxAdsDialogDisabledOnAll3PCBlock",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxPrivacyGuideAdTopics,
             "PrivacySandboxPrivacyGuideAdTopics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPrivacySandboxLocalNoticeConfirmation,
             "PrivacySandboxLocalNoticeConfirmation",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool>
    kPrivacySandboxLocalNoticeConfirmationDefaultToOSCountry{
        &kPrivacySandboxLocalNoticeConfirmation, "default-to-os-country",
        false};

BASE_FEATURE(kPrivacySandboxMigratePrefsToNoticeConsentDataModel,
             "PrivacySandboxMigratePrefsToNoticeConsentDataModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace privacy_sandbox
