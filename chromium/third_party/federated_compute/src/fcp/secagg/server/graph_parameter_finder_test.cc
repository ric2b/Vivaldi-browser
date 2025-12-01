#ifndef THIRD_PARTY_FCP_SECAGG_SERVER_GRAPH_PARAMETER_FINDER_TEST_CC_
#define THIRD_PARTY_FCP_SECAGG_SERVER_GRAPH_PARAMETER_FINDER_TEST_CC_

/*
 * Copyright 2020 Google LLC
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

#include "fcp/secagg/server/graph_parameter_finder.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"
#include "fcp/secagg/server/secagg_server_messages.pb.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace secagg {
namespace {

struct HararyGraphParameterFinderParams {
  const std::string test_name;
  const int kNumClients;
  const double kAdversarialRate;
  const double kDropoutRate;
  const AdversaryClass kAdversaryClass;
  const int kExpectedDegree;
  const int kExpectedThreshold;
};

class HararyGraphParameterFinderTest_Feasible
    : public ::testing::TestWithParam<HararyGraphParameterFinderParams> {};

TEST_P(HararyGraphParameterFinderTest_Feasible,
       ComputesParametersThatMatchPrecomputedValues) {
  // This test computes parameters for feasible instances. It checks that the
  // obtained degree (number of neighbors) and threshold match a precomputed
  // value.
  const HararyGraphParameterFinderParams& test_params = GetParam();
  SecureAggregationRequirements threat_model;
  int security_parameter = 40;
  int correctness_parameter = 20;
  FCP_LOG(INFO)
      << "Running HararyGraphParameterFinder on instance with num. clients = "
      << test_params.kNumClients
      << ", adversarial rate = " << test_params.kAdversarialRate
      << ", dropout rate = " << test_params.kDropoutRate
      << ", security parameter = " << security_parameter
      << ", correctness parameter = " << correctness_parameter
      << ", adversary class = "
      << (test_params.kAdversaryClass == AdversaryClass::CURIOUS_SERVER
              ? "CURIOUS_SERVER"
              : "SEMI_MALICIOUS_SERVER");
  threat_model.set_adversarial_client_rate(test_params.kAdversarialRate);
  threat_model.set_estimated_dropout_rate(test_params.kDropoutRate);
  threat_model.set_adversary_class(test_params.kAdversaryClass);
  auto computed_params =
      ComputeHararyGraphParameters(test_params.kNumClients, threat_model);
  EXPECT_EQ(computed_params.ok(), true);
  int degree = computed_params.value().degree;
  int threshold = computed_params.value().threshold;
  FCP_LOG(INFO) << "Secure parameters were found: degree = " << degree
                << ", threshold = " << threshold;
  int expected_degree = test_params.kExpectedDegree;
  int expected_threshold = test_params.kExpectedThreshold;
  EXPECT_EQ(degree, expected_degree);
  FCP_LOG(INFO) << "degree = " << degree
                << " expected_degree = " << expected_degree;
  EXPECT_LE(threshold, expected_threshold);
  FCP_LOG(INFO) << "threshold = " << threshold
                << " expected_threshold = " << expected_threshold;
}

TEST_P(HararyGraphParameterFinderTest_Feasible,
       ComputesParametersWithinExpectedRange) {
  // This test computes parameters for feasible instances. It checks that the
  // obtained degree (number of neighbors) is in between the analytical lower
  // and upper bounds.
  const HararyGraphParameterFinderParams& test_params = GetParam();
  SecureAggregationRequirements threat_model;
  int security_parameter = 40;
  int correctness_parameter = 20;
  FCP_LOG(INFO) << "Running HararyGraphParameterFinder on instance with num. "
                   "clients = "
                << test_params.kNumClients
                << ", adversarial rate = " << test_params.kAdversarialRate
                << ", dropout rate = " << test_params.kDropoutRate
                << ", security parameter = " << security_parameter
                << ", correctness parameter = " << correctness_parameter
                << ", adversary class = "
                << (test_params.kAdversaryClass ==
                            AdversaryClass::CURIOUS_SERVER
                        ? "CURIOUS_SERVER"
                        : "SEMI_MALICIOUS_SERVER");
  threat_model.set_adversarial_client_rate(test_params.kAdversarialRate);
  threat_model.set_estimated_dropout_rate(test_params.kDropoutRate);
  threat_model.set_adversary_class(test_params.kAdversaryClass);
  auto computed_params =
      ComputeHararyGraphParameters(test_params.kNumClients, threat_model);
  EXPECT_EQ(computed_params.ok(), true);
  int degree = computed_params.value().degree;
  int threshold = computed_params.value().threshold;
  FCP_LOG(INFO) << "Secure parameters were found: degree = " << degree
                << ", threshold = " << threshold;

  bool unconstrained_instance =
      test_params.kAdversarialRate == 0 && test_params.kDropoutRate == 0;
  bool small_instance = test_params.kNumClients < 20;
  // The degree lower bound this enforces doesn't fit with the security
  // guarantee that the rest of this code is designed to provide. Clearing up
  // what the security guarantee should be is b/260400215 and this test should
  // be cleared up in addressing that. Until then it is switched off for small
  // variables where it behaves at odds with the model used elsewhere.
  double degree_lower_bound =
      unconstrained_instance || small_instance
          ? 1
          : log(test_params.kNumClients) + security_parameter * log(2) / 5.;

  double beta = static_cast<double>(threshold) / degree;
  double alpha = test_params.kNumClients / (test_params.kNumClients - 1);
  double a = log(test_params.kNumClients) + correctness_parameter * log(2);
  double b = 2 * pow(alpha * (1 - test_params.kDropoutRate) - beta, 2);
  double c =
      std::min(2 * pow(beta - alpha * test_params.kAdversarialRate, 2),
               -log(test_params.kAdversarialRate + test_params.kDropoutRate));
  double degree_upper_bound =
      unconstrained_instance ? 3 : std::max(degree_lower_bound / c + 1, a / b);
  // We increase the upper bound slightly for the semi-malicious variant
  if (test_params.kAdversaryClass == AdversaryClass::SEMI_MALICIOUS_SERVER) {
    degree_upper_bound += degree_upper_bound * 1. / 5;
  }
  EXPECT_GT(degree, degree_lower_bound);
  EXPECT_LT(degree, degree_upper_bound);
  EXPECT_GE(degree, threshold);
  EXPECT_GT(threshold, 0);
}

INSTANTIATE_TEST_SUITE_P(
    HararyGraphParameterFinderTests, HararyGraphParameterFinderTest_Feasible,
    testing::ValuesIn<HararyGraphParameterFinderParams>({
        // adversarial_rate = 0.45, dropout_rate = 0.45, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {2,3,4,5,6}}
        {"100_clients__security_40__correctness_20__adversaryrate_045__"
         "dropoutrate_045__adversary_class__semihonest",
         100, 0.45, 0.45, AdversaryClass::CURIOUS_SERVER, 92, 46},
        {"1000_clients__security_40__correctness_20__adversaryrate_045__"
         "dropoutrate_045__adversary_class__semihonest",
         1000, 0.45, 0.45, AdversaryClass::CURIOUS_SERVER, 822, 417},
        {"10000_clients__security_40__correctness_20__adversaryrate_045__"
         "dropoutrate_045__adversary_class__semihonest",
         10000, 0.45, 0.45, AdversaryClass::CURIOUS_SERVER, 3494, 1771},
        {"100000_clients__security_40__correctness_20__adversaryrate_045__"
         "dropoutrate_045__adversary_class__semihonest",
         100000, 0.45, 0.45, AdversaryClass::CURIOUS_SERVER, 5508, 2788},
        {"1000000_clients__security_40__correctness_20__adversaryrate_045__"
         "dropoutrate_045__adversary_class__semihonest",
         1000000, 0.45, 0.45, AdversaryClass::CURIOUS_SERVER, 6240, 3156},
        // adversarial_rate = 0.33, dropout_rate = 0.33, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semihonest",
         10, 0.33, 0.33, AdversaryClass::CURIOUS_SERVER, 8, 4},
        {"100_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semihonest",
         100, 0.33, 0.33, AdversaryClass::CURIOUS_SERVER, 68, 34},
        {"1000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semihonest",
         1000, 0.33, 0.33, AdversaryClass::CURIOUS_SERVER, 286, 150},
        {"10000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semihonest",
         10000, 0.33, 0.33, AdversaryClass::CURIOUS_SERVER, 422, 221},
        {"100000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semihonest",
         100000, 0.33, 0.33, AdversaryClass::CURIOUS_SERVER, 480, 251},
        {"1000000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semihonest",
         1000000, 0.33, 0.33, AdversaryClass::CURIOUS_SERVER, 518, 270},
        // adversarial_rate = 0.3, dropout_rate = 0.3, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {2,3,4,5,6}}
        {"100_clients__security_40__correctness_20__adversaryrate_03__"
         "dropoutrate_03__adversary_class__semimalicious",
         100, 0.3, 0.3, AdversaryClass::SEMI_MALICIOUS_SERVER, 92, 62},
        {"1000_clients__security_40__correctness_20__adversaryrate_03__"
         "dropoutrate_03__adversary_class__semimalicious",
         1000, 0.3, 0.3, AdversaryClass::SEMI_MALICIOUS_SERVER, 872, 584},
        {"10000_clients__security_40__correctness_20__adversaryrate_03__"
         "dropoutrate_03__adversary_class__semimalicious",
         10000, 0.3, 0.3, AdversaryClass::SEMI_MALICIOUS_SERVER, 4822, 3230},
        {"100000_clients__security_40__correctness_20__adversaryrate_03__"
         "dropoutrate_03__adversary_class__semimalicious",
         100000, 0.3, 0.3, AdversaryClass::SEMI_MALICIOUS_SERVER, 9388, 6286},
        {"1000000_clients__security_40__correctness_20__adversaryrate_03__"
         "dropoutrate_03__adversary_class__semimalicious",
         1000000, 0.3, 0.3, AdversaryClass::SEMI_MALICIOUS_SERVER, 11136, 7454},
        // adversarial_rate = 0.05, dropout_rate = 0.33, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semihonest",
         10, 0.05, 0.33, AdversaryClass::CURIOUS_SERVER, 4, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semihonest",
         100, 0.05, 0.33, AdversaryClass::CURIOUS_SERVER, 28, 6},
        {"1000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semihonest",
         1000, 0.05, 0.33, AdversaryClass::CURIOUS_SERVER, 72, 24},
        {"10000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semihonest",
         10000, 0.05, 0.33, AdversaryClass::CURIOUS_SERVER, 86, 29},
        {"100000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semihonest",
         100000, 0.05, 0.33, AdversaryClass::CURIOUS_SERVER, 96, 32},
        {"1000000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semihonest",
         1000000, 0.05, 0.33, AdversaryClass::CURIOUS_SERVER, 102, 34},
        // adversarial_rate = 0.05, dropout_rate = 0.33, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semimalicious",
         10, 0.05, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 8, 5},
        {"100_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semimalicious",
         100, 0.05, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 72, 39},
        {"1000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semimalicious",
         1000, 0.05, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 398, 223},
        {"10000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semimalicious",
         10000, 0.05, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 746, 420},
        {"100000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semimalicious",
         100000, 0.05, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 882, 496},
        {"1000000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_033__adversary_class__semimalicious",
         1000000, 0.05, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 970, 545},
        // adversarial_rate = 0.33, dropout_rate = 0.05, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semihonest",
         10, 0.33, 0.05, AdversaryClass::CURIOUS_SERVER, 4, 4},
        {"100_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semihonest",
         100, 0.33, 0.05, AdversaryClass::CURIOUS_SERVER, 34, 29},
        {"1000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semihonest",
         1000, 0.33, 0.05, AdversaryClass::CURIOUS_SERVER, 78, 60},
        {"10000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semihonest",
         10000, 0.33, 0.05, AdversaryClass::CURIOUS_SERVER, 92, 70},
        {"100000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semihonest",
         100000, 0.33, 0.05, AdversaryClass::CURIOUS_SERVER, 102, 77},
        {"1000000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semihonest",
         1000000, 0.33, 0.05, AdversaryClass::CURIOUS_SERVER, 110, 83},
        // adversarial_rate = 0.33, dropout_rate = 0.05, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semimalicious",
         10, 0.33, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 4, 4},
        {"100_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semimalicious",
         100, 0.33, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 42, 37},
        {"1000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semimalicious",
         1000, 0.33, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 132, 109},
        {"10000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semimalicious",
         10000, 0.33, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 178, 146},
        {"100000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semimalicious",
         100000, 0.33, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 196, 160},
        {"1000000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_005__adversary_class__semimalicious",
         1000000, 0.33, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 212, 173},
        // adversarial_rate = 0.05, dropout_rate = 0.05, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semihonest",
         10, 0.05, 0.05, AdversaryClass::CURIOUS_SERVER, 2, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semihonest",
         100, 0.05, 0.05, AdversaryClass::CURIOUS_SERVER, 12, 6},
        {"1000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semihonest",
         1000, 0.05, 0.05, AdversaryClass::CURIOUS_SERVER, 30, 17},
        {"10000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semihonest",
         10000, 0.05, 0.05, AdversaryClass::CURIOUS_SERVER, 34, 20},
        {"100000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semihonest",
         100000, 0.05, 0.05, AdversaryClass::CURIOUS_SERVER, 36, 21},
        {"1000000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semihonest",
         1000000, 0.05, 0.05, AdversaryClass::CURIOUS_SERVER, 38, 22},
        // adversarial_rate = 0.05, dropout_rate = 0.05, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semimalicious",
         10, 0.05, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semimalicious",
         100, 0.05, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 16, 11},
        {"1000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semimalicious",
         1000, 0.05, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 52, 37},
        {"10000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semimalicious",
         10000, 0.05, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 62, 44},
        {"100000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semimalicious",
         100000, 0.05, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 68, 48},
        {"1000000_clients__security_40__correctness_20__adversaryrate_005__"
         "dropoutrate_005__adversary_class__semimalicious",
         1000000, 0.05, 0.05, AdversaryClass::SEMI_MALICIOUS_SERVER, 74, 52},
        // adversarial_rate = 0.33, dropout_rate = 0.0, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semihonest",
         10, 0.33, 0.0, AdversaryClass::CURIOUS_SERVER, 4, 4},
        {"100_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semihonest",
         100, 0.33, 0.0, AdversaryClass::CURIOUS_SERVER, 26, 25},
        {"1000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semihonest",
         1000, 0.33, 0.0, AdversaryClass::CURIOUS_SERVER, 38, 36},
        {"10000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semihonest",
         10000, 0.33, 0.0, AdversaryClass::CURIOUS_SERVER, 42, 40},
        {"100000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semihonest",
         100000, 0.33, 0.0, AdversaryClass::CURIOUS_SERVER, 46, 44},
        {"1000000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semihonest",
         1000000, 0.33, 0.0, AdversaryClass::CURIOUS_SERVER, 50, 47},
        // adversarial_rate = 0.33, dropout_rate = 0.0, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semimalicious",
         10, 0.33, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 4, 4},
        {"100_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semimalicious",
         100, 0.33, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 26, 26},
        {"1000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semimalicious",
         1000, 0.33, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 38, 37},
        {"10000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semimalicious",
         10000, 0.33, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 42, 41},
        {"100000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semimalicious",
         100000, 0.33, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 46, 45},
        {"1000000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_00__adversary_class__semimalicious",
         1000000, 0.33, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 50, 49},
        // adversarial_rate = 0.0, dropout_rate = 0.33, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semihonest",
         10, 0.0, 0.33, AdversaryClass::CURIOUS_SERVER, 4, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semihonest",
         100, 0.0, 0.33, AdversaryClass::CURIOUS_SERVER, 26, 2},
        {"1000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semihonest",
         1000, 0.0, 0.33, AdversaryClass::CURIOUS_SERVER, 38, 2},
        {"10000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semihonest",
         10000, 0.0, 0.33, AdversaryClass::CURIOUS_SERVER, 42, 2},
        {"100000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semihonest",
         100000, 0.0, 0.33, AdversaryClass::CURIOUS_SERVER, 46, 2},
        {"1000000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semihonest",
         1000000, 0.0, 0.33, AdversaryClass::CURIOUS_SERVER, 50, 2},
        // adversarial_rate = 0.0, dropout_rate = 0.33, adversary_class =
        // none, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__none",
         10, 0.0, 0.33, AdversaryClass::NONE, 4, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__none",
         100, 0.0, 0.33, AdversaryClass::NONE, 26, 2},
        {"1000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__none",
         1000, 0.0, 0.33, AdversaryClass::NONE, 38, 2},
        {"10000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__none",
         10000, 0.0, 0.33, AdversaryClass::NONE, 42, 2},
        {"100000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__none",
         100000, 0.0, 0.33, AdversaryClass::NONE, 46, 2},
        {"1000000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__none",
         1000000, 0.0, 0.33, AdversaryClass::NONE, 50, 2},
        // adversarial_rate = 0.0, dropout_rate = 0.33, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semimalicious",
         10, 0.0, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 8, 5},
        {"100_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semimalicious",
         100, 0.0, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 68, 35},
        {"1000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semimalicious",
         1000, 0.0, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 228, 115},
        {"10000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semimalicious",
         10000, 0.0, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 326, 164},
        {"100000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semimalicious",
         100000, 0.0, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 372, 187},
        {"1000000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_033__adversary_class__semimalicious",
         1000000, 0.0, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 410, 206},
        // adversarial_rate = 0.0, dropout_rate = 0.0, adversary_class =
        // semihonest, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semihonest",
         10, 0.0, 0.0, AdversaryClass::CURIOUS_SERVER, 2, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semihonest",
         100, 0.0, 0.0, AdversaryClass::CURIOUS_SERVER, 2, 2},
        {"1000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semihonest",
         1000, 0.0, 0.0, AdversaryClass::CURIOUS_SERVER, 2, 2},
        {"10000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semihonest",
         10000, 0.0, 0.0, AdversaryClass::CURIOUS_SERVER, 2, 2},
        {"100000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semihonest",
         100000, 0.0, 0.0, AdversaryClass::CURIOUS_SERVER, 2, 2},
        {"1000000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semihonest",
         1000000, 0.0, 0.0, AdversaryClass::CURIOUS_SERVER, 2, 2},
        // adversarial_rate = 0.0, dropout_rate = 0.0, adversary_class =
        // none, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"1000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__none",
         1000, 0.0, 0.0, AdversaryClass::NONE, 2, 2},
        {"10000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__none",
         10000, 0.0, 0.0, AdversaryClass::NONE, 2, 2},
        {"100000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__none",
         100000, 0.0, 0.0, AdversaryClass::NONE, 2, 2},
        {"1000000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__none",
         1000000, 0.0, 0.0, AdversaryClass::NONE, 2, 2},
        // adversarial_rate = 0.0, dropout_rate = 0.0, adversary_class =
        // semimalicious, for number_of_clients in {10^i | i\in {1,2,3,4,5,6}}
        {"10_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semimalicious",
         10, 0.0, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
        {"100_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semimalicious",
         100, 0.0, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
        {"1000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semimalicious",
         1000, 0.0, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
        {"10000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semimalicious",
         10000, 0.0, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
        {"100000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semimalicious",
         100000, 0.0, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
        {"1000000_clients__security_40__correctness_20__adversaryrate_00__"
         "dropoutrate_00__adversary_class__semimalicious",
         1000000, 0.0, 0.0, AdversaryClass::SEMI_MALICIOUS_SERVER, 2, 2},
    }),
    [](const ::testing::TestParamInfo<
        HararyGraphParameterFinderTest_Feasible::ParamType>& info) {
      return info.param.test_name;
    });

class HararyGraphParameterFinderTest_InvalidOrUnfeasible
    : public ::testing::TestWithParam<HararyGraphParameterFinderParams> {};

TEST_P(HararyGraphParameterFinderTest_InvalidOrUnfeasible,
       FailsOnIncorrectParameters) {
  // This test tries to compute parameters for invalid (parameters with
  // incorrect values) or unfeasible (combinations of valid parameter values
  // that make the problem unsolvable) instances.
  const HararyGraphParameterFinderParams& test_params = GetParam();
  SecureAggregationRequirements threat_model;
  threat_model.set_adversarial_client_rate(test_params.kAdversarialRate);
  threat_model.set_estimated_dropout_rate(test_params.kDropoutRate);
  threat_model.set_adversary_class(test_params.kAdversaryClass);
  auto computed_params =
      ComputeHararyGraphParameters(test_params.kNumClients, threat_model);
  EXPECT_EQ(computed_params.ok(), false);
}

INSTANTIATE_TEST_SUITE_P(
    HararyGraphParameterFinderTests,
    HararyGraphParameterFinderTest_InvalidOrUnfeasible,
    ::testing::ValuesIn<HararyGraphParameterFinderParams>({
        {"0_clients__security_40__correctness_20__adversaryrate_01__"
         "dropoutrate_01__adversary_class__semihonest",
         0, 0.1, 0.1, AdversaryClass::CURIOUS_SERVER, 0, 0},
        {"1000_clients__security_40__correctness_20__adversaryrate_1__"
         "dropoutrate_1__adversary_class__semihonest",
         1000, 0.1, 1., AdversaryClass::CURIOUS_SERVER, 0, 0},
        {"1000_clients__security_40__correctness_20__adversaryrate_01__"
         "dropoutrate_minus1__adversary_class__semihonest",
         1000, 0.1, -1., AdversaryClass::CURIOUS_SERVER, 0, 0},
        // For semi_honest/honest-but-curious adversary, we need that
        // adversary_rate + dropout_rate < 1 for the instance to be feasible
        {"1000_clients__security_40__correctness_20__adversaryrate_05__"
         "dropoutrate_05__adversary_class__semihonest",
         1000, 0.5, 0.5, AdversaryClass::CURIOUS_SERVER, 0, 0},
        // For semi_malicious adversary, we need that adversary_rate +
        // 2*dropout_rate < 1 for the instance to be feasible
        {"1000_clients__security_40__correctness_20__adversaryrate_05__"
         "dropoutrate_05__adversary_class__semimalicious",
         1000, 0.5, 0.5, AdversaryClass::SEMI_MALICIOUS_SERVER, 0, 0},
        {"1000_clients__security_40__correctness_20__adversaryrate_033__"
         "dropoutrate_033__adversary_class__semimalicious",
         1000, 0.33, 0.33, AdversaryClass::SEMI_MALICIOUS_SERVER, 0, 0},
        // For the no-adversary setting we expect adversary_rate == 0
        {"1000_clients__security_40__correctness_20__adversaryrate_05__"
         "dropoutrate_05__adversary_class__none",
         1000, 0.5, 0.5, AdversaryClass::NONE, 0, 0},
    }),
    [](const ::testing::TestParamInfo<
        HararyGraphParameterFinderTest_InvalidOrUnfeasible::ParamType>& info) {
      return info.param.test_name;
    });

TEST(FullGraphCeckParamsTest, ReturnsTrueOnValidThresholds) {
  SecureAggregationRequirements threat_model;
  threat_model.set_adversarial_client_rate(.05);
  threat_model.set_estimated_dropout_rate(.3);
  threat_model.set_adversary_class(AdversaryClass::CURIOUS_SERVER);
  int num_clients = 60;
  for (int t = 42; t < num_clients; t++) {
    EXPECT_THAT(CheckFullGraphParameters(num_clients, t, threat_model).ok(),
                true)
        << t;
  }
}

TEST(FullGraphCeckParamsTest, ReturnsFalseOnInvalidThresholds) {
  SecureAggregationRequirements threat_model;
  threat_model.set_adversarial_client_rate(.05);
  threat_model.set_estimated_dropout_rate(.3);
  threat_model.set_adversary_class(AdversaryClass::CURIOUS_SERVER);
  int num_clients = 60;
  for (int t = 0; t < 42; t++) {
    EXPECT_THAT(CheckFullGraphParameters(num_clients, t, threat_model).ok(),
                false);
  }
}

}  // namespace
}  // namespace secagg
}  // namespace fcp

#endif  // THIRD_PARTY_FCP_SECAGG_SERVER_GRAPH_PARAMETER_FINDER_TEST_CC_
