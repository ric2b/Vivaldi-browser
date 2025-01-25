// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_feature_list.h"

#include <stddef.h>

#include <string>

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "chrome/browser/android/webapk/webapk_features.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/browsing_data/core/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_settings/core/common/features.h"
#include "components/contextual_search/core/browser/contextual_search_field_trial.h"
#include "components/download/public/common/download_features.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/history/core/browser/features.h"
#include "components/history_clusters/core/features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/messages/android/messages_feature.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/core/features.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/permissions/features.h"
#include "components/plus_addresses/features.h"
#include "components/policy/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/query_tiles/switches.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/saved_tab_groups/features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/segmentation_platform/public/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync/base/features.h"
#include "components/sync_sessions/features.h"
#include "components/visited_url_ranking/public/features.h"
#include "components/viz/common/features.h"
#include "components/webapps/browser/features.h"
#include "content/public/common/content_features.h"
#include "device/fido/features.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/flags/jni_headers/ChromeFeatureMap_jni.h"

namespace chrome {
namespace android {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. chrome/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &autofill::features::kAutofillAddressProfileSavePromptNicknameSupport,
    &autofill::features::kAutofillEnableRankingFormulaAddressProfiles,
    &autofill::features::kAutofillEnableRankingFormulaCreditCards,
    &autofill::features::kAutofillEnableNewCardArtAndNetworkImages,
    &autofill::features::kAutofillEnableCardArtServerSideStretching,
    &autofill::features::kAutofillEnableVirtualCardMetadata,
    &autofill::features::kAutofillEnableCardArtImage,
    &autofill::features::kAutofillEnableCardBenefitsForAmericanExpress,
    &autofill::features::kAutofillEnableCardBenefitsForCapitalOne,
    &autofill::features::kAutofillEnableCardProductName,
    &autofill::features::kAutofillEnableLocalIban,
    &autofill::features::kAutofillEnableSecurityTouchEventFilteringAndroid,
    &autofill::features::kAutofillEnableVerveCardSupport,
    &autofill::features::kAutofillVirtualViewStructureAndroid,
    &autofill::features::kAutofillEnableMovingGPayLogoToTheRightOnClank,
    &autofill::features::kAutofillEnableCvcStorageAndFilling,
    &autofill::features::kAutofillEnableSaveCardLoadingAndConfirmation,
    &autofill::features::kAutofillEnableSyncingOfPixBankAccounts,
    &autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation,
    &blink::features::kBackForwardTransitions,
    &blink::features::kForceWebContentsDarkMode,
    &blink::features::kPrerender2,
    &browsing_data::features::kBrowsingDataModel,
    &commerce::kCommerceMerchantViewer,
    &commerce::kCommercePriceTracking,
    &commerce::kPriceInsights,
    &commerce::kShoppingList,
    &commerce::kShoppingPDPMetrics,
    &content_settings::kDarkenWebsitesCheckboxInThemesSetting,
    &content_settings::features::kTrackingProtection3pcd,
    &content_settings::features::kUserBypassUI,
    &device::kWebAuthnEnableAndroidCableAuthenticator,
    &download::features::kSmartSuggestionForLargeDownloads,
    &download::features::kDownloadsMigrateToJobsAPI,
    &base::features::kCollectAndroidFrameTimelineMetrics,
    &download::features::kDownloadNotificationServiceUnifiedAPI,
    &features::kAndroidBrowserControlsInViz,
    &features::kGenericSensorExtraClasses,
    &features::kBackForwardCache,
    &features::kBoardingPassDetector,
    &features::kNetworkServiceInProcess,
    &features::kElasticOverscroll,
    &features::kLinkedServicesSetting,
    &features::kNotificationOneTapUnsubscribe,
    &features::kPrivacyGuideAndroid3,
    &features::kPrivacyGuidePreloadAndroid,
    &features::kPrefetchBrowserInitiatedTriggers,
    &features::kPushMessagingDisallowSenderIDs,
    &features::kPwaUpdateDialogForIcon,
    &features::kSafetyHub,
    &features::kSafetyHubMagicStack,
    &features::kQuietNotificationPrompts,
    &features::kWebNfc,
    &feature_engagement::kIPHTabSwitcherButtonFeature,
    &feed::kFeedContainment,
    &feed::kFeedDynamicColors,
    &feed::kFeedFollowUiUpdate,
    &feed::kFeedImageMemoryCacheSizePercentage,
    &feed::kFeedLoadingPlaceholder,
    &feed::kFeedNoViewCache,
    &feed::kFeedPerformanceStudy,
    &feed::kFeedShowSignInCommand,
    &feed::kFeedSignedOutViewDemotion,
    &feed::kInterestFeedV2,
    &feed::kInterestFeedV2Hearts,
    &feed::kWebFeedAwareness,
    &feed::kWebFeedOnboarding,
    &feed::kWebFeedSort,
    &feed::kXsurfaceMetricsReporting,
    &history::kOrganicRepeatableQueries,
    &history_clusters::internal::kJourneys,
    &history_clusters::internal::kOmniboxAction,
    &kAdaptiveButtonInTopToolbarTranslate,
    &kAdaptiveButtonInTopToolbarAddToBookmarks,
    &kAdaptiveButtonInTopToolbarCustomizationV2,
    &kAddToHomescreenIPH,
    &kRedirectExplicitCTAIntentsToExistingActivity,
    &kAllowNewIncognitoTabIntents,
    &kAndroidAppIntegration,
    &kAndroidElegantTextHeight,
    &kAndroidGoogleSansText,
    &kAndroidHatsRefactor,
    &kAndroidHubFloatingActionButton,
    &kAndroidHubV2,
    &kAndroidImprovedBookmarks,
    &kAndroidNoVisibleHintForDifferentTLD,
    &kAndroidTabDeclutter,
    &kAndroidTabDeclutterArchiveAllButActiveTab,
    &kAndroidTabDeclutterRescueKillswitch,
    &kAndroidToolbarScrollAblation,
    &kAnimatedImageDragShadow,
    &kAppSpecificHistory,
    &kArchiveTabService,
    &kAsyncNotificationManager,
    &kAuxiliarySearchDonation,
    &kAvoidSelectedTabFocusOnLayoutDoneShowing,
    &kBackGestureActivityTabProvider,
    &kBackGestureMoveToBackDuringStartup,
    &kBackGestureRefactorAndroid,
    &kBackToHomeAnimation,
    &kBackgroundThreadPool,
    &kBlockIntentsWhileLocked,
    &kBottomBrowserControlsRefactor,
    &kBrowserControlsEarlyResize,
    &kCacheActivityTaskID,
    &kCastDeviceFilter,
    &kCCTAuthView,
    &kCCTBeforeUnload,
    &kCCTClientDataHeader,
    &kCCTExtendTrustedCdnPublisher,
    &kCCTFeatureUsage,
    &kCCTEphemeralMode,
    &kCCTIncognitoAvailableToThirdParty,
    &kCCTIntentFeatureOverrides,
    &kCCTMinimized,
    &kCCTMinimizedEnabledByDefault,
    &kCCTNavigationalPrefetch,
    &kCCTNestedSecurityIcon,
    &kCCTPageInsightsHub,
    &kCCTPageInsightsHubPeek,
    &kCCTPageInsightsHubBetterScroll,
    &kCCTGoogleBottomBar,
    &kCCTGoogleBottomBarVariantLayouts,
    &kCCTPrewarmTab,
    &kCCTReportParallelRequestStatus,
    &kCCTResizableForThirdParties,
    &kCCTRevampedBranding,
    &kCCTTabModalDialog,
    &kDataSharingAndroid,
    &kDefaultBrowserPromoAndroid,
    &kDontAutoHideBrowserControls,
    &kCacheDeprecatedSystemLocationSetting,
    &kChromeSharePageInfo,
    &kChromeSurveyNextAndroid,
    &kCommandLineOnNonRooted,
    &kContextMenuTranslateWithGoogleLens,
    &kContextMenuSysUiMatchesActivity,
    &kContextualSearchDisableOnlineDetection,
    &kContextualSearchSuppressShortView,
    &kDelayTempStripRemoval,
    &kDeviceAuthenticatorAndroidx,
    &kDragDropIntoOmnibox,
    &kDragDropTabTearing,
    &kDragDropTabTearingEnableOEM,
    &kDrawEdgeToEdge,
    &kDrawKeyNativeEdgeToEdge,
    &kDrawNativeEdgeToEdge,
    &kDrawWebEdgeToEdge,
    &kEdgeToEdgeBottomChin,
    &kEducationalTipModule,
    &kExperimentsForAgsa,
    &kFeedPositionAndroid,
    &kFocusOmniboxInIncognitoTabIntents,
    &kForceListTabSwitcher,
    &kFullscreenInsetsApiMigration,
    &kFullscreenInsetsApiMigrationOnAutomotive,
    &kGcmNativeBackgroundTask,
    &kGtsCloseTabAnimation,
    &kIncognitoReauthenticationForAndroid,
    &kIncognitoScreenshot,
    &kLensOnQuickActionSearchWidget,
    &kLogoPolish,
    &kLogoPolishAnimationKillSwitch,
    &kMagicStackAndroid,
    &kMayLaunchUrlUsesSeparateStoragePartition,
    &kMostVisitedTilesSelectExistingTab,
    &kMultiInstanceApplicationStatusCleanup,
    &kNavBarColorMatchesTabBackground,
    &kNewTabSearchEngineUrlAndroid,
    &kNewTabPageAndroidTriggerForPrerender2,
    &kNotificationPermissionVariant,
    &kNotificationPermissionBottomSheet,
    &kTinkerTankBottomSheet,
    &kPageAnnotationsService,
    &kPreconnectOnTabCreation,
    &kPriceChangeModule,
    &kPwaRestoreUi,
    &kPwaRestoreUiAtStartup,
    &kOmahaMinSdkVersionAndroid,
    &kShortCircuitUnfocusAnimation,
    &kPartnerCustomizationsUma,
    &kQuickDeleteForAndroid,
    &kQuickDeleteAndroidFollowup,
    &kQuickDeleteAndroidSurvey,
    &kReadAloud,
    &kReadAloudInOverflowMenuInCCT,
    &kReadAloudInMultiWindow,
    &kReadAloudBackgroundPlayback,
    &kReadAloudPlayback,
    &kReadAloudTapToSeek,
    &kReadAloudIPHMenuButtonHighlightCCT,
    &kReaderModeInCCT,
    &kRecordSuppressionMetrics,
    &kReengagementNotification,
    &kRelatedSearchesAllLanguage,
    &kReportParentalControlSitesChild,
    &kSearchInCCT,
    &kSearchResumptionModuleAndroid,
    &kShareCustomActionsInCCT,
    &kSmallerTabStripTitleLimit,
    &kSuppressToolbarCaptures,
    &kSuppressToolbarCapturesAtGestureEnd,
    &kTabDragDropAndroid,
    &kTabGroupCreationDialogAndroid,
    &kTabGroupParityAndroid,
    &kTabletTabSwitcherLongPressMenu,
    &kTabletToolbarReordering,
    &kTabResumptionModuleAndroid,
    &kTabStateFlatBuffer,
    &kTabStripGroupCollapseAndroid,
    &kTabStripGroupContextMenuAndroid,
    &kTabStripGroupIndicatorsAndroid,
    &kTabStripLayoutOptimization,
    &kTabStripStartupRefactoring,
    &kTabStripTransitionInDesktopWindow,
    &kTabWindowManagerIndexReassignmentActivityFinishing,
    &kTabWindowManagerIndexReassignmentActivityInSameTask,
    &kTabWindowManagerIndexReassignmentActivityNotInAppTasks,
    &kTabWindowManagerReportIndicesMismatch,
    &kTestDefaultDisabled,
    &kTestDefaultEnabled,
    &kStartSurfaceReturnTime,
    &kAccountReauthenticationRecentTimeWindow,
    &kSurfacePolish,
    &kSurfacePolishForToolbarKillSwitch,
    &kUmaBackgroundSessions,
    &kUseLibunwindstackNativeUnwinderAndroid,
    &kVerticalAutomotiveBackButtonToolbar,
    &kVoiceSearchAudioCapturePolicy,
    &kWebOtpCrossDeviceSimpleString,
    &kWebApkAllowIconUpdate,
    &kWebApkMinShellVersion,
    &features::kCookieDeprecationFacilitatedTesting,
    &notifications::features::kUseChimeAndroidSdk,
    &paint_preview::kPaintPreviewDemo,
    &language::kCctAutoTranslate,
    &language::kDetailedLanguageSettings,
    &messages::kMessagesForAndroidSaveCard,
    &omnibox::kUpdatedConnectionSecurityIndicators,
    &optimization_guide::features::kPushNotifications,
    &page_info::kPageInfoAboutThisSiteMoreLangs,
    &password_manager::features::kBiometricTouchToFill,
    &password_manager::features::
        kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning,
    &password_manager::features::
        kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
    &password_manager::features::
        kUnifiedPasswordManagerLocalPasswordsMigrationWarning,
    &permissions::features::kPermissionsPromptSurvey,
    &permissions::features::kPermissionDedicatedCpssSettingAndroid,
    &plus_addresses::features::kPlusAddressesEnabled,
    &privacy_sandbox::kFingerprintingProtectionSetting,
    &privacy_sandbox::kIpProtectionV1,
    &privacy_sandbox::kIpProtectionUx,
    &privacy_sandbox::kPrivacySandboxActivityTypeStorage,
    &privacy_sandbox::kPrivacySandboxAdsNoticeCCT,
    &privacy_sandbox::kPrivacySandboxFirstPartySetsUI,
    &privacy_sandbox::kPrivacySandboxRelatedWebsiteSetsUi,
    &privacy_sandbox::kPrivacySandboxSettings4,
    &privacy_sandbox::kPrivacySandboxPrivacyGuideAdTopics,
    &privacy_sandbox::kPrivacySandboxProactiveTopicsBlocking,
    &privacy_sandbox::kTrackingProtectionFullOnboardingMobileTrigger,
    &privacy_sandbox::kTrackingProtectionSettingsLaunch,
    &privacy_sandbox::kTrackingProtectionUserBypassPwa,
    &privacy_sandbox::kTrackingProtectionUserBypassPwaTrigger,
    &query_tiles::features::kQueryTiles,
    &safe_browsing::kFriendlierSafeBrowsingSettingsEnhancedProtection,
    &safe_browsing::kFriendlierSafeBrowsingSettingsStandardProtection,
    &safe_browsing::kHashPrefixRealTimeLookups,
    &safe_browsing::kSafeBrowsingCallNewGmsApiOnStartup,
    &safe_browsing::kSafeBrowsingNewGmsApiForBrowseUrlDatabaseCheck,
    &safe_browsing::kSafeBrowsingNewGmsApiForSubresourceFilterCheck,
    &segmentation_platform::features::kContextualPageActions,
    &segmentation_platform::features::kContextualPageActionReaderMode,
    &segmentation_platform::features::kContextualPageActionShareModel,
    &segmentation_platform::features::
        kSegmentationPlatformAndroidHomeModuleRanker,
    &segmentation_platform::features::
        kSegmentationPlatformAndroidHomeModuleRankerV2,
    &send_tab_to_self::kSendTabToSelfV2,
    &supervised_user::kKidFriendlyContentFeed,
    &supervised_user::kReplaceProfileIsChildWithAccountCapabilitiesOnAndroid,
    &switches::kForceStartupSigninPromo,
    &switches::kForceDisableExtendedSyncPromos,
    &switches::kSearchEngineChoice,
    &switches::kPersistentSearchEngineChoiceImport,
    &switches::kSearchEnginePromoDialogRewrite,
    &switches::kSeedAccountsRevamp,
    &sync_sessions::kOptimizeAssociateWindowsAndroid,
    &syncer::kEnableBatchUploadFromSettings,
    &syncer::kEnablePasswordsAccountStorageForNonSyncingUsers,
    &syncer::kReadingListEnableSyncTransportModeUponSignIn,
    &syncer::kReplaceSyncPromosWithSignInPromos,
    &syncer::kSyncAndroidLimitNTPPromoImpressions,
    &syncer::kSyncEnableContactInfoDataTypeInTransportMode,
    &syncer::kWebApkBackupAndRestoreBackend,
    &tab_groups::kAndroidTabGroupStableIds,
    &tab_groups::kTabGroupSyncAndroid,
    &tab_groups::kTabGroupPaneAndroid,
    &tab_groups::kTabGroupSyncAutoOpenKillSwitch,
    &webapps::features::kPwaUniversalInstallUi,
    &visited_url_ranking::features::kVisitedURLRankingService,
    &webapps::features::kWebApkInstallFailureNotification,
    &network::features::kPrivateStateTokens,
    &switches::kPersistentSearchEngineChoiceImport,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_ChromeFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

// Alphabetical:

BASE_FEATURE(kAdaptiveButtonInTopToolbarTranslate,
             "AdaptiveButtonInTopToolbarTranslate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAdaptiveButtonInTopToolbarAddToBookmarks,
             "AdaptiveButtonInTopToolbarAddToBookmarks",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAdaptiveButtonInTopToolbarCustomizationV2,
             "AdaptiveButtonInTopToolbarCustomizationV2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAddToHomescreenIPH,
             "AddToHomescreenIPH",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAllowNewIncognitoTabIntents,
             "AllowNewIncognitoTabIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAvoidSelectedTabFocusOnLayoutDoneShowing,
             "AvoidSelectedTabFocusOnLayoutDoneShowing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFocusOmniboxInIncognitoTabIntents,
             "FocusOmniboxInIncognitoTabIntents",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Long-term flag for debugging only.
BASE_FEATURE(kForceListTabSwitcher,
             "ForceListTabSwitcher",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidAppIntegration,
             "AndroidAppIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidElegantTextHeight,
             "AndroidElegantTextHeight",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidGoogleSansText,
             "AndroidGoogleSansText",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidHatsRefactor,
             "AndroidHatsRefactor",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidHubFloatingActionButton,
             "AndroidHubFloatingActionButton",
             base::FEATURE_ENABLED_BY_DEFAULT); // Vivaldi

BASE_FEATURE(kAndroidHubV2, "AndroidHubV2", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidImprovedBookmarks,
             "AndroidImprovedBookmarks",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidNoVisibleHintForDifferentTLD,
             "AndroidNoVisibleHintForDifferentTLD",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutter,
             "AndroidTabDeclutter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterArchiveAllButActiveTab,
             "AndroidTabDeclutterArchiveAllButActiveTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidTabDeclutterRescueKillswitch,
             "AndroidTabDeclutterRescueKillswitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAndroidToolbarScrollAblation,
             "AndroidToolbarScrollAblation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAnimatedImageDragShadow,
             "AnimatedImageDragShadow",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAppSpecificHistory,
             "AppSpecificHistory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kArchiveTabService,
             "ArchiveTabService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAsyncNotificationManager,
             "AsyncNotificationManager",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAuxiliarySearchDonation,
             "AuxiliarySearchDonation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTinkerTankBottomSheet,
             "TinkerTankBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackgroundThreadPool,
             "BackgroundThreadPool",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBlockIntentsWhileLocked,
             "BlockIntentsWhileLocked",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBottomBrowserControlsRefactor,
             "BottomBrowserControlsRefactor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserControlsEarlyResize,
             "BrowserControlsEarlyResize",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCacheActivityTaskID,
             "CacheActivityTaskID",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used in downstream code.
BASE_FEATURE(kCastDeviceFilter,
             "CastDeviceFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTAuthView, "CCTAuthView", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTBeforeUnload,
             "CCTBeforeUnload",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTClientDataHeader,
             "CCTClientDataHeader",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTEphemeralMode,
             "CCTEphemeralMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTExtendTrustedCdnPublisher,
             "CCTExtendTrustedCdnPublisher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTFeatureUsage,
             "CCTFeatureUsage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIncognitoAvailableToThirdParty,
             "CCTIncognitoAvailableToThirdParty",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTIntentFeatureOverrides,
             "CCTIntentFeatureOverrides",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTMinimized, "CCTMinimized", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTMinimizedEnabledByDefault,
             "CCTMinimizedEnabledByDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTNavigationalPrefetch,
             "CCTNavigationalPrefetch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTNestedSecurityIcon,
             "CCTNestedSecurityIcon",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPageInsightsHub,
             "CCTPageInsightsHub",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPageInsightsHubPeek,
             "CCTPageInsightsHubPeek",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPageInsightsHubBetterScroll,
             "CCTPageInsightsHubBetterScroll",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTGoogleBottomBar,
             "CCTGoogleBottomBar",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTGoogleBottomBarVariantLayouts,
             "CCTGoogleBottomBarVariantLayouts",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTPrewarmTab,
             "CCTPrewarmTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTReportParallelRequestStatus,
             "CCTReportParallelRequestStatus",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTResizableForThirdParties,
             "CCTResizableForThirdParties",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCTRevampedBranding,
             "CCTRevampedBranding",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCCTTabModalDialog,
             "CCTTabModalDialog",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDontAutoHideBrowserControls,
             "DontAutoHideBrowserControls",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCacheDeprecatedSystemLocationSetting,
             "CacheDeprecatedSystemLocationSetting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeSharePageInfo,
             "ChromeSharePageInfo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kChromeSurveyNextAndroid,
             "ChromeSurveyNextAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCommandLineOnNonRooted,
             "CommandLineOnNonRooted",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuSysUiMatchesActivity,
             "ContextMenuSysUiMatchesActivity",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextMenuTranslateWithGoogleLens,
             "ContextMenuTranslateWithGoogleLens",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOnQuickActionSearchWidget,
             "LensOnQuickActionSearchWidget",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchDisableOnlineDetection,
             "ContextualSearchDisableOnlineDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kContextualSearchSuppressShortView,
             "ContextualSearchSuppressShortView",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDataSharingAndroid,
             "DataSharingAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDefaultBrowserPromoAndroid,
             "DefaultBrowserPromoAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDelayTempStripRemoval,
             "DelayTempStripRemoval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The feature is a no-op, it replaces android.hardware.biometrics library on
// Android with androidx.biometric.
BASE_FEATURE(kDeviceAuthenticatorAndroidx,
             "DeviceAuthenticatorAndroidx",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadAutoResumptionThrottling,
             "DownloadAutoResumptionThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDragDropIntoOmnibox,
             "DragDropIntoOmnibox",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDragDropTabTearing,
             "DragDropTabTearing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDragDropTabTearingEnableOEM,
             "DragDropTabTearingEnableOEM",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDrawEdgeToEdge,
             "DrawEdgeToEdge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDrawKeyNativeEdgeToEdge,
             "DrawKeyNativeEdgeToEdge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDrawNativeEdgeToEdge,
             "DrawNativeEdgeToEdge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDrawWebEdgeToEdge,
             "DrawWebEdgeToEdge",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEdgeToEdgeBottomChin,
             "EdgeToEdgeBottomChin",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEducationalTipModule,
             "EducationalTipModule",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExperimentsForAgsa,
             "ExperimentsForAgsa",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenInsetsApiMigration,
             "FullscreenInsetsApiMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenInsetsApiMigrationOnAutomotive,
             "FullscreenInsetsApiMigrationOnAutomotive",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(b/41490045): This flag should be cleaned up as soon as there is enough
// data to prove that this reduces ANRs and doesn't significantly regress
// notifications.
BASE_FEATURE(kGcmNativeBackgroundTask,
             "GcmNativeBackgroundTask",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Will likely rollout with waterfall and be used as a killswitch, but is
// default disabled for now until the animation is polished.
BASE_FEATURE(kGtsCloseTabAnimation,
             "GtsCloseTabAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoReauthenticationForAndroid,
             "IncognitoReauthenticationForAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIncognitoScreenshot,
             "IncognitoScreenshot",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLogoPolish, "LogoPolish", base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogoPolishAnimationKillSwitch,
             "LogoPolishAnimationKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMagicStackAndroid,
             "MagicStackAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables an experimental feature which forces mayLaunchUrl to use a different
// storage partition. This may reduce performance. This should not be enabled by
// default.
BASE_FEATURE(kMayLaunchUrlUsesSeparateStoragePartition,
             "MayLaunchUrlUsesSeparateStoragePartition",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMostVisitedTilesSelectExistingTab,
             "MostVisitedTilesSelectExistingTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMultiInstanceApplicationStatusCleanup,
             "MultiInstanceApplicationStatusCleanup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNavBarColorMatchesTabBackground,
             "NavBarColorMatchesTabBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabSearchEngineUrlAndroid,
             "NewTabSearchEngineUrlAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageAndroidTriggerForPrerender2,
             "NewTabPageAndroidTriggerForPrerender2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationPermissionVariant,
             "NotificationPermissionVariant",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationPermissionBottomSheet,
             "NotificationPermissionBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPageAnnotationsService,
             "PageAnnotationsService",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreconnectOnTabCreation,
             "PreconnectOnTabCreation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPriceChangeModule,
             "PriceChangeModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPwaRestoreUi, "PwaRestoreUi", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPwaRestoreUiAtStartup,
             "PwaRestoreUiAtStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackGestureActivityTabProvider,
             "BackGestureActivityTabProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBackGestureMoveToBackDuringStartup,
             "BackGestureMoveToBackDuringStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBackGestureRefactorAndroid,
             "BackGestureRefactorAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBackToHomeAnimation,
             "BackToHomeAnimation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOmahaMinSdkVersionAndroid,
             "OmahaMinSdkVersionAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShortCircuitUnfocusAnimation,
             "ShortCircuitUnfocusAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPartnerCustomizationsUma,
             "PartnerCustomizationsUma",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQuickDeleteForAndroid,
             "QuickDeleteForAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kQuickDeleteAndroidFollowup,
             "QuickDeleteAndroidFollowup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQuickDeleteAndroidSurvey,
             "QuickDeleteAndroidSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloud, "ReadAloud", base::FEATURE_DISABLED_BY_DEFAULT); // Vivaldi

BASE_FEATURE(kReadAloudInOverflowMenuInCCT,
             "ReadAloudInOverflowMenuInCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudInMultiWindow,
             "ReadAloudInMultiWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudBackgroundPlayback,
             "ReadAloudBackgroundPlayback",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudPlayback,
             "ReadAloudPlayback",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudTapToSeek,
             "ReadAloudTapToSeek",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReadAloudIPHMenuButtonHighlightCCT,
             "ReadAloudIPHMenuButtonHighlightCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeInCCT,
             "ReaderModeInCCT",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRecordSuppressionMetrics,
             "RecordSuppressionMetrics",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRedirectExplicitCTAIntentsToExistingActivity,
             "RedirectExplicitCTAIntentsToExistingActivity",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReengagementNotification,
             "ReengagementNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kRelatedSearchesAllLanguage,
             "RelatedSearchesAllLanguage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportParentalControlSitesChild,
             "ReportParentalControlSitesChild",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShareCustomActionsInCCT,
             "ShareCustomActionsInCCT",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSmallerTabStripTitleLimit,
             "SmallerTabStripTitleLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSuppressToolbarCaptures,
             "SuppressToolbarCaptures",
             base::FEATURE_DISABLED_BY_DEFAULT); // Vivaldi

BASE_FEATURE(kSuppressToolbarCapturesAtGestureEnd,
             "SuppressToolbarCapturesAtGestureEnd",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabDragDropAndroid,
             "TabDragDropAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupCreationDialogAndroid,
             "TabGroupCreationDialogAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupParityAndroid,
             "TabGroupParityAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabletTabSwitcherLongPressMenu,
             "TabletTabSwitcherLongPressMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabletToolbarReordering,
             "TabletToolbarReordering",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStateFlatBuffer,
             "TabStateFlatBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripGroupCollapseAndroid,
             "TabStripGroupCollapseAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripGroupContextMenuAndroid,
             "TabStripGroupContextMenuAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripGroupIndicatorsAndroid,
             "TabStripGroupIndicatorsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripLayoutOptimization,
             "TabStripLayoutOptimization",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripStartupRefactoring,
             "TabStripStartupRefactoring",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabStripTransitionInDesktopWindow,
             "TabStripTransitionInDesktopWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabWindowManagerIndexReassignmentActivityFinishing,
             "TabWindowManagerIndexReassignmentActivityFinishing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabWindowManagerIndexReassignmentActivityInSameTask,
             "TabWindowManagerIndexReassignmentActivityInSameTask",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabWindowManagerIndexReassignmentActivityNotInAppTasks,
             "TabWindowManagerIndexReassignmentActivityNotInAppTasks",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabWindowManagerReportIndicesMismatch,
             "TabWindowManagerReportIndicesMismatch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTestDefaultDisabled,
             "TestDefaultDisabled",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTestDefaultEnabled,
             "TestDefaultEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSearchInCCT, "SearchInCCT", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFeedPositionAndroid,
             "FeedPositionAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchResumptionModuleAndroid,
             "SearchResumptionModuleAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStartSurfaceReturnTime,
             "StartSurfaceReturnTime",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAccountReauthenticationRecentTimeWindow,
             "AccountReauthenticationRecentTimeWindow",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSurfacePolish, "SurfacePolish", base::FEATURE_DISABLED_BY_DEFAULT);// Vivaldi Ref. VAB-9134

BASE_FEATURE(kSurfacePolishForToolbarKillSwitch,
             "SurfacePolishForToolbarKillSwitch",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTabResumptionModuleAndroid,
             "TabResumptionModuleAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, keep logging and reporting UMA while chrome is backgrounded.
BASE_FEATURE(kUmaBackgroundSessions,
             "UMABackgroundSessions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Use the LibunwindstackNativeUnwinderAndroid for only browser main thread, and
// only on Android.
BASE_FEATURE(kUseLibunwindstackNativeUnwinderAndroid,
             "UseLibunwindstackNativeUnwinderAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUserMediaScreenCapturing,
             "UserMediaScreenCapturing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVerticalAutomotiveBackButtonToolbar,
             "VerticalAutomotiveBackButtonToolbar",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kVoiceSearchAudioCapturePolicy,
             "VoiceSearchAudioCapturePolicy",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Shows only the remote device name on the Android notification instead of
// a descriptive text.
BASE_FEATURE(kWebOtpCrossDeviceSimpleString,
             "WebOtpCrossDeviceSimpleString",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebApkAllowIconUpdate,
             "WebApkAllowIconUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace android
}  // namespace chrome
