/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/secagg/server/distribution_utilities.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "fcp/base/monitoring.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {
namespace {

struct HypergeometricCDForPMFInstance {
  const double x;
  const int total;
  const int marked;
  const int sampled;
  const double probability;
};

struct HypergeometricQuantileInstance {
  const double probability;
  const int total;
  const int marked;
  const int sampled;
  const int lower;
  const int upper;
};

class HypergeometricPMF
    : public ::testing::TestWithParam<HypergeometricCDForPMFInstance> {};

class HypergeometricCDF
    : public ::testing::TestWithParam<HypergeometricCDForPMFInstance> {};

class HypergeometricQuantile
    : public ::testing::TestWithParam<HypergeometricQuantileInstance> {};

TEST(HypergeometricDistributionCreate, RejectsInvalidInputs) {
  ASSERT_FALSE(HypergeometricDistribution::Create(10, 11, 5).ok());
  ASSERT_FALSE(HypergeometricDistribution::Create(10, 5, 11).ok());
  ASSERT_FALSE(HypergeometricDistribution::Create(10, -1, 5).ok());
  ASSERT_FALSE(HypergeometricDistribution::Create(10, 5, -1).ok());
  ASSERT_FALSE(HypergeometricDistribution::Create(-10, 5, 5).ok());
  ASSERT_FALSE(HypergeometricDistribution::Create(-10, -5, -5).ok());
}

TEST_P(HypergeometricPMF, ReturnsPrecomputedValues) {
  const HypergeometricCDForPMFInstance& test_params = GetParam();
  FCP_LOG(INFO) << "Testing hypergeometric pmf with x = " << test_params.x
                << " total = " << test_params.total
                << " marked = " << test_params.marked
                << " sampled = " << test_params.sampled << ".";
  auto p = HypergeometricDistribution::Create(
      test_params.total, test_params.marked, test_params.sampled);
  ASSERT_THAT(p, IsOk());
  double result = p.value()->PMF(test_params.x);
  double relative_error =
      abs(result - test_params.probability) / (test_params.probability + 1e-30);
  EXPECT_LT(relative_error, 1e-9);
  FCP_LOG(INFO) << "result = " << result
                << " expected_result = " << test_params.probability
                << " relative_error" << relative_error;
}

INSTANTIATE_TEST_SUITE_P(HypergeometricPMFTests, HypergeometricPMF,
                         ::testing::ValuesIn<HypergeometricCDForPMFInstance>(
                             {{-5, 9, 3, 3, 0.0},
                              {17, 9, 3, 3, 0.0},
                              {0, 10, 0, 5, 1.0},
                              {3, 10, 10, 5, 0.0},
                              {4, 15, 6, 12, 0.2967032967032967},
                              {38, 98, 63, 17, 0.0},
                              {2, 187, 105, 43, 5.423847289689941e-16},
                              {40, 980, 392, 103, 0.08225792329713294},
                              {89, 1489, 312, 370, 0.014089199026838601},
                              {100000, 1000000, 200000, 500000,
                               0.0019947087839501726}}));

TEST_P(HypergeometricCDF, ReturnsPrecomputedValues) {
  const HypergeometricCDForPMFInstance& test_params = GetParam();
  FCP_LOG(INFO) << "Testing hypergeometric cdf with x = " << test_params.x
                << " total = " << test_params.total
                << " marked = " << test_params.marked
                << " sampled = " << test_params.sampled << ".";
  auto p = HypergeometricDistribution::Create(
      test_params.total, test_params.marked, test_params.sampled);
  ASSERT_THAT(p, IsOk());
  double result = p.value()->CDF(test_params.x);
  double relative_error =
      abs(result - test_params.probability) / (test_params.probability + 1e-30);
  EXPECT_LT(relative_error, 1e-9);
  FCP_LOG(INFO) << "result = " << result
                << " expected_result = " << test_params.probability
                << " relative_error" << relative_error;
}

INSTANTIATE_TEST_SUITE_P(HypergeometricCDFTests, HypergeometricCDF,
                         ::testing::ValuesIn<HypergeometricCDForPMFInstance>(
                             {{-5, 9, 3, 3, 0.0},
                              {17, 9, 3, 3, 1.0},
                              {0, 10, 0, 5, 1.0},
                              {3, 10, 10, 5, 0.0},
                              {4.5, 15, 6, 12, 0.34065934065934067},
                              {38, 98, 63, 17, 1.0},
                              {2, 187, 105, 43, 5.526570670097338e-16},
                              {40, 980, 392, 103, 0.4430562850817352},
                              {89, 1489, 312, 370, 0.9599670222722507},
                              {100000, 1000000, 200000, 500000,
                               0.5009973543919738}}));

TEST_P(HypergeometricQuantile, ReturnsPrecomputedValues) {
  const HypergeometricQuantileInstance& test_params = GetParam();
  FCP_LOG(INFO) << "Testing hypergeometric quantile with probability = "
                << test_params.probability << " total = " << test_params.total
                << " marked = " << test_params.marked
                << " sampled = " << test_params.sampled << ".";
  auto p = HypergeometricDistribution::Create(
      test_params.total, test_params.marked, test_params.sampled);
  ASSERT_THAT(p, IsOk());
  double result_lower = p.value()->FindQuantile(test_params.probability);
  EXPECT_GE(result_lower, test_params.lower);
  EXPECT_LE(result_lower, test_params.lower + 1);
  FCP_LOG(INFO) << "Lower result = " << result_lower
                << " which should be between " << test_params.lower << " and "
                << test_params.lower + 1 << ".";
  double result_upper = p.value()->FindQuantile(test_params.probability, true);
  EXPECT_LE(result_upper, test_params.upper);
  EXPECT_GE(result_upper, test_params.upper - 1);
  FCP_LOG(INFO) << "Upper result = " << result_upper
                << " which should be between " << test_params.upper - 1
                << " and " << test_params.upper << ".";
}

INSTANTIATE_TEST_SUITE_P(HypergeometricQuantileTests, HypergeometricQuantile,
                         ::testing::ValuesIn<HypergeometricQuantileInstance>(
                             {{0.5, 10, 0, 5, -1, 0},
                              {0.2, 10, 10, 5, 4, 5},
                              {0.97, 15, 6, 12, 5, 3},
                              {0.0001, 98, 63, 17, 3, 17},
                              {1e-05, 187, 105, 43, 11, 36},
                              {3e-08, 980, 392, 103, 16, 67},
                              {1.1e-09, 1489, 312, 370, 38, 119},
                              {1e-18, 1000000, 200000, 500000, 98248,
                               101751}}));

}  // namespace
}  // namespace secagg
}  // namespace fcp
