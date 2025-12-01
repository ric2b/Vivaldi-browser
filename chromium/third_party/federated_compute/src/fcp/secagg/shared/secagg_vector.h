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

#ifndef FCP_SECAGG_SHARED_SECAGG_VECTOR_H_
#define FCP_SECAGG_SHARED_SECAGG_VECTOR_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/node_hash_map.h"
#include "absl/numeric/bits.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "fcp/base/monitoring.h"

// Represents an immutable vector of nonnegative integers, where each entry has
// the same specified bit width. This is used in the SecAgg package both to
// provide input to SecAggClient and by SecAggServer to provide its output (more
// specifically, inputs and outputs are of type
// unordered_map<std::string, SecAggVector>, where the key denotes a name
// associated with the vector).
//
// This class is backed by a packed byte representation of a uint64_t vector, in
// little endian order, where each consecutive bit_width sequence of bits of
// the packed vector corresponds to an integer value between
// 0 and modulus.

namespace fcp {
namespace secagg {

class SecAggVector {
 public:
  static constexpr uint64_t kMaxModulus = 1ULL << 62;  // max 62 bitwidth

  // Creates a SecAggVector of the specified modulus, using the specified
  // span of uint64s. The integers are converted into a packed byte
  // representation and stored in that format.
  //
  // Each element of span must be in [0, modulus-1].
  //
  // modulus itself must be > 1 and <= kMaxModulus.
  SecAggVector(absl::Span<const uint64_t> span, uint64_t modulus,
               bool branchless_codec = false);

  // Creates a SecAggVector from the given little-endian packed byte
  // representation. The packed representation should have num_elements longs,
  // each of bit_width length (in bits).
  //
  // packed_bytes must be in the same format as the output of GetAsPackedBytes.
  //
  // modulus must be > 1 and <= kMaxModulus.
  //
  // For large strings, copying may be avoided by specifying an rvalue for
  // packed bytes, e.g. std::move(large_caller_string), which should move the
  // contents.
  SecAggVector(std::string packed_bytes, uint64_t modulus, size_t num_elements,
               bool branchless_codec = false);

  // Disallow memory expensive copying of SecAggVector.
  SecAggVector(const SecAggVector&) = delete;
  SecAggVector& operator=(const SecAggVector&) = delete;

  // Enable move semantics.
  SecAggVector(SecAggVector&& other) { other.MoveTo(this); }

  SecAggVector& operator=(SecAggVector&& other) {
    other.MoveTo(this);
    return *this;
  }

  // Calculates bitwith for the specified modulus.
  inline static int GetBitWidth(uint64_t modulus) {
    return static_cast<int>(absl::bit_width(modulus - 1ULL));
  }

  ABSL_MUST_USE_RESULT inline uint64_t modulus() const { return modulus_; }
  ABSL_MUST_USE_RESULT inline size_t bit_width() const { return bit_width_; }
  ABSL_MUST_USE_RESULT inline size_t num_elements() const {
    return num_elements_;
  }

  // Produces and returns a representation of this SecAggVector as a vector of
  // uint64_t. The returned vector is obtained by unpacking the stored packed
  // representation of the vector.
  ABSL_MUST_USE_RESULT std::vector<uint64_t> GetAsUint64Vector() const;

  // Returns the stored, compressed representation of the SecAggVector.
  // The bytes are stored in little-endian order, using only bit_width bits to
  // represent each element of the vector.
  ABSL_MUST_USE_RESULT inline const std::string& GetAsPackedBytes() const {
    CheckHasValue();
    return packed_bytes_;
  }

  // Takes out the stored, compressed representation of the SecAggVector.
  // This call "consumes" the SecAggVector instance, and after that it becomes
  // invalid.
  // The bytes are stored in little-endian order, using only bit_width bits to
  // represent each element of the vector.
  ABSL_MUST_USE_RESULT inline std::string TakePackedBytes() && {
    CheckHasValue();
    modulus_ = 0;
    bit_width_ = 0;
    num_elements_ = 0;
    return std::move(packed_bytes_);
  }

  inline friend bool operator==(const SecAggVector& lhs,
                                const SecAggVector& rhs) {
    return lhs.packed_bytes_ == rhs.packed_bytes_;
  }

  // Decoder for unpacking SecAggVector values one by one.
  class Decoder {
   public:
    explicit Decoder(const SecAggVector& v)
        : Decoder(v.packed_bytes_, v.modulus_) {}

    explicit Decoder(absl::string_view packed_bytes, uint64_t modulus);

    // Unpacks and returns the next value.
    // Result of this operation is undetermined when the decoder has already
    // decoded all values. For performance reasons ReadValue doesn't validate
    // the state.
    uint64_t ReadValue();

   private:
    inline void ReadData();

    const char* read_cursor_;
    const char* const cursor_sentinel_;
    uint64_t cursor_read_value_;
    uint64_t scratch_;
    int read_cursor_bit_;
    uint8_t bit_width_;
    const uint64_t mask_;
    uint64_t modulus_;
  };

  // Coder for packing SecAggVector values one by one.
  class Coder {
   public:
    explicit Coder(uint64_t modulus, int bit_width, size_t num_elements);

    // Pack and write value to packed buffer.
    void WriteValue(uint64_t value);

    // Consumes the coder and creates SecAggVector with the packed buffer.
    SecAggVector Create() &&;

   private:
    std::string packed_bytes_;
    int num_bytes_needed_;
    uint64_t modulus_;
    int bit_width_;
    size_t num_elements_;
    char* write_cursor_;
    uint64_t target_cursor_value_;
    uint8_t starting_bit_position_;
  };

