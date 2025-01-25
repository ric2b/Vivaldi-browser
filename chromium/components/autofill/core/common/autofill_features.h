// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace autofill::features {

// All features in alphabetical order.
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillAcrossIframesIos);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillGivePrecedenceToNumericQuantities);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAddressProfileSavePromptNicknameSupport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAddressUserPerceptionSurvey);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCaretExtraction);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillContentEditableLeftClickFix);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCreditCardUserPerceptionSurvey);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillAssociateForms);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<base::TimeDelta> kAutofillAssociateFormsTTL;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPreferParsedPhoneNumber);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillInferCountryCallingCode);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillConsiderPhoneNumberSeparatorsValidLabels);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDefaultToCityAndNumber);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDetectFieldVisibility);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisambiguateContradictingFieldTypes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDontPrefixMatchCreditCardNumbersOrCvcs);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDontPreserveAutofillState);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableAccountStorageForIneligibleCountries);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableAddressFieldParserNG);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableManualFallbackIPH);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForBetweenStreets);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForAdminLevel2);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForAddressOverflow);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForAddressOverflowAndLandmark);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForBetweenStreetsOrLandmark);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForLandmark);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableParsingOfStreetLocation);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableRationalizationEngineForMX);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableFilling);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableAddressImport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableShadowHeuristics);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableDependentLocalityParsing);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableExpirationDateImprovements);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableRankingFormulaAddressProfiles);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaAddressProfilesUsageHalfLife;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableRankingFormulaCreditCards);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaCreditCardsUsageHalfLife;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoost;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillRankingFormulaVirtualCardBoostHalfLife;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableEmailHeuristicOnlyAddressForms);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillEnableEmailHeuristicAutocompleteEmail;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForApartmentNumbers);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSupportForPhoneNumberTrunkTypes);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillExtractOnlyNonAdFrames);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableXHRSubmissionDetectionIOS);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAddressFieldSwapping);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillFixCachingOnJavaScriptChanges);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillNewFocusEvents);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDontUpdateLastQueriedElementOnFill);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillFindCachedFieldsByIdOnly);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSuggestionNStrikeModel);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kSuggestionStrikeLimit;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillChangeDisusedAddressSuggestionTreatment);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int> kNumberOfIgnoredSuggestions;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUnifyAndFixFormTracking);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillReplaceFormElementObserver);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDetectRemovedFormControls);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillReplaceCachedWebElementsByRendererIds);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseI18nAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseAUAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseBRAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseCAAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseDEAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseFRAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseINAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseITAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUseMXAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUsePLAddressModel);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUploadVotesForFieldsWithEmail);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillLocalHeuristicsOverrides);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillModelPredictions);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillModelPredictionsAreActive;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillOverwritePlaceholdersOnly);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSkipPreFilledFields);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillPageLanguageDetection);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillParsingPatternProvider);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillParsingPatternActiveSource;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAlwaysParsePlaceholders);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPopupDisablePaintChecks);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPopupMeasureTimeAfterPaint);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPopupZOrderSecuritySurface);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillPreferLabelsInSomeCountries);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillSharedAutofill);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillStructuredFieldsDisableAddressLines);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillGranularFillingAvailable);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillGranularFillingAvailableWithImprovedLabelsParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillGranularFillingAvailableWithFillEverythingAtTheBottomParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillGranularFillingAvailableWithExpandControlVisibleOnSelectionOnly;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillForUnclassifiedFieldsAvailable);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillTestFormWithTestAddresses);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillContentEditableChangeEvents);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kComposePopupAnnouncePolitely);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillEnableAblationStudy);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForAddressesParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool>
    kAutofillAblationStudyEnabledForPaymentsParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList1Param;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList2Param;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList3Param;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList4Param;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList5Param;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleList6Param;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillAblationStudyAblationWeightPerMilleParam;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<bool> kAutofillAblationStudyIsDryRun;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(
    kAutofillEnableFillingPhoneCountryCodesByAddressCountryCodes);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillMoreProminentPopupMaxOffsetToCenterParam;
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillMoreProminentPopup);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillLogUKMEventsWithSamplingOnSession);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillLogUKMEventsWithSamplingOnSessionRate;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillAndroidDisableSuggestionsOnJSFocus);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableCacheForRegexMatching);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<int>
    kAutofillEnableCacheForRegexMatchingCacheSizeParam;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillLogDeduplicationMetrics);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillSilentlyRemoveQuasiDuplicates);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUpdateLowQualityTokenOnImport);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillUKMExperimentalFields);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillUKMExperimentalFieldsBucket0;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillUKMExperimentalFieldsBucket1;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillUKMExperimentalFieldsBucket2;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillUKMExperimentalFieldsBucket3;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillUKMExperimentalFieldsBucket4;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillEnableSecurityTouchEventFilteringAndroid);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillVirtualViewStructureAndroid);

// Used as param for `kAutofillVirtualViewStructureAndroid` to allow
// skipping certain checks when testing manually.
enum class VirtualViewStructureSkipChecks {
  kDontSkip = 0,
  kSkipAllChecks = 1,
  kOnlySkipAwGCheck = 2,
};

inline constexpr base::FeatureParam<VirtualViewStructureSkipChecks>::Option
    kVirtualViewStructureSkipChecksOption[] = {
        {VirtualViewStructureSkipChecks::kDontSkip, "dont_skip"},
        {VirtualViewStructureSkipChecks::kSkipAllChecks, "skip_all_checks"},
        {VirtualViewStructureSkipChecks::kOnlySkipAwGCheck,
         "only_skip_awg_check"},
};
inline constexpr base::FeatureParam<VirtualViewStructureSkipChecks>
    kAutofillVirtualViewStructureAndroidSkipsCompatibilityCheck{
        &kAutofillVirtualViewStructureAndroid, "skip_compatibility_check",
        VirtualViewStructureSkipChecks::kDontSkip,
        &kVirtualViewStructureSkipChecksOption};

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_APPLE)
// Returns true if whether the views autofill popup feature is enabled or the
// we're using the views browser.
COMPONENT_EXPORT(AUTOFILL)
bool IsMacViewsAutofillPopupExperimentEnabled();
#endif  // BUILDFLAG(IS_APPLE)

// The features in this namespace contains are not meant to be rolled out. They
// are are only intended for manual testing purposes.
namespace test {

COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCapturedSiteTestsMetricsScraper);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillCapturedSiteTestsMetricsScraperOutputDir;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillCapturedSiteTestsMetricsScraperHistogramRegex;
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillCapturedSiteTestsUseAutofillFlow);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillDisableProfileUpdates);
COMPONENT_EXPORT(AUTOFILL)
BASE_DECLARE_FEATURE(kAutofillDisableSilentProfileUpdates);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillLogToTerminal);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillOverridePredictions);
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillOverridePredictionsSpecification;
COMPONENT_EXPORT(AUTOFILL)
extern const base::FeatureParam<std::string>
    kAutofillOverridePredictionsForAlternativeFormSignaturesSpecification;
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillServerCommunication);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillShowTypePredictions);
COMPONENT_EXPORT(AUTOFILL) BASE_DECLARE_FEATURE(kAutofillUploadThrottling);

}  // namespace test

}  // namespace autofill::features

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_FEATURES_H_
