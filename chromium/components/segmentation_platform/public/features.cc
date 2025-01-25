// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/features.h"

#include "build/build_config.h"

namespace segmentation_platform::features {

BASE_FEATURE(kSegmentationPlatformFeature,
             "SegmentationPlatform",
             base::FEATURE_DISABLED_BY_DEFAULT); // Vivaldi - disable UKM

BASE_FEATURE(kSegmentationPlatformUkmEngine,
             "SegmentationPlatformUkmEngine",

#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT); // Vivaldi - disable UKM
#endif

BASE_FEATURE(kSegmentationPlatformUserVisibleTaskRunner,
             "SegmentationPlatformUserVisibleTaskRunner",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformAdaptiveToolbarV2Feature,
             "SegmentationPlatformAdaptiveToolbarV2Feature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformLowEngagementFeature,
             "SegmentationPlatformLowEngagementFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShoppingUserSegmentFeature,
             "ShoppingUserSegmentFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformSearchUser,
             "SegmentationPlatformSearchUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformDeviceSwitcher,
             "SegmentationPlatformDeviceSwitcher",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformFeedSegmentFeature,
             "SegmentationPlatformFeedSegmentFeature",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kResumeHeavyUserSegmentFeature,
             "ResumeHeavyUserSegment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformPowerUserFeature,
             "SegmentationPlatformPowerUserFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFrequentFeatureUserSegmentFeature,
             "FrequentFeatureUserSegmentFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActions,
             "ContextualPageActions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kContextualPageActionReaderMode,
             "ContextualPageActionReaderMode",
#if defined(VIVALDI_BUILD)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif // VIVALDI_BUILD

BASE_FEATURE(kContextualPageActionShareModel,
             "ContextualPageActionShareModel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationDefaultReportingSegments,
             "SegmentationDefaultReportingSegments",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformDeviceTier,
             "SegmentationPlatformDeviceTier",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTabletProductivityUser,
             "SegmentationPlatformTabletProductivityUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformModelExecutionSampling,
             "SegmentationPlatformModelExecutionSampling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformCrossDeviceUser,
             "SegmentationPlatformCrossDeviceUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIntentionalUser,
             "SegmentationPlatformIntentionalUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformPasswordManagerUser,
             "SegmentationPlatformPasswordManagerUser",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTabResumptionRanker,
             "SegmentationPlatformTabResumptionRanker",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIosModuleRanker,
             "SegmentationPlatformIosModuleRanker",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformAndroidHomeModuleRanker,
             "SegmentationPlatformAndroidHomeModuleRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformAndroidHomeModuleRankerV2,
             "SegmentationPlatformAndroidHomeModuleRankerV2",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformTimeDelaySampling,
             "SegmentationPlatformTimeDelaySampling",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformCollectTabRankData,
             "SegmentationPlatformCollectTabRankData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformModelInitializationDelay,
             "SegmentationPlatformModelInitializationDelay",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enabled only on iOS to improve startup performance of the module ranker.
BASE_FEATURE(kSegmentationPlatformSignalDbCache,
             "SegmentationPlatformSignalDbCache",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformComposePromotion,
             "SegmentationPlatformComposePromotion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformUmaFromSqlDb,
             "SegmentationPlatformUmaFromSqlDb",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSegmentationPlatformIosModuleRankerSplitBySurface,
             "SegmentationPlatformIosModuleRankerSplitBySurface",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSegmentationPlatformURLVisitResumptionRanker,
             "SegmentationPlatformURLVisitResumptionRanker",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace segmentation_platform::features
