// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_

#include <stdint.h>

#include <compare>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/numerics/checked_math.h"
#include "base/types/expected.h"

namespace attribution_reporting {

class TriggerSpecs;

struct FakeEventLevelReport {
  uint32_t trigger_data;
  int window_index;

  friend std::strong_ordering operator<=>(const FakeEventLevelReport&,
                                          const FakeEventLevelReport&) =
      default;
};

// Corresponds to `StoredSource::AttributionLogic` as follows:
// `std::nullopt` -> `StoredSource::AttributionLogic::kTruthfully`
// empty vector -> `StoredSource::AttributionLogic::kNever`
// non-empty vector -> `StoredSource::AttributionLogic::kFalsely`
using RandomizedResponse = std::optional<std::vector<FakeEventLevelReport>>;

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
bool IsValid(const RandomizedResponse&, const TriggerSpecs&);

enum class RandomizedResponseError {
  kExceedsChannelCapacityLimit,
  kExceedsTriggerStateCardinalityLimit,
};

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING) RandomizedResponseData {
 public:
  RandomizedResponseData(double rate,
                         RandomizedResponse);

  ~RandomizedResponseData();

  RandomizedResponseData(const RandomizedResponseData&);
  RandomizedResponseData& operator=(const RandomizedResponseData&);

  RandomizedResponseData(RandomizedResponseData&&);
  RandomizedResponseData& operator=(RandomizedResponseData&&);

  double rate() const { return rate_; }

  const RandomizedResponse& response() const { return response_; }

  RandomizedResponse& response() { return response_; }

  friend bool operator==(const RandomizedResponseData&,
                         const RandomizedResponseData&) = default;

 private:
  double rate_;
  RandomizedResponse response_;
};

// Returns true with probability r.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING) bool GenerateWithRate(double r);

// https://wicg.github.io/attribution-reporting-api/#obtain-a-randomized-source-response-pick-rate
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
double GetRandomizedResponseRate(uint32_t num_states, double epsilon);

// Returns the number of possible output states for the given API configuration.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<uint32_t, RandomizedResponseError> GetNumStates(
    const TriggerSpecs& specs);

// Determines the randomized response flip probability for the given API
// configuration, and performs randomized response on that output space.
//
// Returns `std::nullopt` if the output should be determined truthfully.
// Otherwise will return a vector of fake reports.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponse(
    const TriggerSpecs& specs,
    double epsilon,
    double max_channel_capacity);

COMPONENT_EXPORT(ATTRIBUTION_REPORTING) uint32_t MaxTriggerStateCardinality();

// Exposed for testing purposes.
namespace internal {

// Computes the binomial coefficient aka (`n` choose `k`).
// https://en.wikipedia.org/wiki/Binomial_coefficient
// Negative inputs are not supported.
//
// Note: large values of `n` and `k` may overflow, which will cause the returned
// `CheckedNumeric` to be invalid.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::CheckedNumeric<uint32_t> BinomialCoefficient(int n, int k);

// Returns the k-combination associated with the number `combination_index`. In
// other words, returns the combination of `k` integers uniquely indexed by
// `combination_index` in the combinatorial number system.
// https://en.wikipedia.org/wiki/Combinatorial_number_system
//
// The returned vector is guaranteed to have size `k`.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::vector<int> GetKCombinationAtIndex(
    base::StrictNumeric<uint32_t> combination_index,
    int k);

// Returns the number of possible sequences of "stars and bars" sequences
// https://en.wikipedia.org/wiki/Stars_and_bars_(combinatorics),
// which is equivalent to (num_stars + num_bars choose num_stars).
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::CheckedNumeric<uint32_t> GetNumberOfStarsAndBarsSequences(int num_stars,
                                                                int num_bars);

// Returns a vector of the indices of every star in the stars and bars sequence
// indexed by `sequence_index`. The indexing technique uses the k-combination
// utility documented above.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::vector<int>, absl::monostate> GetStarIndices(
    int num_stars,
    int num_bars,
    base::StrictNumeric<uint32_t> sequence_index);

// From a vector with the index of every star in a stars and bars sequence,
// returns a vector which, for every star, counts the number of bars preceding
// it. Assumes `star_indices` is in descending order. Output is also sorted
// in descending order.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
std::vector<int> GetBarsPrecedingEachStar(std::vector<int> star_indices);

// Computes the binary entropy function:
// https://en.wikipedia.org/wiki/Binary_entropy_function
COMPONENT_EXPORT(ATTRIBUTION_REPORTING) double BinaryEntropy(double p);

// Computes the channel capacity of a qary-symmetric channel.
// https://wicg.github.io/attribution-reporting-api/#computing-channel-capacity
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
double ComputeChannelCapacity(base::StrictNumeric<uint32_t> num_states_strict,
                              double randomized_response_rate);

// Generates fake reports from the "stars and bars" sequence index of a
// possible output of the API. This output is determined by the following
// algorithm:
// 1. Find all stars before the first bar. These stars represent suppressed
//    reports.
// 2. For all other stars, count the number of bars that precede them. Each
//    star represents a report where the reporting window and trigger data is
//    uniquely determined by that number.
//
// `CHECK()`s `TriggerSpecs::SingleSharedSpec()`.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::vector<FakeEventLevelReport>, RandomizedResponseError>
GetFakeReportsForSequenceIndex(
    const TriggerSpecs&,
    base::StrictNumeric<uint32_t> random_stars_and_bars_sequence_index);

// Note: this method for sampling is not 1:1 with the above function for the
// same sequence index, even for equivalent API configs.
//
// Takes a `StateMap`, to optimize with the cache from previous calls that
// pre-compute the number of states (`GetNumStatesRecursive()`).
using ConfigForCache = uint32_t;
using StateMap = std::map<ConfigForCache, base::CheckedNumeric<uint32_t>>;
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::vector<FakeEventLevelReport>, RandomizedResponseError>
GetFakeReportsForSequenceIndex(const TriggerSpecs& specs,
                               base::StrictNumeric<uint32_t> index,
                               StateMap& map);

// Exposed to speed up tests which perform randomized response many times in a
// row.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<RandomizedResponseData, RandomizedResponseError>
DoRandomizedResponseWithCache(
    const TriggerSpecs& specs,
    double epsilon,
    StateMap& map,
    double max_channel_capacity);

}  // namespace internal

class COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
    ScopedMaxTriggerStateCardinalityForTesting {
 public:
  explicit ScopedMaxTriggerStateCardinalityForTesting(uint32_t);

  ~ScopedMaxTriggerStateCardinalityForTesting();

  ScopedMaxTriggerStateCardinalityForTesting(
      const ScopedMaxTriggerStateCardinalityForTesting&) = delete;
  ScopedMaxTriggerStateCardinalityForTesting& operator=(
      const ScopedMaxTriggerStateCardinalityForTesting&) = delete;

  ScopedMaxTriggerStateCardinalityForTesting(
      ScopedMaxTriggerStateCardinalityForTesting&&) = delete;
  ScopedMaxTriggerStateCardinalityForTesting& operator=(
      ScopedMaxTriggerStateCardinalityForTesting&&) = delete;

 private:
  uint32_t previous_;
};

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_PRIVACY_MATH_H_
