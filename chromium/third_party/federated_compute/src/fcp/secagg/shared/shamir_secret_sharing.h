/*
 * Copyright 2018 Google LLC
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

#ifndef FCP_SECAGG_SHARED_SHAMIR_SECRET_SHARING_H_
#define FCP_SECAGG_SHARED_SHAMIR_SECRET_SHARING_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "fcp/secagg/shared/key.h"

namespace fcp {
namespace secagg {

// A ShamirShare represents one share of a shared secret, stored as binary data.
typedef struct ShamirShare {
  std::string data;
} ShamirShare;

// This class encapsulates all of the logic needed to perform t-of-n Shamir
// Secret Sharing on arbitrary-size secrets. For efficiency, the secrets are
// subdivided into 31-bit chunks called "subsecrets" - this allows us to use
// native unsigned 64-bit integer multiplication without worrying about
// overflow. This should be invisible to users of this class, as one ShamirShare
// still holds one user's share of the secret, represented as one share of each
// subsecret.
//
// This class is not thread-safe.

class ShamirSecretSharing {
 public:
  // This is the smallest 32-bit prime, 2^31+11. Everything this class does is
  // modulo kPrime. We need all values to be no more than 32 bits, so that we
  // can multiply using native types without overflow.
  static constexpr uint64_t kPrime = 2147483659L;

  // Constructs the ShamirSecretSharing object.
  ShamirSecretSharing();

  // Splits the arbitrary-length value stored in to_share into shares, following
  // threshold-out-of-num_shares Shamir Secret Sharing.
  //
  // The output is a vector such that the i-th element of the vector is the i-th
  // share of the secret.
  std::vector<ShamirShare> Share(int threshold, int num_shares,
                                 const std::string& to_share);

  // Convenience method to share a key instead of an arbitrary string.
  inline std::vector<ShamirShare> Share(int threshold, int num_shares,
                                        const Key& to_share) {
    return Share(threshold, num_shares, to_share.AsString());
  }

  // Reconstructs a secret, based on a vector of shares. The vector is
  // interpreted such that the i-th element of the vector is the i-th share. If
  // the i-th element of the vector is set to the default ShamirShare (an empty
  // string), that share is considered not to be present.
  //
  // secret_length should be set to the expected length of the reconstructed
  // secret, in bytes.
  //
  // At least threshold of the shares must be set to non-empty strings, or this
  // operation will fail.
  //
  // Reconstruct is most efficient when consecutive calls to this method use
  // shares with the same indices, because this allows for caching of
  // intermediate values that depend only on the x-value of the Shamir shares.
  absl::StatusOr<std::string> Reconstruct(
      int threshold, const std::vector<ShamirShare>& shares, int secret_length);

 private:
  // Returns the modular inverse of n mod kPrime, getting the value from a cache
  // if possible. If not, extends the cache to contain modular inverses from
  // integers from 1 to n.
  //
  // Fails if n is not between 1 and kPrime-1, inclusive.
  //
  // For most efficiency, call this method using the largest value of n that
  // will be needed before calling Reconstruct.
  uint32_t ModInverse(uint32_t n);

  // Returns the Lagrange coefficients needed to reconstruct secrets for this
  // exact set of shares. The Lagrange coefficient for the i-th value is
  // the product, for all j != i, of x_values[j] / (x_values[j] - x_values[i]).
  //
  // If this method is called twice in a row on the same input, the output is
  // returned from cache instead of being recomputed.
  std::vector<uint32_t> LagrangeCoefficients(const std::vector<int>& x_values);

  // Divides a secret into subsecrets. This takes place when Share is called,
  // before any further secret sharing work.
  std::vector<uint32_t> DivideIntoSubsecrets(const std::string& to_share);

  // Rebuilds a secret from subsecrets. This takes place at the end of
  // the Reconstruct operation, after all the secret reconstruction is already
  // finished.
  std::string RebuildFromSubsecrets(const std::vector<uint32_t>& secret_parts,
                                    int secret_length);

  // We will split our secret into sub-secrets of no more than 31 bits each.
  // This allows us to multiply two field elements using only native types.
  static constexpr int kBitsPerSubsecret = 31;

  // Returns a pseudorandom number uniformly between 0 and kPrime-1.
  uint32_t RandomFieldElement();

  // Returns the evaluation of x on the specified polynomial.
  // polynomial[i] is the i-degree coefficient of the polynomial.
  uint32_t EvaluatePolynomial(const std::vector<uint32_t>& polynomial,
                              uint32_t x) const;

  // Caches previously computed modular inverses.
  // inverses_[i] = (i+1)^-1 mod kPrime
  std::vector<uint32_t> inverses_;

  // Store a copy of the last input/output from LagrangeCoefficients.
  std::vector<int> last_lc_input_;
  std::vector<uint32_t> last_lc_output_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_SHAMIR_SECRET_SHARING_H_
