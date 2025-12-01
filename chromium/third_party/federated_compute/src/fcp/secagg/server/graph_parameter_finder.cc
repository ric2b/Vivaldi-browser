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
#include <cmath>
#include <memory>
#include <optional>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/server/distribution_utilities.h"
#include "fcp/secagg/server/secagg_server_enums.pb.h"

namespace fcp {
namespace secagg {

class HararyGraphParameterFinder {
 public:
  // Checks that parameters have valid values and returns an instance of the
  // class
  static StatusOr<std::unique_ptr<HararyGraphParameterFinder>> Create(
      int number_of_clients, double adversarial_rate, double dropout_rate,
      AdversaryClass adversary_class) {
    if (number_of_clients <= 0) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "The number of clients should greater than zero. Value "
                "provided = "
             << number_of_clients;
    }
    if (number_of_clients > kMaxNumberOfClients) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "The valid number of clients is upper bounded by 1M. There is "
                "no "
                "fundamental reason for that, and this "
                "parameter finder should work for that setting. Just add the "
                "corresponding tests.";
    }
    if (adversarial_rate < 0 || adversarial_rate >= 1) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "The adversarial rate should be in [0,1). Value provided = "
             << adversarial_rate;
    }
    if (dropout_rate < 0 || dropout_rate >= 1) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "The dropout rate should be in [0,1). Value provided = "
             << dropout_rate;
    }
    FCP_CHECK(adversary_class == AdversaryClass::CURIOUS_SERVER ||
              adversary_class == AdversaryClass::SEMI_MALICIOUS_SERVER ||
              adversary_class == AdversaryClass::NONE)
        << "CURIOUS_SERVER, SEMI_MALICIOUS_SERVER, and NONE are the only "
           "supported "
           "adversary classes.";
    if (adversary_class == AdversaryClass::NONE && adversarial_rate > 0) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "The no-adversary setting expects that adversarial_rate = 0. "
                "Value provided = "
             << adversarial_rate;
    }
    if ((adversary_class == AdversaryClass::CURIOUS_SERVER ||
         adversary_class == AdversaryClass::NONE) &&
        adversarial_rate + dropout_rate > .9) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "In the semi-honest and no-adversary settings, "
                "adversarial_rate + dropout_rate "
                "<= 0.9 must hold for the instance to be feasible. Values "
                "provided = "
             << adversarial_rate << " and " << dropout_rate;
    }
    if (adversary_class == AdversaryClass::SEMI_MALICIOUS_SERVER &&
        adversarial_rate + 2 * dropout_rate > .9) {
      return FCP_STATUS(FAILED_PRECONDITION)
             << "In the semi-malicious setting, adversarial_rate + "
                "2*dropout_rate <= 0.9 must hold for the instance to be "
                "feasible. Values provided = "
             << adversarial_rate << " and " << dropout_rate;
    }
    return absl::WrapUnique(new HararyGraphParameterFinder(
        number_of_clients, adversarial_rate, dropout_rate, adversary_class));
  }

  // Returns the degree of a Harary graph and threshold of a Shamir secret
  // sharing scheme that result in an instance of subgraph-secagg with
  // statistical security [kSecurityParameter] and failure probability less
  // that 2**(-[kCorrectnessParameter]), assuming [number_of_clients_]
  // participants and a fraction of [adversarial_rate_] (resp. [dropout_rate_])
  // adversarial clients (resp. dropouts).
  StatusOr<HararyGraphParameters> ComputeDegreeAndThreshold() {
    for (int number_of_neighbors = 2;
         number_of_neighbors < number_of_clients_ - 1;
         number_of_neighbors += 2) {
      auto threshold = CheckNumberOfNeighbors(number_of_neighbors);
      if (threshold.has_value()) {
        HararyGraphParameters params = {number_of_clients_, number_of_neighbors,
                                        threshold.value()};
        return params;
      }
    }
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Parameters consistent with the provided aggregation "
              "requirements were not found. Unless the number of clients is "
              "small (<500) and adversarial rate plus dropout rate are large "
              "(close to 1), this is an unlikely error that should be "
              "investigated as a bug.";
  }

 private:
  static constexpr int kMaxNumberOfClients = 1000000;

  // Statistical security parameter. Parameters found by
  // HararyGraphParameterFinder guarantee statistical security with probability
  // > 1-2^{-40}
  static constexpr double kSecurityParameter = 40;
  // Statistical correctness parameter. Parameters found by
  // HararyGraphParameterFinder guarantee a failure probability < 2^{-20}
  static constexpr double kCorrectnessParameter = 20;

  // Adding kSmall stops the floating point rounding error being magnified in
  // an insecure direction by the rounding. Being small it is unlikely to
  // introduce an error the other way either but that would only slightly hit
  // performance anyway.
  static constexpr double kSmall = 1e-14;

  int number_of_clients_;
  double adversarial_rate_;
  double dropout_rate_;
  AdversaryClass adversary_class_;

  // Returns a bound on the probability p of a random harary graph of
  // number_of_clients_ nodes and degree number_of_neighbors being disconnected
  // after adversarial and dropout nodes are removed.
  double LogProbOfDisconnectedRandomHarary(int number_of_neighbors) {
    // Note that any set of removed nodes disconnecting a Harary graph needs to
    // include number_of_neighbors/2 successive clients in two disjoint places
    // (in the ring forming the Harary graph). In our setting that means that
    // these clients need to be adversarial or dropouts. There are
    // number_of_clients_ ways to pick the first break and
    // number_of_clients_-number_of_neighbors-1 ways to pick the other, and this
    // double counts the pairs so dividing by two gives the number of gap pairs.
    // For each pair this probability is given by the ratio of the given
    // factorials. Multiplying by number_of_gap_pairs bounds the total
    // probability by a union bound.
    double log_number_of_gap_pairs =
        std::log(number_of_clients_) +
        std::log(number_of_clients_ - number_of_neighbors - 1) - std::log(2);
    int max_adversarial_clients = static_cast<int>(
        std::floor((adversarial_rate_ + kSmall) * number_of_clients_));
    int max_dropout_clients = static_cast<int>(
        std::floor((dropout_rate_ + kSmall) * number_of_clients_));
    int max_bad_clients = max_adversarial_clients + max_dropout_clients;

    if (number_of_neighbors > max_bad_clients) return -HUGE_VAL;
    double ret = log_number_of_gap_pairs + std::lgamma(max_bad_clients + 1) +
                 std::lgamma(number_of_clients_ - number_of_neighbors + 1) -
                 std::lgamma(number_of_clients_ + 1) -
                 std::lgamma(max_bad_clients - number_of_neighbors + 1);
    return ret;
  }

  // Checks if degree number_of_neighbors_ results in a secure and correct
  // protocol, and returns an appropriate threshold if so.
  std::optional<int> CheckNumberOfNeighbors(int number_of_neighbors) {
    // We split the security parameter evenly across the two bad events
    constexpr double kSecurityParameterPerEvent = kSecurityParameter + 1;
    const double kLogProbSecurityParameterPerEvent =
        -kSecurityParameterPerEvent * std::log(2);
    // We first check that the graph of honest surviving nodes is connected with
    // large enough probability
    if (LogProbOfDisconnectedRandomHarary(number_of_neighbors) >=
        kLogProbSecurityParameterPerEvent) {
      // The probability of the graph getting disconnected is not small enough
      return std::nullopt;
    }

    // We now check find threshold t such that (a) for every client i, the
    // number of adversarial neighbors of i is greater than t-1 with small
    // enough probability, and (b) that the number of surviving nodes of a
    // client is greater than t-1 with large enough probability.
    int upper_bound_adversarial_clients = static_cast<int>(
        std::floor((adversarial_rate_ + kSmall) * number_of_clients_));
    int lower_bound_surviving_clients = static_cast<int>(
        std::ceil((1 - dropout_rate_ - kSmall) * number_of_clients_));
    // Distribution of the number of adversarial neighbors of a client
    auto num_adversarial_neighbors = HypergeometricDistribution::Create(
        number_of_clients_, upper_bound_adversarial_clients,
        number_of_neighbors);
    // Distribution of the number of dropout neighbors of a client
    auto num_surviving_neighbors = HypergeometricDistribution::Create(
        number_of_clients_, lower_bound_surviving_clients, number_of_neighbors);

    // t1 is such that Pr[# of adversarial neighbors of a client > t1] <=
    // 2^{-security_parameter_per_event} / number_of_clients_

    // t2 is such that Pr[# of adversarial neighbors of a client <= t2] >=
    // 2^{-correctness_parameter_} / number_of_clients_

    // The result of the below quantile functions is an integer result rounded
    // outwards i.e. away from the median of the distribution.
    auto t1 = num_adversarial_neighbors.value()->FindQuantile(
        std::pow(2, -kSecurityParameterPerEvent) / number_of_clients_, true);
    auto t2 = num_surviving_neighbors.value()->FindQuantile(
        std::pow(2, -kCorrectnessParameter) / number_of_clients_);
    if (num_surviving_neighbors.value()->CDF(t2) <
        std::pow(2, -kCorrectnessParameter) / number_of_clients_) {
      t2++;
    }

    // In the semihonest case, the returned threshold must satisfy that t \in
    // (t1, t2]. To save computation when reconstructing shamir shares we simply
    // choose t1+1.

    // In the semi-malicious case t should be such that Pr[2t -
    // number_of_neighbors] < 2^{-security_parameter_ + 1} / number_of_clients_,
    // and thus we set t1 + 1 to be at least t1/2 + number_of_neighbors/2 + 1/2,
    // so that also in this case (t1, t2] defines the range of acceptable values
    // for t

    if (adversary_class_ == AdversaryClass::SEMI_MALICIOUS_SERVER) {
      t1 = std::ceil((t1 + number_of_neighbors - 1.25) / 2);
    }
    if (t2 <= t1) {
      return std::nullopt;
    }
    // The Shamir secret sharing implementation requires that threshold is >= 2
    return std::max(t1 + 1, 2.);
  }

  HararyGraphParameterFinder(int number_of_clients, double adversarial_rate,
                             double dropout_rate,
                             AdversaryClass adversary_class)
      : number_of_clients_(number_of_clients),
        adversarial_rate_(adversarial_rate),
        dropout_rate_(dropout_rate),
        adversary_class_(adversary_class) {}
};

