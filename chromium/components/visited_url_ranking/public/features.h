// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace visited_url_ranking::features {

// Core feature flag for Visited URL Ranking service.
BASE_DECLARE_FEATURE(kVisitedURLRankingService);

// Parameter determining the fetch option's default query duration in hours.
extern const char kVisitedURLRankingFetchDurationInHoursParam[];

// Parameter to determine the age limit for history entries to be ranked.
extern const char kHistoryAgeThresholdHours[];
extern const int kHistoryAgeThresholdHoursDefaultValue;

// Parameter to determine the age limit for tab entries to be ranked.
extern const char kTabAgeThresholdHours[];
extern const int kTabAgeThresholdHoursDefaultValue;

// Max number of URL aggregate candidates to rank.
extern const char kURLAggregateCountLimit[];
extern const int kURLAggregateCountLimitDefaultValue;

// Feature flag to disable History visibility score filter.
BASE_DECLARE_FEATURE(kVisitedURLRankingHistoryVisibilityScoreFilter);

// Feature flag to disable the segmentation metrics transformer.
BASE_DECLARE_FEATURE(kVisitedURLRankingSegmentationMetricsData);

// Feature flag for enabling URL visit resumption deduplication.
BASE_DECLARE_FEATURE(kVisitedURLRankingDeduplication);

// Parameter determining if the docs deduplication handler should be used.
extern const base::FeatureParam<bool> kVisitedURLRankingDeduplicationDocs;

// Parameter determining if the search engine deduplication handler should be
// used.
extern const base::FeatureParam<bool>
    kVisitedURLRankingDeduplicationSearchEngine;

// Parameter determining if the query should be cleared.
extern const base::FeatureParam<bool> kVisitedURLRankingDeduplicationFallback;

// Parameter determining if the scheme should be updated to be http.
extern const base::FeatureParam<bool>
    kVisitedURLRankingDeduplicationUpdateScheme;

// Parameter determining which prefixes should be excluded. i.e.
// "www.google.com" would become "google.com" if "www." is excluded.
extern const base::FeatureParam<std::string>
    kVisitedURLRankingDeduplicationExcludedPrefixes;

}  // namespace visited_url_ranking::features

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_FEATURES_H_
