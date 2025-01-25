// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_

// This file defines all password manager features used in the browser process.
// Prefer adding new features here instead of "core/common/".
#include <limits>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace password_manager::features {
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

// When enabled, updates to shared existing passwords from the same sender are
// auto-approved.
BASE_DECLARE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Feature flag to control the displaying of an ongoing hats survey that
// measures users perception of autofilling password forms. Differently from
// other surveys, the Autofill user perception surveys will not have a specific
// target number of answers where it will be fully stop, instead, it will run
// indefinitely. A target number of full answers exists, but per quarter. The
// goal is to have a go to place to understand how users are perceiving autofill
// across quarters.
BASE_DECLARE_FEATURE(kAutofillPasswordUserPerceptionSurvey);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN)
// OS authentication will use IUserConsentVerifierInterop api to trigger Windows
// Hello authentication. This api allows us to specify explicitly to which
// window, the OS prompt should attach.
BASE_DECLARE_FEATURE(kAuthenticateUsingUserConsentVerifierInteropApi);

// OS authentication will use UserConsentVerifier api to trigger Windows Hello
// authentication.
BASE_DECLARE_FEATURE(kAuthenticateUsingUserConsentVerifierApi);
#endif  // BUILDFLAG(IS_WIN)

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_DECLARE_FEATURE(kBiometricTouchToFill);

// Delete undecryptable passwords from the login database.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswords);

// Delete undecryptable passwords from the store when Sync is active.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);

#if BUILDFLAG(IS_ANDROID)
// Enables reading credentials from SharedPreferences.
BASE_DECLARE_FEATURE(kFetchGaiaHashOnSignIn);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
BASE_DECLARE_FEATURE(kFillOnAccountSelect);

#if BUILDFLAG(IS_IOS)
// Enables filling for sign-in UFF on iOS.
BASE_DECLARE_FEATURE(kIOSPasswordSignInUff);

// Enable saving username in UFF on iOS.
BASE_DECLARE_FEATURE(kIosDetectUsernameInUff);

// Enables password generation bottom sheet to be displayed (on iOS) when a user
// is signed-in and taps on a new password field.
BASE_DECLARE_FEATURE(kIOSProactivePasswordGenerationBottomSheet);

#endif

// Enables saving enterprise password hashes to a local state preference.
BASE_DECLARE_FEATURE(kLocalStateEnterprisePasswordHashes);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables different experiments that modify content and behavior of the
// existing generated password suggestion dropdown.
BASE_DECLARE_FEATURE(kPasswordGenerationExperiment);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

// Enables triggering password suggestions through the context menu.
BASE_DECLARE_FEATURE(kPasswordManualFallbackAvailable);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Enables promo card in settings encouraging users to enable screenlock reauth
// before filling passwords.
BASE_DECLARE_FEATURE(kScreenlockReauthPromoCard);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

// Starts passwords resync after undecryptable passwords were removed. This flag
// is enabled by default and should be treaded as a killswitch.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncAfterDeletingUndecryptablePasswords);

#if BUILDFLAG(IS_ANDROID)

// Enables showing various warnings for password manager users not yet enrolled
// into the new experience of storing passwords in GMSCore.
BASE_DECLARE_FEATURE(
    kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);

// Enables use of Google Mobile services for non-synced password storage that
// contains no passwords, so no migration will be necessary.
// UnifiedPasswordManagerLocalPasswordsAndroidWithMigration will replace this
// feature once UPM starts to be rolled out to users who have saved local
// passwords.
// See also kLocalUpmMinGmsVersionParam below.
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration);

// Enables use of Google Mobile services for non-synced password storage add for
// users who have local passwords saved.
// See also kLocalUpmMinGmsVersionParam below.
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration);

// Helper function which returns the delay when the local passwords migration is
// triggered after Chrome startup in seconds.
int GetLocalPasswordsMigrationToAndroidBackendDelay();

// Enables UPM M4 that no longer needs Password sync engine to sync passwords.
BASE_DECLARE_FEATURE(kUnifiedPasswordManagerSyncOnlyInGMSCore);

// Enables clearing the login database for the users who already migrated their
// credentials to GMS Core.
BASE_DECLARE_FEATURE(kClearLoginDatabaseForAllMigratedUPMUsers);

// This feature clears login database if user is capable of using UPM.
BASE_DECLARE_FEATURE(kClearLoginDatabaseForUPMUsers);

// A parameter for both the NoMigration and WithMigration features above. It
// dictates the min value of base::android::BuildInfo::gms_version_code() for
// the flag take effect.
inline constexpr char kLocalUpmMinGmsVersionParam[] = "min_gms_version";
// Default value of kLocalUpmMinGmsVersionParam.
inline constexpr int kDefaultLocalUpmMinGmsVersion = 240212000;
// The min GMS version, which supports UPM for syncing users.
inline constexpr int kAccountUpmMinGmsVersion = 223012000;