 private:
  std::string packed_bytes_;
  uint64_t modulus_;
  int bit_width_;
  size_t num_elements_;
  bool branchless_codec_;

  // Moves this object's value to the target one and resets this object's state.
  inline void MoveTo(SecAggVector* target) {
    target->modulus_ = modulus_;
    target->bit_width_ = bit_width_;
    target->num_elements_ = num_elements_;
    target->branchless_codec_ = branchless_codec_;
    target->packed_bytes_ = std::move(packed_bytes_);
    modulus_ = 0;
    bit_width_ = 0;
    num_elements_ = 0;
    branchless_codec_ = false;
  }

  // Verifies that this SecAggVector value can't be accessed after swapping it
  // with another SecAggVector via std::move().
  void CheckHasValue() const {
    FCP_CHECK(modulus_ > 0) << "SecAggVector has no value";
  }

  void PackUint64IntoByteStringAt(int index, uint64_t element);
  // A version without expensive branches or multiplies.
  void PackUint64IntoByteStringBranchless(absl::Span<const uint64_t> span);

  static ABSL_MUST_USE_RESULT uint64_t UnpackUint64FromByteStringAt(
      int index, int bit_width, const std::string& byte_string);
  // A version without expensive branches or multiplies.
  void UnpackByteStringToUint64VectorBranchless(
      std::vector<uint64_t>* long_vector) const;
};  // class SecAggVector

// This is equivalent to
// using SecAggVectorMap = absl::node_hash_map<std::string, SecAggVector>;
// except copy construction and assignment are explicitly prohibited.
class SecAggVectorMap : public absl::node_hash_map<std::string, SecAggVector> {
 public:
  using Base = absl::node_hash_map<std::string, SecAggVector>;
  using Base::Base;
  using Base::operator=;
  SecAggVectorMap(const SecAggVectorMap&) = delete;
  SecAggVectorMap& operator=(const SecAggVectorMap&) = delete;
};

// Unpacked vector is simply a pair vector<uint64_t> and the modulus used with
// each element.
class SecAggUnpackedVector : public std::vector<uint64_t> {
 public:
  explicit SecAggUnpackedVector(size_t size, uint64_t modulus)
      : vector(size), modulus_(modulus) {}

  explicit SecAggUnpackedVector(std::vector<uint64_t> elements,
                                uint64_t modulus)
      : vector(std::move(elements)), modulus_(modulus) {}

  ABSL_MUST_USE_RESULT inline uint64_t modulus() const { return modulus_; }
  ABSL_MUST_USE_RESULT inline size_t num_elements() const { return size(); }

  // Disallow memory expensive copying of SecAggVector.
  SecAggUnpackedVector(const SecAggUnpackedVector&) = delete;
  SecAggUnpackedVector& operator=(const SecAggUnpackedVector&) = delete;

  explicit SecAggUnpackedVector(const SecAggVector& other)
      : vector(other.num_elements()), modulus_(other.modulus()) {
    SecAggVector::Decoder decoder(other);
    for (auto& v : *this) {
      v = decoder.ReadValue();
    }
  }

  // Enable move semantics.
  SecAggUnpackedVector(SecAggUnpackedVector&& other)
      : vector(std::move(other)), modulus_(other.modulus_) {
    other.modulus_ = 0;
  }

  SecAggUnpackedVector& operator=(SecAggUnpackedVector&& other) {
    modulus_ = other.modulus_;
    other.modulus_ = 0;
    vector::operator=(std::move(other));
    return *this;
  }

  // Combines this vector with another (packed) vector by adding elements of
  // this vector to corresponding elements of the other vector.
  // It is assumed that both vectors have the same modulus. The modulus is
  // applied to each sum.
  void Add(const SecAggVector& other);

  // Combines this vector with another (unpacked) vector by adding elements of
  // this vector to corresponding elements of the other vector.
  // It is assumed that both vectors have the same modulus. The modulus is
  // applied to each sum.
  void Add(const SecAggUnpackedVector& other);

 private:
  uint64_t modulus_;
};

// This is mostly equivalent to
// using SecAggUnpackedVectorMap =
//     absl::node_hash_map<std::string, SecAggUnpackedVector>;
// except copy construction and assignment are explicitly prohibited and
// Add method is added.
class SecAggUnpackedVectorMap
    : public absl::node_hash_map<std::string, SecAggUnpackedVector> {
 public:
  using Base = absl::node_hash_map<std::string, SecAggUnpackedVector>;
  using Base::Base;
  using Base::operator=;
  SecAggUnpackedVectorMap(const SecAggUnpackedVectorMap&) = delete;
  SecAggUnpackedVectorMap& operator=(const SecAggUnpackedVectorMap&) = delete;

  explicit SecAggUnpackedVectorMap(const SecAggVectorMap& other) {
    for (auto& [name, vector] : other) {
      this->emplace(name, SecAggUnpackedVector(vector));
    }
  }

  // Combines this map with another (packed) map by adding all vectors in this
  // map to corresponding vectors in the other map.
  // It is assumed that names of vectors match in both maps.
  void Add(const SecAggVectorMap& other);

  // Combines this map with another (unpacked) map by adding all vectors in this
  // map to corresponding vectors in the other map.
  // It is assumed that names of vectors match in both maps.
  void Add(const SecAggUnpackedVectorMap& other);

  // Analogous to the above, as a static method. Also assumes that names of
  // vectors match in both maps.
  static std::unique_ptr<SecAggUnpackedVectorMap> AddMaps(
      const SecAggUnpackedVectorMap& a, const SecAggUnpackedVectorMap& b);
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_SECAGG_VECTOR_H_
