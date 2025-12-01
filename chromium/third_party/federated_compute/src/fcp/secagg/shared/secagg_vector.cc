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

#include "fcp/secagg/shared/secagg_vector.h"

#include <inttypes.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/math.h"

namespace fcp {
namespace secagg {

const uint64_t SecAggVector::kMaxModulus;

SecAggVector::SecAggVector(absl::Span<const uint64_t> span, uint64_t modulus,
                           bool branchless_codec)
    : modulus_(modulus),
      bit_width_(SecAggVector::GetBitWidth(modulus)),
      num_elements_(span.size()),
      branchless_codec_(branchless_codec) {
  FCP_CHECK(modulus_ > 1 && modulus_ <= kMaxModulus)
      << "The specified modulus is not valid: must be > 1 and <= "
      << kMaxModulus << "; supplied value : " << modulus_;
  // Ensuring the supplied vector has the appropriate modulus.
  for (uint64_t element : span) {
    FCP_CHECK(element >= 0)
        << "Only non negative elements are allowed in the vector.";
    FCP_CHECK(element < modulus_)
        << "The span does not have the appropriate modulus: element "
           "with value "
        << element << " found, max value allowed " << (modulus_ - 1ULL);
  }

  // Packs the long vector into a string, initialized to all null.
  if (branchless_codec_) {
    PackUint64IntoByteStringBranchless(span);
  } else {
    int num_bytes_needed =
        DivideRoundUp(static_cast<uint32_t>(num_elements_ * bit_width_), 8);
    packed_bytes_ = std::string(num_bytes_needed, '\0');
    for (int i = 0; static_cast<size_t>(i) < span.size(); ++i) {
      PackUint64IntoByteStringAt(i, span[i]);
    }
  }
}

SecAggVector::SecAggVector(std::string packed_bytes, uint64_t modulus,
                           size_t num_elements, bool branchless_codec)
    : packed_bytes_(std::move(packed_bytes)),
      modulus_(modulus),
      bit_width_(SecAggVector::GetBitWidth(modulus)),
      num_elements_(num_elements),
      branchless_codec_(branchless_codec) {
  FCP_CHECK(modulus_ > 1 && modulus_ <= kMaxModulus)
      << "The specified modulus is not valid: must be > 1 and <= "
      << kMaxModulus << "; supplied value : " << modulus_;
  int expected_num_bytes = DivideRoundUp(num_elements_ * bit_width_, 8);
  FCP_CHECK(packed_bytes_.size() == static_cast<size_t>(expected_num_bytes))
      << "The supplied string is not the right size for " << num_elements_
      << " packed elements: given string has a limit of "
      << packed_bytes_.size() << " bytes, " << expected_num_bytes
      << " bytes would have been needed.";
}

std::vector<uint64_t> SecAggVector::GetAsUint64Vector() const {
  CheckHasValue();
  std::vector<uint64_t> long_vector;
  if (branchless_codec_) {
    UnpackByteStringToUint64VectorBranchless(&long_vector);
  } else {
    long_vector.reserve(num_elements_);
    for (int i = 0; i < num_elements_; ++i) {
      long_vector.push_back(
          UnpackUint64FromByteStringAt(i, bit_width_, packed_bytes_));
    }
  }
  return long_vector;
}

void SecAggVector::PackUint64IntoByteStringAt(int index, uint64_t element) {
  // The element will be packed starting with the least significant (leftmost)
  // bits.
  //
  // TODO(team): Optimize out this extra per element computation.
  int leftmost_bit_position = index * bit_width_;
  int current_byte_index = leftmost_bit_position / 8;
  int bits_left_to_pack = bit_width_;

  // If leftmost_bit_position is in the middle of a byte, first fill that byte.
  if (leftmost_bit_position % 8 != 0) {
    int starting_bit_position = leftmost_bit_position % 8;
    int empty_bits_left = 8 - starting_bit_position;
    // Extract enough bits from "element" to fill the current byte, and shift
    // them to the correct position.
    uint64_t mask = (1ULL << std::min(empty_bits_left, bits_left_to_pack)) - 1L;
    uint64_t value_to_add = (element & mask) << starting_bit_position;
    packed_bytes_[current_byte_index] |= static_cast<char>(value_to_add);

    bits_left_to_pack -= empty_bits_left;
    element >>= empty_bits_left;
    current_byte_index++;
  }

  // Current bit position is now aligned with the start of the current byte.
  // Pack as many whole bytes as possible.
  uint64_t lower_eight_bit_mask = 255L;
  while (bits_left_to_pack >= 8) {
    packed_bytes_[current_byte_index] =
        static_cast<char>(element & lower_eight_bit_mask);

    bits_left_to_pack -= 8;
    element >>= 8;
    current_byte_index++;
  }

  // Pack the remaining partial byte, if necessary.
  if (bits_left_to_pack > 0) {
    // there should be < 8 bits left, so pack all remaining bits at once.
    packed_bytes_[current_byte_index] |= static_cast<char>(element);
  }
}

uint64_t SecAggVector::UnpackUint64FromByteStringAt(
    int index, int bit_width, const std::string& byte_string) {
  // all the bits starting from, and including, this bit are copied.
  int leftmost_bit_position = index * bit_width;
  // byte containing the lowest order bit to be copied
  int leftmost_byte_index = leftmost_bit_position / 8;
  // all bits up to, but not including this bit, are copied.
  int right_boundary_bit_position = ((index + 1) * bit_width);
  // byte containing the highest order bit to copy
  int rightmost_byte_index = (right_boundary_bit_position - 1) / 8;

  // Special case: when the entire long value to unpack is contained in a single
  // byte, then extract that long value in a single step.
  if (leftmost_byte_index == rightmost_byte_index) {
    int num_bits_to_skip = (leftmost_bit_position % 8);
    int mask = ((1 << bit_width) - 1) << num_bits_to_skip;
    // drop the extraneous bits below and above the value to unpack.
    uint64_t unpacked_element =
        (byte_string[leftmost_byte_index] & mask) >> num_bits_to_skip;
    return unpacked_element;
  }

  // Normal case: the value to unpack spans one or more byte boundaries.
  // The element will be unpacked in reverse order, starting from the most
  // significant (rightmost) bits.
  int current_byte_index = rightmost_byte_index;
  uint64_t unpacked_element = 0;
  int bits_left_to_unpack = bit_width;

  // If right_boundary_bit_position is in the middle of a byte, unpack the bits
  // up to right_boundary_bit_position within that byte.
  if (right_boundary_bit_position % 8 != 0) {
    int bits_to_copy_from_current_byte = (right_boundary_bit_position % 8);
    int lower_bits_mask = (1 << bits_to_copy_from_current_byte) - 1;
    unpacked_element |= (byte_string[current_byte_index] & lower_bits_mask);

    bits_left_to_unpack -= bits_to_copy_from_current_byte;
    current_byte_index--;
  }

  // Current bit position is now aligned with a byte boundary. Unpack as many
  // whole bytes as possible.
  while (bits_left_to_unpack >= 8) {
    unpacked_element <<= 8;
    unpacked_element |= byte_string[current_byte_index] & 0xff;

    bits_left_to_unpack -= 8;
    current_byte_index--;
  }

  // Unpack the remaining partial byte, if necessary.
  if (bits_left_to_unpack > 0) {
    unpacked_element <<= bits_left_to_unpack;
    int bits_to_skip_in_current_byte = 8 - bits_left_to_unpack;
    unpacked_element |= (byte_string[current_byte_index] & 0xff) >>
                        bits_to_skip_in_current_byte;
  }

  return unpacked_element;
}

void SecAggVector::PackUint64IntoByteStringBranchless(
    const absl::Span<const uint64_t> span) {
  SecAggVector::Coder coder(modulus_, bit_width_, num_elements_);
  for (uint64_t element : span) {
    coder.WriteValue(element);
  }
  packed_bytes_ = std::move(coder).Create().TakePackedBytes();
}

void SecAggVector::UnpackByteStringToUint64VectorBranchless(
    std::vector<uint64_t>* long_vector) const {
  long_vector->resize(num_elements_);
  Decoder decoder(*this);
  for (uint64_t& element : *long_vector) {
    element = decoder.ReadValue();
  }
}

SecAggVector::Decoder::Decoder(absl::string_view packed_bytes, uint64_t modulus)
    : read_cursor_(packed_bytes.data()),
      cursor_sentinel_(packed_bytes.data() + packed_bytes.size()),
      cursor_read_value_(0),
      scratch_(0),
      read_cursor_bit_(0),
      bit_width_(SecAggVector::GetBitWidth(modulus)),
      mask_((1ULL << bit_width_) - 1),
      modulus_(modulus) {
  ReadData();
}

inline void SecAggVector::Decoder::ReadData() {
  static constexpr ssize_t kBlockSizeBytes = sizeof(cursor_read_value_);
  const ptrdiff_t bytes_remaining = cursor_sentinel_ - read_cursor_;
  // Here, we use memcpy() to avoid the undefined behavior of
  // reinterpret_cast<> on unaligned reads, opportunistically reading up to
  // eight bytes at a time.
  if (bytes_remaining >= kBlockSizeBytes) {
    memcpy(&cursor_read_value_, read_cursor_, kBlockSizeBytes);
  } else {
    memcpy(&cursor_read_value_, read_cursor_,
           bytes_remaining > 0 ? bytes_remaining : 0);
  }
  scratch_ |= cursor_read_value_ << static_cast<unsigned>(read_cursor_bit_);
}

uint64_t SecAggVector::Decoder::ReadValue() {
  static constexpr int kBlockSizeBits = sizeof(cursor_read_value_) * 8;
  // Get the current value.
  const uint64_t current_value = scratch_ & mask_;
  // Advance to the next value.
  scratch_ >>= bit_width_;
  int unwritten_bits = read_cursor_bit_;
  read_cursor_bit_ -= bit_width_;
  // Because we read in eight byte chunks on byte boundaries, and only keep
  // eight bytes of scratch, a portion of the read could not fit, and now
  // belongs at the back of scratch. The following assignments are compiled
  // to a branchless conditional move on Clang X86_64 and Clang ARMv{7, 8}.
  int read_bit_shift = bit_width_ - unwritten_bits;
  unsigned int right_shift_value = read_bit_shift > 0 ? read_bit_shift : 0;
  unsigned int left_shift_value = read_bit_shift < 0 ? -read_bit_shift : 0;
  cursor_read_value_ >>= right_shift_value;
  cursor_read_value_ <<= left_shift_value;
  scratch_ |= cursor_read_value_;
  int valid_scratch_bits = kBlockSizeBits - bit_width_ + unwritten_bits;
  valid_scratch_bits = (valid_scratch_bits > kBlockSizeBits)
                           ? kBlockSizeBits
                           : valid_scratch_bits;
  int new_read_cursor_bit =
      read_cursor_bit_ +
      static_cast<signed>(
          (static_cast<unsigned>(valid_scratch_bits - read_cursor_bit_) & ~7U));
  new_read_cursor_bit = new_read_cursor_bit == kBlockSizeBits
                            ? static_cast<int>(kBlockSizeBits - 8)
                            : new_read_cursor_bit;
  read_cursor_ +=
      static_cast<unsigned>((new_read_cursor_bit - read_cursor_bit_)) / 8;
  read_cursor_bit_ = new_read_cursor_bit;
  ReadData();
  // The current_value is guaranteed to be in [0, 2 * modulus_) range due to the
  // relationship between modulus_ and bit_width_, and therefore the below
  // statement guarantees the return value to be in [0, modulus_) range.
  return current_value < modulus_ ? current_value : current_value - modulus_;
}

SecAggVector::Coder::Coder(uint64_t modulus, int bit_width, size_t num_elements)
    : modulus_(modulus),
      bit_width_(bit_width),
      num_elements_(num_elements),
      target_cursor_value_(0),
      starting_bit_position_(0) {
  num_bytes_needed_ =
      DivideRoundUp(static_cast<uint32_t>(num_elements_ * bit_width_), 8);
  // The branchless variant assumes eight bytes of scratch space.
  // The string is resized to the correct size at the end.
  packed_bytes_ = std::string(num_bytes_needed_ + 8, '\0');
  write_cursor_ = &packed_bytes_[0];
}

void SecAggVector::Coder::WriteValue(uint64_t value) {
  static constexpr size_t kBlockSize = sizeof(target_cursor_value_);
  // Here, we use memcpy() to avoid the undefined behavior of
  // reinterpret_cast<> on unaligned stores, opportunistically writing eight
  // bytes at a time.
  target_cursor_value_ &= (1ULL << starting_bit_position_) - 1;
  target_cursor_value_ |= value << starting_bit_position_;
  std::memcpy(write_cursor_, &target_cursor_value_, kBlockSize);
  const auto new_write_cursor =
      write_cursor_ + (starting_bit_position_ + bit_width_) / 8;
  const auto new_starting_bit_position =
      (starting_bit_position_ + bit_width_) % 8;
  // Because we write in eight byte chunks, a portion of element may have
  // been missed, and now belongs at the front of target_cursor_value. The
  // following assignments are compiled to a branchless conditional move on
  // Clang X86_64 and Clang ARMv{7, 8}.
  auto runt_cursor_value =
      new_starting_bit_position
          ? value >> (static_cast<unsigned>(kBlockSize * 8 -
                                            starting_bit_position_) &
                      (kBlockSize * 8 - 1))  // Prevent unused UB warning.
          : 0;
  // Otherwise, remove fully written values from our scratch space.
  target_cursor_value_ >>=
      (static_cast<unsigned>(new_write_cursor - write_cursor_) * 8) &
      (kBlockSize * 8 - 1);  // Prevent unused UB warning.
  target_cursor_value_ = (new_write_cursor - write_cursor_ == kBlockSize)
                             ? runt_cursor_value
                             : target_cursor_value_;
  write_cursor_ = new_write_cursor;
  starting_bit_position_ = new_starting_bit_position;
}

SecAggVector SecAggVector::Coder::Create() && {
  static constexpr size_t kBlockSize = sizeof(target_cursor_value_);
  std::memcpy(write_cursor_, &target_cursor_value_, kBlockSize);
  packed_bytes_.resize(num_bytes_needed_);
  return SecAggVector(std::move(packed_bytes_), modulus_, num_elements_,
                      /* branchless_codec=*/true);
}

void SecAggUnpackedVector::Add(const SecAggVector& other) {
  FCP_CHECK(num_elements() == other.num_elements());
  FCP_CHECK(modulus() == other.modulus());
  SecAggVector::Decoder decoder(other);
  for (auto& v : *this) {
    v = AddModOpt(v, decoder.ReadValue(), modulus());
  }
}

void SecAggUnpackedVector::Add(const SecAggUnpackedVector& other) {
  FCP_CHECK(num_elements() == other.num_elements());
  FCP_CHECK(modulus() == other.modulus());
  auto it = other.begin();
  for (auto& v : *this) {
    v = AddModOpt(v, *it++, modulus());
  }
}

void SecAggUnpackedVectorMap::Add(const SecAggVectorMap& other) {
  FCP_CHECK(size() == other.size());
  for (auto& [name, vector] : *this) {
    auto it = other.find(name);
    FCP_CHECK(it != other.end());
    vector.Add(it->second);
  }
}

void SecAggUnpackedVectorMap::Add(const SecAggUnpackedVectorMap& other) {
  FCP_CHECK(size() == other.size());
  for (auto& [name, vector] : *this) {
    auto it = other.find(name);
    FCP_CHECK(it != other.end());
    vector.Add(it->second);
  }
}

std::unique_ptr<SecAggUnpackedVectorMap> SecAggUnpackedVectorMap::AddMaps(
    const SecAggUnpackedVectorMap& a, const SecAggUnpackedVectorMap& b) {
  auto result = std::make_unique<SecAggUnpackedVectorMap>();
  for (const auto& entry : a) {
    auto name = entry.first;
    auto length = entry.second.num_elements();
    auto modulus = entry.second.modulus();
    const auto& a_at_name = entry.second;
    const auto& b_at_name = b.at(name);
    SecAggUnpackedVector result_vector(length, modulus);
    for (int j = 0; j < length; ++j) {
      result_vector[j] = AddModOpt(a_at_name[j], b_at_name[j], modulus);
    }
    result->emplace(name, std::move(result_vector));
  }
  return result;
}

}  // namespace secagg
}  // namespace fcp
