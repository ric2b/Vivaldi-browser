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

#include "fcp/secagg/shared/shamir_secret_sharing.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "fcp/secagg/shared/math.h"
#include "openssl/rand.h"

namespace fcp {
namespace secagg {

const uint64_t ShamirSecretSharing::kPrime;
constexpr size_t kSubsecretSize = sizeof(uint32_t);

ShamirSecretSharing::ShamirSecretSharing() = default;

std::vector<ShamirShare> ShamirSecretSharing::Share(
    int threshold, int num_shares, const std::string& to_share) {
  FCP_CHECK(!to_share.empty()) << "to_share must not be empty";
  FCP_CHECK(num_shares > 1) << "num_shares must be greater than 1";
  FCP_CHECK(2 <= threshold && threshold <= num_shares)
      << "threshold must be at least 2 and at most num_shares";

  std::vector<uint32_t> subsecrets = DivideIntoSubsecrets(to_share);

  // Each ShamirShare is specified as a string of length 4 * subsecrets.size().
  // The first four characters of the ShamirShare are the share of the first
  // subsecret stored in big-endian order, and so on.
  std::vector<ShamirShare> shares(num_shares);
  for (auto& share : shares) {
    share.data.reserve(kSubsecretSize * subsecrets.size());
  }

  for (uint32_t subsecret : subsecrets) {
    std::vector<uint32_t> coefficients;
    coefficients.reserve(threshold);
    coefficients.push_back(subsecret);

    for (int i = 1; i < threshold; ++i) {
      coefficients.push_back(RandomFieldElement());
    }

    for (int i = 0; i < num_shares; ++i) {
      // The client with id x gets the share of the polynomial evaluated at x+1.
      uint32_t subshare = EvaluatePolynomial(coefficients, i + 1);
      // Big-endian encoding
      shares[i].data += IntToByteString(subshare);
    }
  }
  return shares;
}

StatusOr<std::string> ShamirSecretSharing::Reconstruct(
    int threshold, const std::vector<ShamirShare>& shares, int secret_length) {
  FCP_CHECK(threshold > 1) << "threshold must be at least 2";
  FCP_CHECK(secret_length > 0) << "secret_length must be positive";
  FCP_CHECK(static_cast<int>(shares.size()) >= threshold)
      << "A vector of size " << shares.size()
      << " was provided, but threshold was specified as " << threshold;

  // The max possible number of subsecrets is based on the secret_length.
  int max_num_subsecrets =
      ((8 * secret_length) + kBitsPerSubsecret - 1) / kBitsPerSubsecret;
  // The number of subsecrets may be different due to compatibility with the
  // legacy Java implementation and may be smaller than max_num_subsecrets.
  // The actual number is determined below.
  int num_subsecrets = 0;

  // The X values of the participating clients' shares. The i-th share will be
  // given an X value of i+1, to account for the fact that shares are 0-indexed.
  // We want exactly threshold participating clients.
  std::vector<int> x_values;

  for (int i = 0; i < static_cast<int>(shares.size()) &&
                  static_cast<int>(x_values.size()) < threshold;
       ++i) {
    if (shares[i].data.empty()) {
      continue;
    }

    FCP_CHECK(shares[i].data.size() % kSubsecretSize == 0)
        << "Share with index " << i << " is invalid: a share of size "
        << shares[i].data.size() << " was provided but a multiple of "
        << kSubsecretSize << " is expected";
    if (num_subsecrets == 0) {
      num_subsecrets = static_cast<int>(shares[i].data.size() / kSubsecretSize);
      FCP_CHECK(num_subsecrets > 0 && num_subsecrets <= max_num_subsecrets)
          << "Share with index " << i << " is invalid: "
          << "the number of subsecrets is " << num_subsecrets
          << " but between 1 and " << max_num_subsecrets << " is expected";
    } else {
      FCP_CHECK(shares[i].data.size() == num_subsecrets * kSubsecretSize)
          << "Share with index " << i << " is invalid: "
          << "all shares must match sizes: "
          << "shares[i].data.size() = " << shares[i].data.size()
          << ", num_subsecrets = " << num_subsecrets;
    }
    x_values.push_back(i + 1);
  }
  if (static_cast<int>(x_values.size()) < threshold) {
    return FCP_STATUS(FAILED_PRECONDITION)
           << "Only " << x_values.size()
           << " valid shares were provided, but threshold was specified as "
           << threshold;
  }

  // Recover the sharing polynomials using Lagrange polynomial interpolation.
  std::vector<uint32_t> coefficients = LagrangeCoefficients(x_values);
  std::vector<uint32_t> subsecrets;
  for (int i = 0; i < num_subsecrets; ++i) {
    subsecrets.push_back(0);
    for (int j = 0; j < static_cast<int>(x_values.size()); ++j) {
      int share_index = x_values[j] - 1;
      uint32_t subshare = 0;
      // Big-endian decoding
      for (int k = 0; k < kSubsecretSize; ++k) {
        subshare <<= 8;
        subshare += static_cast<uint8_t>(
            shares[share_index].data[kSubsecretSize * i + k]);
      }
      subsecrets[i] += MultiplyMod(subshare, coefficients[j], kPrime);
      subsecrets[i] %= kPrime;
    }
  }

  return RebuildFromSubsecrets(subsecrets, secret_length);
}

// Helper function for ModInverse.
static uint32_t ModPow(uint32_t x, uint32_t y) {
  if (y == 0) {
    return 1;
  }
  uint32_t p = ModPow(x, y / 2) % ShamirSecretSharing::kPrime;
  uint32_t q = MultiplyMod(p, p, ShamirSecretSharing::kPrime);
  return ((y & 0x01) == 0) ? q : MultiplyMod(x, q, ShamirSecretSharing::kPrime);
}

uint32_t ShamirSecretSharing::ModInverse(uint32_t n) {
  FCP_CHECK(n > 0 && n < kPrime) << "Invalid value " << n << " for ModInverse";
  while (inverses_.size() < n) {
    // Fermat's Little Theorem guarantees n^-1 = n^(P-2) mod P.
    inverses_.push_back(ModPow(inverses_.size() + 1, kPrime - 2));
  }
  return inverses_[n - 1];
}

std::vector<uint32_t> ShamirSecretSharing::LagrangeCoefficients(
    const std::vector<int>& x_values) {
  FCP_CHECK(x_values.size() > 1) << "Must have at least 2 x_values";
  for (int x : x_values) {
    FCP_CHECK(x > 0) << "x_values must all be positive, but got a value of "
                     << x;
  }

  if (x_values == last_lc_input_) {
    return last_lc_output_;
  }
  last_lc_input_ = x_values;
  last_lc_output_.clear();

  for (int i = 0; i < static_cast<int>(x_values.size()); ++i) {
    last_lc_output_.push_back(1);
    for (int j = 0; j < static_cast<int>(x_values.size()); ++j) {
      if (i == j) {
        continue;
      }
      last_lc_output_[i] = MultiplyMod(last_lc_output_[i], x_values[j], kPrime);
      if (x_values[j] > x_values[i]) {
        last_lc_output_[i] = MultiplyMod(
            last_lc_output_[i], ModInverse(x_values[j] - x_values[i]), kPrime);
      } else {
        // Factor out -1 (mod kPrime)
        last_lc_output_[i] =
            MultiplyMod(last_lc_output_[i], kPrime - 1, kPrime);
        last_lc_output_[i] = MultiplyMod(
            last_lc_output_[i], ModInverse(x_values[i] - x_values[j]), kPrime);
      }
    }
  }

  return last_lc_output_;
}

std::vector<uint32_t> ShamirSecretSharing::DivideIntoSubsecrets(
    const std::string& to_share) {
  std::vector<uint32_t> secret_parts(DivideRoundUp(
      static_cast<uint32_t>(to_share.size()) * 8, kBitsPerSubsecret));

  int bits_done = 0;
  auto current_subsecret = secret_parts.rbegin();

  // This is a packing of the bits in to_share into the bits in secret_parts.
  // The last 31 bits in to_share are kept in the same order and placed into
  // the last element of secret_parts, the second-to-last 31 bits are placed in
  // the second-to-last element, and so on. The high-order bit of every element
  // of secret_parts is 0. And the first element of secret_parts will contain
  // the remaining bits at the front of to_share.
  for (int i = to_share.size() - 1; i >= 0; --i) {
    // Ensure high-order characters are treated consistently
    uint8_t current_byte = static_cast<uint8_t>(to_share[i]);
    if (kBitsPerSubsecret - bits_done > 8) {
      *current_subsecret |= static_cast<uint32_t>(current_byte) << bits_done;
      bits_done += 8;
    } else {
      uint8_t current_byte_right =
          current_byte & (0xFF >> (8 - (kBitsPerSubsecret - bits_done)));
      *current_subsecret |= static_cast<uint32_t>(current_byte_right)
                            << bits_done;
      // Make sure we're not in the edge case where we're exactly done.
      if (!(i == 0 && bits_done + 8 == kBitsPerSubsecret)) {
        bits_done = (bits_done + 8) % kBitsPerSubsecret;
        ++current_subsecret;
        *current_subsecret |= current_byte >> (8 - bits_done);
      }
    }
  }
  // We should have been working on the 0th element of the vector.
  FCP_CHECK(current_subsecret + 1 == secret_parts.rend());
  return secret_parts;
}

std::string ShamirSecretSharing::RebuildFromSubsecrets(
    const std::vector<uint32_t>& secret_parts, int secret_length) {
  std::string secret(secret_length, 0);
  int bits_done = 0;
  auto subsecret = secret_parts.crbegin();
  // Exactly reverse the process in DivideIntoSubsecrets.
  for (int i = static_cast<int>(secret.size()) - 1;
       i >= 0 && subsecret != secret_parts.crend(); --i) {
    if (kBitsPerSubsecret - bits_done > 8) {
      secret[i] = static_cast<uint8_t>((*subsecret >> bits_done) & 0xFF);
      bits_done += 8;
    } else {
      uint8_t next_low_bits = static_cast<uint8_t>(*subsecret >> bits_done);
      ++subsecret;
      if (subsecret != secret_parts.crend()) {
        secret[i] = static_cast<uint8_t>(
            *subsecret & (0xFF >> (kBitsPerSubsecret - bits_done)));
      }
      bits_done = (bits_done + 8) % kBitsPerSubsecret;
      secret[i] <<= 8 - bits_done;
      secret[i] |= next_low_bits;
    }
  }

  return secret;
}

uint32_t ShamirSecretSharing::EvaluatePolynomial(
    const std::vector<uint32_t>& polynomial, uint32_t x) const {
  uint64_t sum = 0;

  for (int i = polynomial.size() - 1; i > 0; --i) {
    sum += polynomial[i];
    sum *= x;
    sum %= kPrime;
  }

  sum += polynomial[0];
  sum %= kPrime;

  return static_cast<uint32_t>(sum);
}

uint32_t ShamirSecretSharing::RandomFieldElement() {
  uint32_t rand = 0;
  do {
    rand = 0;
    RAND_bytes(reinterpret_cast<uint8_t*>(&rand), sizeof(uint32_t));
  } while (rand >= kPrime);
  return rand;
}

}  // namespace secagg
}  // namespace fcp