// Same as above, but for automotive.
//
// IMPORTANT: as the flags have been enabled by default, this is now the only
// feature guard remaining on automotive!
inline constexpr char kLocalUpmMinGmsVersionParamForAuto[] =
    "min_gms_version_for_auto";
inline constexpr int kDefaultLocalUpmMinGmsVersionForAuto = 241512000;
// Helper function returning the status of
// `UnifiedPasswordManagerSyncOnlyInGMSCore`.
bool IsUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled();

#endif  // !BUILDFLAG(IS_ANDROID)

// Improves PSL matching capabilities by utilizing PSL-extension list from
// affiliation service. It fixes problem with incorrect password suggestions on
// websites like slack.com.
BASE_DECLARE_FEATURE(kUseExtensionListForPSLMatching);

// Enables support of sending additional votes on username first flow. The votes
// are sent on single password forms and contain information about preceding
// single username forms.
// TODO(crbug.com/40626063): Clean up if the main crowdsourcing is good enough
// and we don't need additional signals.
BASE_DECLARE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing);

// Enables storing more possible username values in the LRU cache. Part of the
// `kUsernameFirstFlowWithIntermediateValues` feature.
BASE_DECLARE_FEATURE(kUsernameFirstFlowStoreSeveralValues);

// If `kUsernameFirstFlowStoreSeveralValues` is enabled, the size of LRU
// cache that stores all username candidates outside the form.
extern const base::FeatureParam<int> kMaxSingleUsernameFieldsToStore;

// Enables tolerating intermediate fields like OTP or CAPTCHA
// between username and password fields in Username First Flow.
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValues);

// If `kUsernameFirstFlowWithIntermediateValues` is enabled, after this amount
// of minutes single username will not be used in the save prompt.
extern const base::FeatureParam<int> kSingleUsernameTimeToLive;

// Enables new prediction that is based on votes from Username First Flow with
// Intermediate Values.
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValuesPredictions);

// Enables voting for more text fields outside of the password form in Username
// First Flow.
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValuesVoting);

// Enables async implementation of OSCrypt inside LoginDatabase.
BASE_DECLARE_FEATURE(kUseAsyncOsCryptInLoginDatabase);

// All features parameters in alphabetical order.

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// This enum supports enabling specific arms of the
// `kPasswordGenerationExperiment` (go/strong-passwords-desktop).
// Keep the order consistent with
// `kPasswordGenerationExperimentVariationOption` below and with
// `kPasswordGenerationExperimentVariations` in about_flags.cc.
enum class PasswordGenerationVariation {
  // Adjusts the language focusing on recommendation and security messaging.
  kTrustedAdvice = 1,
  // Adjusts the language making the suggestion softer and more guiding.
  kSafetyFirst = 2,
  // Adjusts the language adding a more persuasive and reassuring tone.
  kTrySomethingNew = 3,
  // Adjusts the language focusing on the convenience of use.
  kConvenience = 4,
  // Adjusts the language of the help text pointing out the benefits.
  kCrossDevice = 5,
  // Adds a row for switching to editing the suggested password directly.
  kEditPassword = 6,
  // Adds chunking generated passwords into smaller readable parts.
  kChunkPassword = 7,
  // Removes strong password row and adds nudge passwords buttons instead.
  kNudgePassword = 8,
};

inline constexpr base::FeatureParam<PasswordGenerationVariation>::Option
    kPasswordGenerationExperimentVariationOption[] = {
        {PasswordGenerationVariation::kTrustedAdvice, "trusted_advice"},
        {PasswordGenerationVariation::kSafetyFirst, "safety_first"},
        {PasswordGenerationVariation::kTrySomethingNew, "try_something_new"},
        {PasswordGenerationVariation::kConvenience, "convenience"},
        {PasswordGenerationVariation::kCrossDevice, "cross_device"},
        {PasswordGenerationVariation::kEditPassword, "edit_password"},
        {PasswordGenerationVariation::kChunkPassword, "chunk_password"},
        {PasswordGenerationVariation::kNudgePassword, "nudge_password"},
};

inline constexpr base::FeatureParam<PasswordGenerationVariation>
    kPasswordGenerationExperimentVariationParam{
        &kPasswordGenerationExperiment, "password_generation_variation",
        PasswordGenerationVariation::kTrustedAdvice,
        &kPasswordGenerationExperimentVariationOption};

inline constexpr base::FeatureParam<std::string>
    kPasswordGenerationExperimentSurveyTriggerId{
        &kPasswordGenerationExperiment,
        "PasswordGenerationExperimentSurveyTriggedId", /*default_value=*/""};

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
