// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/heuristic_source.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Depending on `kAutofillModelPredictions`, the active and non active heuristic
// sources will differ.
//
// Currently, the available heuristic sources are the ML model and
// the pattern sources. If the model predictions are disabled, then
// only pattern sources are used. If model predictions are enabled,
// `kMachineLearning` is also considered. Depending on
// `kAutofillModelPredictionsAreActive`, use  `kMachineLearning`
// as the active heuristic source.

struct HeuristicSourceParams {
  std::optional<bool> model_predictions_feature;
  std::string pattern_provider_feature;
  const HeuristicSource expected_active_source;
  const DenseSet<HeuristicSource> expected_nonactive_sources;
};

class HeuristicSourceTest
    : public testing::TestWithParam<HeuristicSourceParams> {
 public:
  HeuristicSourceTest() {
    const HeuristicSourceParams& test_case = GetParam();
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (test_case.model_predictions_feature) {
      std::string model_prediction_active =
          *test_case.model_predictions_feature ? "true" : "false";
      enabled_features.push_back(
          {features::kAutofillModelPredictions,
           {{features::kAutofillModelPredictionsAreActive.name,
             model_prediction_active}}});
    } else {
      disabled_features.push_back(features::kAutofillModelPredictions);
    }
    enabled_features.push_back(
        {features::kAutofillParsingPatternProvider,
         {{features::kAutofillParsingPatternActiveSource.name,
           test_case.pattern_provider_feature}}});
    features_.InitWithFeaturesAndParameters(enabled_features,
                                            disabled_features);
  }

 private:
  base::test::ScopedFeatureList features_;
};

TEST_P(HeuristicSourceTest, HeuristicSourceParams) {
  const HeuristicSourceParams& test_case = GetParam();
  EXPECT_EQ(GetActiveHeuristicSource(), test_case.expected_active_source);
  EXPECT_EQ(GetNonActiveHeuristicSources(),
            test_case.expected_nonactive_sources);
}

INSTANTIATE_TEST_SUITE_P(
    HeuristicSourceTest,
    HeuristicSourceTest,
    testing::Values(
// The pattern provider behavior differs between Chrome and non-Chrome branded
// instances.
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
        HeuristicSourceParams{
            .pattern_provider_feature = "legacy",
            .expected_active_source = HeuristicSource::kLegacy,
            .expected_nonactive_sources = {}},

        HeuristicSourceParams{
            .model_predictions_feature = true,
            .pattern_provider_feature = "legacy",
            .expected_active_source = HeuristicSource::kMachineLearning,
            .expected_nonactive_sources = {HeuristicSource::kLegacy}},

        HeuristicSourceParams {
          .model_predictions_feature = false,
          .pattern_provider_feature = "legacy",
          .expected_active_source = HeuristicSource::kLegacy,
          .expected_nonactive_sources = {
            HeuristicSource::kMachineLearning
          }
        }
#else
        HeuristicSourceParams{
            .model_predictions_feature = true,
            .pattern_provider_feature = "default",
            .expected_active_source = HeuristicSource::kMachineLearning,
            .expected_nonactive_sources = {HeuristicSource::kDefault}},

        HeuristicSourceParams{
            .model_predictions_feature = false,
            .pattern_provider_feature = "default",
            .expected_active_source = HeuristicSource::kDefault,
            .expected_nonactive_sources = {HeuristicSource::kExperimental,
                                           HeuristicSource::kMachineLearning}},
        HeuristicSourceParams{
            .pattern_provider_feature = "default",
            .expected_active_source = HeuristicSource::kDefault,
            .expected_nonactive_sources = {HeuristicSource::kExperimental}},

        HeuristicSourceParams{
            .pattern_provider_feature = "experimental",
            .expected_active_source = HeuristicSource::kExperimental,
            .expected_nonactive_sources = {}}
#endif
        ));

}  // namespace autofill
