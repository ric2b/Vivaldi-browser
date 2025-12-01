/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/secagg/shared/map_of_masks.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/numeric/bits.h"
#include "absl/strings/str_cat.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/compute_session_id.h"
#include "fcp/secagg/shared/input_vector_specification.h"
#include "fcp/secagg/shared/math.h"
#include "fcp/secagg/shared/prng.h"
#include "fcp/secagg/shared/secagg_vector.h"
#include "openssl/evp.h"

namespace fcp {
namespace secagg {

// Constant for backwards compatibility with legacy clients. Even though it is
// no longer needed, removing it would be disruptive due to making a large
// number of clients incompatible while not providing any benefits.
uint8_t kPrngSeedConstant = 0x02;

// We specifically avoid sample_bits == 64 to sidestep numerical precision
// issues, e.g. a uint64_t cannot represent the associated modulus.
constexpr int kMaxSampleBits = 63;

// We consider using at most 16 additional random bits from the underlying
// PRNG per sample.
//
constexpr int kMaxSampleBitsExpansion = 16;

static AesKey DigestKey(EVP_MD_CTX* mdctx, const std::string& prng_input,
                        int bit_width, const AesKey& prng_key) {
  int input_size = prng_input.size();
  std::string input_size_data = IntToByteString(input_size);
  std::string bit_width_data = IntToByteString(bit_width);
  FCP_CHECK(EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr));
  FCP_CHECK(EVP_DigestUpdate(mdctx, bit_width_data.c_str(), sizeof(int)));
  FCP_CHECK(EVP_DigestUpdate(mdctx, prng_key.data(), prng_key.size()));
  FCP_CHECK(EVP_DigestUpdate(mdctx, &kPrngSeedConstant, 1));
  FCP_CHECK(EVP_DigestUpdate(mdctx, input_size_data.c_str(), sizeof(int)));
  FCP_CHECK(EVP_DigestUpdate(mdctx, prng_input.c_str(), input_size));

  uint8_t digest[AesKey::kSize];
  uint32_t digest_length = 0;
  FCP_CHECK(EVP_DigestFinal_ex(mdctx, digest, &digest_length));
  FCP_CHECK(digest_length == AesKey::kSize);
  return AesKey(digest);
}

// Determines whether sample_bits_1 or sample_bits_2 will be more efficient
// for sampling uniformly from [0, modulus).
//
int choose_better_sample_bits(uint64_t modulus, int sample_bits_1,
                              int sample_bits_2) {
  FCP_CHECK(sample_bits_1 <= sample_bits_2);
  FCP_CHECK(sample_bits_2 <= kMaxSampleBits);
  FCP_CHECK(sample_bits_2 - sample_bits_1 <= kMaxSampleBitsExpansion);

  uint64_t sample_modulus_1 = 1ULL << sample_bits_1;
  FCP_CHECK(modulus <= sample_modulus_1);

  if (sample_bits_1 == sample_bits_2) {
    return sample_bits_1;
  }

  uint64_t sample_modulus_2 = 1ULL << sample_bits_2;
  uint64_t sample_modulus_2_over_1 = 1ULL << (sample_bits_2 - sample_bits_1);
  uint32_t cost_per_sample_1 = DivideRoundUp(sample_bits_1, 8);
  uint32_t cost_per_sample_2 = DivideRoundUp(sample_bits_2, 8);
  uint64_t modulus_reps_1 = sample_modulus_1 / modulus;
  uint64_t modulus_reps_2 = sample_modulus_2 / modulus;
  uint64_t cost_product_1 = cost_per_sample_1 * modulus_reps_1;
  uint64_t cost_product_2 =
      cost_per_sample_2 * modulus_reps_2 * sample_modulus_2_over_1;
  return cost_product_1 > cost_product_2 ? sample_bits_2 : sample_bits_1;
}

// Computes the sample_bits that minimizes the expected number of bytes of
// randomness that will be consumed when drawing a uniform sample from
// [0, modulus) using our rejection sampling algorithm.
//
int compute_best_sample_bits(uint64_t modulus) {
  int min_sample_bits = static_cast<int>(absl::bit_width(modulus - 1ULL));
  int max_sample_bits = std::min(kMaxSampleBitsExpansion,
                                 min_sample_bits + kMaxSampleBitsExpansion);
  int best_sample_bits = min_sample_bits;
  for (int sample_bits = min_sample_bits + 1; sample_bits <= max_sample_bits;
       sample_bits++) {
    best_sample_bits =
        choose_better_sample_bits(modulus, best_sample_bits, sample_bits);
  }
  return best_sample_bits;
}

// PrngBuffer implements the logic for generating pseudo-random masks while
// fetching and caching buffers of psedo-random uint8_t numbers.
// Two important factors of this implementation compared to using SecurePrng
// directly are:
// 1) The implementation is fully inlineable allowing the the compiler to
//    greatly optimize the resulting code.
// 2) Checking whether a new buffer of pseudo-random bytes needs to be filled is
//    done only once per mask as opposed to doing that for every byte, which
//    optimizes the most nested loop.
class PrngBuffer {
 public:
  PrngBuffer(std::unique_ptr<SecurePrng> prng, uint8_t msb_mask,
             size_t bytes_per_output)
      : prng_(static_cast<SecureBatchPrng*>(prng.release())),
        msb_mask_(msb_mask),
        bytes_per_output_(bytes_per_output),
        buffer_(prng_->GetMaxBufferSize()),
        buffer_end_(buffer_.data() + buffer_.size()) {
    FCP_CHECK((prng_->GetMaxBufferSize() % bytes_per_output) == 0)
        << "PRNG buffer size must be a multiple bytes_per_output.";
    FillBuffer();
  }

