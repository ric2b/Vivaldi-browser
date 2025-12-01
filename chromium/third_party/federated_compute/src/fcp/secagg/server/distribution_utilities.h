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

#ifndef FCP_SECAGG_SERVER_DISTRIBUTION_UTILITIES_H_
#define FCP_SECAGG_SERVER_DISTRIBUTION_UTILITIES_H_

#include <memory>

#include "fcp/base/monitoring.h"

namespace fcp {
namespace secagg {

// Represents a Hypergeometric distribution with parameters fixed at creation of
// the object. Allows to query certain distribution functions.
class HypergeometricDistribution {
 public:
  static StatusOr<std::unique_ptr<HypergeometricDistribution>> Create(
      int total, int marked, int sampled);

  // Evaluates the probability mass funciton of the random variable at x.
  double PMF(double x);

  // Evaluates the cumulative distribution function of the random variable at x.
  double CDF(double x);

  // Finds the value whose cdf is quantile rounded outwards to an integer.
  // Setting complement to true is equivalent to setting quantile = 1 - quantile
  // but can avoid numerical error in the extreme upper tail.
  double FindQuantile(double quantile, bool complement = false);

 private:
  const int total_;
  const int marked_;
  const int sampled_;

  HypergeometricDistribution(int total, int marked, int sampled)
      : total_(total), marked_(marked), sampled_(sampled) {}

  double PMFImpl(double x, int counted);

  double CDFImpl(double x, int counted);

  double FindQuantileImpl(double quantile, int counted);
};

}  // namespace secagg
}  // namespace fcp
#endif  // FCP_SECAGG_SERVER_DISTRIBUTION_UTILITIES_H_
