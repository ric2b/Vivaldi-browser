// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/heuristic_source.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

HeuristicSource GetActiveHeuristicSource() {
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
      features::kAutofillModelPredictionsAreActive.Get()) {
    return HeuristicSource::kMachineLearning;
  }
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  return features::kAutofillParsingPatternActiveSource.Get() == "default"
             ? HeuristicSource::kDefault
             : HeuristicSource::kExperimental;
#else
  return HeuristicSource::kLegacy;
#endif
}

DenseSet<HeuristicSource> GetNonActiveHeuristicSources() {
  DenseSet<HeuristicSource> non_active_sources;
  switch (GetActiveHeuristicSource()) {
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    // If a `PatternSource` is the active `HeuristicSource`, compute shadow
    // predictions against the `PatternSource` of the prior rollout stage.
    case HeuristicSource::kDefault:
      non_active_sources.insert(HeuristicSource::kExperimental);
      break;
    case HeuristicSource::kExperimental:
      break;
#endif
    // On non Chrome-branded builds, no alternative `PatternSource`s exist.
    case HeuristicSource::kLegacy:
      break;
    // If ML is active, compare against the `PatternSource`-based predictions
    // that would otherwise be active.
    case HeuristicSource::kMachineLearning:
      non_active_sources.insert(
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
          HeuristicSource::kDefault
#else
          HeuristicSource::kLegacy
#endif
      );
      break;
  }
  // If ML is enabled but inactive, compute shadow predictions for it.
  if (base::FeatureList::IsEnabled(features::kAutofillModelPredictions) &&
      !features::kAutofillModelPredictionsAreActive.Get()) {
    non_active_sources.insert(HeuristicSource::kMachineLearning);
  }
  return non_active_sources;
}

std::optional<PatternSource> HeuristicSourceToPatternSource(
    HeuristicSource source) {
  switch (source) {
    case HeuristicSource::kLegacy:
      return PatternSource::kLegacy;
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    case HeuristicSource::kDefault:
      return PatternSource::kDefault;
    case HeuristicSource::kExperimental:
      return PatternSource::kExperimental;
#endif
    case autofill::HeuristicSource::kMachineLearning:
      return std::nullopt;
  }
  NOTREACHED_NORETURN();
}

HeuristicSource PatternSourceToHeuristicSource(PatternSource source) {
  switch (source) {
    case PatternSource::kLegacy:
      return HeuristicSource::kLegacy;
#if BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
    case PatternSource::kDefault:
      return HeuristicSource::kDefault;
    case PatternSource::kExperimental:
      return HeuristicSource::kExperimental;
#endif
  }
  NOTREACHED_NORETURN();
}
}  // namespace autofill