  inline uint64_t NextMask() {
    if (buffer_ptr_ == buffer_end_) {
      FillBuffer();
    }

    auto output = static_cast<uint64_t>((*buffer_ptr_++) & msb_mask_);
    for (size_t i = 1; i < bytes_per_output_; ++i) {
      output <<= 8UL;
      output |= static_cast<uint64_t>(*buffer_ptr_++);
    }
    return output;
  }

 private:
  inline int buffer_size() { return static_cast<int>(buffer_.size()); }

  inline void FillBuffer() {
    buffer_ptr_ = buffer_.data();
    FCP_CHECK(prng_->RandBuffer(buffer_.data(), buffer_size()) ==
              buffer_size());
  }

  std::unique_ptr<SecureBatchPrng> prng_;
  const uint8_t msb_mask_;
  const size_t bytes_per_output_;
  std::vector<uint8_t> buffer_;
  const uint8_t* buffer_ptr_ = nullptr;
  const uint8_t* const buffer_end_;
};

struct AddModAdapter {
  inline static uint64_t AddModImpl(uint64_t a, uint64_t b, uint64_t z) {
    return AddMod(a, b, z);
  }
  inline static uint64_t SubtractModImpl(uint64_t a, uint64_t b, uint64_t z) {
    return SubtractMod(a, b, z);
  }
};

struct AddModOptAdapter {
  inline static uint64_t AddModImpl(uint64_t a, uint64_t b, uint64_t z) {
    return AddModOpt(a, b, z);
  }
  inline static uint64_t SubtractModImpl(uint64_t a, uint64_t b, uint64_t z) {
    return SubtractModOpt(a, b, z);
  }
};

// Templated implementation of MapOfMasks that allows substituting
// AddMod and SubtractMod implementations.
template <typename TAdapter, typename TVector, typename TVectorMap>
inline std::unique_ptr<TVectorMap> MapOfMasksImpl(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort) {
  FCP_CHECK(prng_factory.SupportsBatchMode());

  auto map_of_masks = std::make_unique<TVectorMap>();
  std::unique_ptr<EVP_MD_CTX, void (*)(EVP_MD_CTX*)> mdctx(EVP_MD_CTX_create(),
                                                           EVP_MD_CTX_destroy);
  FCP_CHECK(mdctx.get());
  for (const InputVectorSpecification& vector_spec : input_vector_specs) {
    if (async_abort && async_abort->Signalled()) return nullptr;
    int bit_width =
        static_cast<int>(absl::bit_width(vector_spec.modulus() - 1ULL));
    std::string prng_input =
        absl::StrCat(session_id.data, IntToByteString(bit_width),
                     IntToByteString(vector_spec.length()), vector_spec.name());
    std::vector<uint64_t> mask_vector_buffer(vector_spec.length(), 0);

    bool modulus_is_power_of_two = (1ULL << bit_width == vector_spec.modulus());
    if (modulus_is_power_of_two) {
      // Because the modulus is a power of two, we can sample uniformly
      // simply by drawing the correct number of random bits.
      int bytes_per_output = DivideRoundUp(bit_width, 8);
      // msb = "most significant byte"
      size_t bits_in_msb = bit_width - ((bytes_per_output - 1) * 8);
      uint8_t msb_mask = (1UL << bits_in_msb) - 1;

      for (const auto& prng_key : prng_keys_to_add) {
        if (async_abort && async_abort->Signalled()) return nullptr;
        AesKey digest_key =
            DigestKey(mdctx.get(), prng_input, bit_width, prng_key);
        PrngBuffer prng(prng_factory.MakePrng(digest_key), msb_mask,
                        bytes_per_output);
        for (auto& v : mask_vector_buffer) {
          v = TAdapter::AddModImpl(v, prng.NextMask(), vector_spec.modulus());
        }
      }

      for (const auto& prng_key : prng_keys_to_subtract) {
        if (async_abort && async_abort->Signalled()) return nullptr;
        AesKey digest_key =
            DigestKey(mdctx.get(), prng_input, bit_width, prng_key);
        PrngBuffer prng(prng_factory.MakePrng(digest_key), msb_mask,
                        bytes_per_output);
        for (auto& v : mask_vector_buffer) {
          v = TAdapter::SubtractModImpl(v, prng.NextMask(),
                                        vector_spec.modulus());
        }
      }
    } else {
      // Rejection Sampling algorithm for arbitrary moduli.
      // Follows Algorithm 3 from:
      // "Fast Random Integer Generation in an Interval," Daniel Lemire, 2018.
      // https://arxiv.org/pdf/1805.10941.pdf.
      //
      // The inner loops are structured to avoid conditional branches
      // and the associated branch misprediction errors they would entail.
      //
      // We choose sample_bits to minimize the expected number of bytes
      // drawn from the PRNG.

      int sample_bits = compute_best_sample_bits(vector_spec.modulus());
      int bytes_per_output = DivideRoundUp(sample_bits, 8);
      // msb = "most significant byte"
      size_t bits_in_msb = sample_bits - ((bytes_per_output - 1) * 8);
      uint8_t msb_mask = (1UL << bits_in_msb) - 1;

      uint64_t sample_modulus = 1ULL << sample_bits;
      uint64_t rejection_threshold =
          (sample_modulus - vector_spec.modulus()) % vector_spec.modulus();

      for (const auto& prng_key : prng_keys_to_add) {
        if (async_abort && async_abort->Signalled()) return nullptr;
        AesKey digest_key =
            DigestKey(mdctx.get(), prng_input, sample_bits, prng_key);
        PrngBuffer prng(prng_factory.MakePrng(digest_key), msb_mask,
                        bytes_per_output);
        int i = 0;
        while (i < vector_spec.length()) {
          auto& v = mask_vector_buffer[i];
          auto mask = prng.NextMask();
          auto reject = mask < rejection_threshold;
          auto inc = reject ? 0 : 1;
          mask = reject ? 0 : mask;
          v = TAdapter::AddModImpl(v, mask % vector_spec.modulus(),
                                   vector_spec.modulus());
          i += inc;
        }
      }

      for (const auto& prng_key : prng_keys_to_subtract) {
        if (async_abort && async_abort->Signalled()) return nullptr;
        AesKey digest_key =
            DigestKey(mdctx.get(), prng_input, sample_bits, prng_key);
        PrngBuffer prng(prng_factory.MakePrng(digest_key), msb_mask,
                        bytes_per_output);
        int i = 0;
        while (i < vector_spec.length()) {
          auto& v = mask_vector_buffer[i];
          auto mask = prng.NextMask();
          auto reject = mask < rejection_threshold;
          auto inc = reject ? 0 : 1;
          mask = reject ? 0 : mask;
          v = TAdapter::SubtractModImpl(v, mask % vector_spec.modulus(),
                                        vector_spec.modulus());
          i += inc;
        }
      }
    }

    if (async_abort && async_abort->Signalled()) return nullptr;
    map_of_masks->emplace(vector_spec.name(),
                          TVector(mask_vector_buffer, vector_spec.modulus()));
  }
  return map_of_masks;
}

std::unique_ptr<SecAggVectorMap> MapOfMasks(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort) {
  return MapOfMasksImpl<AddModAdapter, SecAggVector, SecAggVectorMap>(
      prng_keys_to_add, prng_keys_to_subtract, input_vector_specs, session_id,
      prng_factory, async_abort);
}

std::unique_ptr<SecAggVectorMap> MapOfMasksV3(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort) {
  return MapOfMasksImpl<AddModOptAdapter, SecAggVector, SecAggVectorMap>(
      prng_keys_to_add, prng_keys_to_subtract, input_vector_specs, session_id,
      prng_factory, async_abort);
}

SecAggVector AddVectors(const SecAggVector& a, const SecAggVector& b) {
  FCP_CHECK(a.modulus() == b.modulus() && a.num_elements() == b.num_elements());
  uint64_t modulus = a.modulus();
  SecAggVector::Decoder decoder_a(a);
  SecAggVector::Decoder decoder_b(b);
  SecAggVector::Coder sum_coder(modulus, static_cast<int>(a.bit_width()),
                                a.num_elements());
  for (int remaining_elements = static_cast<int>(a.num_elements());
       remaining_elements > 0; --remaining_elements) {
    sum_coder.WriteValue((decoder_a.ReadValue() + decoder_b.ReadValue()) %
                         modulus);
  }
  return std::move(sum_coder).Create();
}

std::unique_ptr<SecAggVectorMap> AddMaps(const SecAggVectorMap& a,
                                         const SecAggVectorMap& b) {
  auto result = std::make_unique<SecAggVectorMap>();
  for (const auto& item : a) {
    result->emplace(item.first, AddVectors(item.second, b.at(item.first)));
  }
  return result;
}

std::unique_ptr<SecAggUnpackedVectorMap> UnpackedMapOfMasks(
    const std::vector<AesKey>& prng_keys_to_add,
    const std::vector<AesKey>& prng_keys_to_subtract,
    const std::vector<InputVectorSpecification>& input_vector_specs,
    const SessionId& session_id, const AesPrngFactory& prng_factory,
    AsyncAbort* async_abort) {
  return MapOfMasksImpl<AddModOptAdapter, SecAggUnpackedVector,
                        SecAggUnpackedVectorMap>(
      prng_keys_to_add, prng_keys_to_subtract, input_vector_specs, session_id,
      prng_factory, async_abort);
}

}  // namespace secagg
}  // namespace fcp
