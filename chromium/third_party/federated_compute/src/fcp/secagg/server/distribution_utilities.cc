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

#include <cmath>
#include <iostream>
#include <memory>

namespace fcp {
namespace secagg {

StatusOr<std::unique_ptr<HypergeometricDistribution>>
HypergeometricDistribution::Create(int total, int marked, int sampled) {
  if (total < 0) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The population should be at least zero. Value provided = "
           << total;
  }
  if (marked < 0) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The marked population should have size at least zero. Value "
              "provided = "
           << marked;
  }
  if (sampled < 0) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The sample size should be at least zero. Value provided = "
           << sampled;
  }
  if (marked > total) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The marked population " << marked
           << " should not exceed the total population " << total;
  }
  if (sampled > total) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The sample size " << sampled
           << " should not exceed the total population " << total;
  }
  return std::unique_ptr<HypergeometricDistribution>(
      new HypergeometricDistribution(total, marked, sampled));
}

double HypergeometricDistribution::PMF(double x) { return PMFImpl(x, marked_); }

double HypergeometricDistribution::PMFImpl(double x, int counted) {
  if (x < 0 || x > sampled_ || x > counted) return 0;
  if (total_ + x < counted + sampled_) return 0;
  double lpmf = std::lgamma(sampled_ + 1) + std::lgamma(counted + 1) +
                std::lgamma(total_ - counted + 1) +
                std::lgamma(total_ - sampled_ + 1) - std::lgamma(x + 1) -
                std::lgamma(sampled_ - x + 1) - std::lgamma(counted - x + 1) -
                std::lgamma(total_ + 1) -
                std::lgamma(total_ - sampled_ - counted + x + 1);
  return std::exp(lpmf);
}

double HypergeometricDistribution::CDF(double x) {
  x = std::floor(x);
  double mean = marked_ * static_cast<double>(sampled_) / total_;
  if (x > mean) {
    return 1 - CDFImpl(sampled_ - x - 1, total_ - marked_);
  } else {
    return CDFImpl(x, marked_);
  }
}

double HypergeometricDistribution::CDFImpl(double x, int counted) {
  double current_pmf = PMFImpl(x, counted);
  double result = 0;
  while (current_pmf > result * 1e-16) {
    result += current_pmf;
    current_pmf *= x;
    current_pmf *= total_ - counted - sampled_ + x;
    current_pmf /= counted - x + 1;
    current_pmf /= sampled_ - x + 1;
    --x;
  }
  return result;
}

double HypergeometricDistribution::FindQuantile(double quantile,
                                                bool complement) {
  if (quantile > 0.5) {
    quantile = 1 - quantile;
    complement = !complement;
  }
  if (complement) {
    return sampled_ - FindQuantileImpl(quantile, total_ - marked_) - 1;
  } else {
    return FindQuantileImpl(quantile, marked_);
  }
}

double HypergeometricDistribution::FindQuantileImpl(double quantile,
                                                    int counted) {
  double basic_bound = counted + sampled_ - total_ - 1;
  // An inverted tail bound gives a lower bound on the result
  double fancy_bound =
      sampled_ * (static_cast<double>(counted) / total_ -
                  std::sqrt(-std::log(quantile) / (2 * sampled_)));
  double result = -1;
  if (basic_bound > result) result = basic_bound;
  if (fancy_bound > result) result = fancy_bound;
  result = std::floor(result);

  double current_cdf = CDFImpl(result, counted);
  double current_pmf = PMFImpl(result, counted);
  while (current_cdf < quantile && result < sampled_) {
    if (current_pmf > 0) {
      current_pmf /= result + 1;
      current_pmf /= total_ - counted - sampled_ + result + 1;
      current_pmf *= counted - result;
      current_pmf *= sampled_ - result;
    } else {
      current_pmf = PMFImpl(result + 1, counted);
    }
    current_cdf += current_pmf;
    ++result;
  }
  --result;
  return result;
}

}  // namespace secagg
}  // namespace fcp