StatusOr<HararyGraphParameters> ComputeHararyGraphParameters(
    int number_of_clients, SecureAggregationRequirements threat_model) {
  FCP_ASSIGN_OR_RETURN(
      auto pf, HararyGraphParameterFinder::Create(
                   number_of_clients, threat_model.adversarial_client_rate(),
                   threat_model.estimated_dropout_rate(),
                   threat_model.adversary_class()));
  return pf->ComputeDegreeAndThreshold();
}

Status CheckFullGraphParameters(int number_of_clients, int threshold,
                                SecureAggregationRequirements threat_model) {
  if (number_of_clients <= 0) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The number of clients should greater than zero. Value "
              "provided = "
           << number_of_clients;
  }
  if (threshold <= 1 || threshold > number_of_clients) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The threshold should be > 1 and <= number_of_clients. Value "
              "provided = "
           << threshold;
  }
  if (threat_model.adversarial_client_rate() < 0 ||
      threat_model.adversarial_client_rate() >= 1) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The adversarial rate should be in [0,1). Value provided = "
           << threat_model.adversarial_client_rate();
  }
  if (threat_model.estimated_dropout_rate() < 0 ||
      threat_model.estimated_dropout_rate() >= 1) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The dropout rate should be in [0,1). Value provided = "
           << threat_model.estimated_dropout_rate();
  }
  FCP_CHECK(threat_model.adversary_class() == AdversaryClass::CURIOUS_SERVER ||
            threat_model.adversary_class() ==
                AdversaryClass::SEMI_MALICIOUS_SERVER ||
            threat_model.adversary_class() == AdversaryClass::NONE)
      << "CURIOUS_SERVER, SEMI_MALICIOUS_SERVER, and NONE are the only "
         "supported "
         "adversary classes.";
  if (threshold < std::ceil((1 - threat_model.estimated_dropout_rate()) *
                            number_of_clients)) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "The threshold should be at least ceil(1 - dropout_rate * "
              "number_of_clients). Values provided are "
           << threshold << ", "
           << std::floor((1 - threat_model.estimated_dropout_rate()) *
                         number_of_clients);
  }
  if (threat_model.adversary_class() == AdversaryClass::CURIOUS_SERVER &&
      threshold <= std::ceil(threat_model.adversarial_client_rate() *
                             number_of_clients)) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "In the full-graph variant and CURIOUS_SERVER threat model, the "
              "threshold should be at least ceil(adversarial_client_rate * "
              "number_of_clients). Values provided are "
           << threshold << ", "
           << std::ceil(threat_model.adversarial_client_rate() *
                        number_of_clients);
  } else if (threat_model.adversary_class() ==
                 AdversaryClass::SEMI_MALICIOUS_SERVER &&
             threshold <= std::ceil((number_of_clients +
                                     threat_model.adversarial_client_rate() *
                                         number_of_clients) /
                                    2)) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "In the full-graph variant and SEMI_MALICIOUS_SERVER threat "
              "model, the threshold should be at least "
              "ceil((total_number_of_clients "
              "+ adversarial_client_rate * number_of_clients) / 2). "
              "Values provided are "
           << threshold << ", "
           << (number_of_clients +
               std::ceil(threat_model.adversarial_client_rate() *
                         number_of_clients) /
                   2);
  }
  return FCP_STATUS(OK);
}

}  // namespace secagg
}  // namespace fcp
