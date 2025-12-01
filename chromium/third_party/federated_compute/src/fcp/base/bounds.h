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

/**
 * This file defines safe operations related to bounds checking and size
 * calculations - robust against overflow, etc.
 */

#ifndef FCP_BASE_BOUNDS_H_
#define FCP_BASE_BOUNDS_H_

#include <limits>
#include <type_traits>

#include "fcp/base/monitoring.h"

namespace fcp {

/**
 * Attempts to cast 'from' to 'To'. Returns true (and sets *to) if the cast is
 * lossless; otherwise, returns false without modifying *to.
 *
 * Examples:
 *   TryCastInteger<uint8_t, uint32_t>(1024, &to) => false
 *     static_cast<uint8_t>(1024) would yield 0
 *   TryCastInteger<uint8_t, uint32_t>(123, &to) => true
 *   TryCastInteger<uint32_t, int32_t>(123, &to) => true
 *   TryCastInteger<uint32_t, int32_t>(-1, &to) => false
 */
template <typename To, typename From>
bool TryCastInteger(From from, To* to) {
  static_assert(std::is_integral<To>::value && std::is_integral<From>::value,
                "Both types must be integral");

  if (std::is_signed<From>::value == std::is_signed<To>::value) {
    // Same sign: Easy!
    if (from < std::numeric_limits<To>::min() ||
        from > std::numeric_limits<To>::max()) {
      return false;
    }
  } else if (std::is_signed<From>::value && !std::is_signed<To>::value) {
    // Signed => Unsigned: Widening conversion would sign-extend 'from' first;
    // i.e. -1 would look larger than To's min(). Negative values are definitely
    // out of range anyway.  Positive values are effectively zero-extended,
    // which is fine.
    if (from < 0 || from > std::numeric_limits<To>::max()) {
      return false;
    }
  } else {
    // Unsigned => Signed: We don't want to mention min(), since widening
    // conversion of min() would have the same problem as in the prior case.
    if (from > std::numeric_limits<To>::max()) {
      return false;
    }
  }

  *to = static_cast<To>(from);
  return true;
}

/**
 * Casts from 'from' to 'To'. Check-fails if the cast is not lossless.
 * See also: TryCastInteger
 */
template <typename To, typename From>
To CastIntegerChecked(From from) {
  To to;
  FCP_CHECK(TryCastInteger(from, &to));
  return to;
}

/** Multiplies without the possibility of overflow. */
inline uint64_t SafeMultiply(uint32_t a, uint32_t b) {
  return static_cast<uint64_t>(a) * static_cast<uint64_t>(b);
}

/**
 * Represents an embedded address space as a pair of a starting address and a
 * size. This is a correspondence of the addresses [0, size) <=> [start, start +
 * size), for the embedded address space and this one, respectively.
 *
 * A ForeignPointer represents an address in an embedded address space (left).
 * Given a ForeignPointer and a supposed size, one can use
 * MapOutOfAddressSpace() to get a pointer in this address space (right),
 * subject to bounds checking.
 *
 * We require that start + size does not overflow; this is convenient for bounds
 * checks. Since start + size is the open part of the interval (one past the
 * end), that happens to mean that ~0 cannot be in bounds (irrelevant in
 * practice).
 */
struct AddressSpace {
  void* start;
  uint64_t size;

  /**
   * Returns a representation of the ambient, 'native' address space - it just
   * starts at address zero and has maximum size. Note that the highest address
   * (~0) is thus out of bounds.
   */
  static constexpr AddressSpace Current() {
    return AddressSpace{nullptr, ~static_cast<uint64_t>(0)};
  }

  /**
   * Returns an AddressSpace spanning mapping [0, size) <=> [start, start +
   * size).
   */
  static AddressSpace Embedded(void* start, uint64_t size) {
    uint64_t end;
    FCP_CHECK(
        !__builtin_add_overflow(reinterpret_cast<uint64_t>(start), size, &end));
    return AddressSpace{start, size};
  }
};

/**
 * An address in some AddressSpace. It can be translated to a pointer in this
 * address space with MapOutOfAddressSpace().
 */
struct ForeignPointer {
  uint64_t value;
};

/**
 * Translates a ForeignPointer out of an embedded AddressSpace, yielding a void*
 * in this address space. The pointer is understood to refer to (size * count)
 * bytes of memory (i.e. an array), as useful pointers tend to do.
 *
 * If that span does _not_ fully reside within the provided AddressSpace,
 * returns nullptr. Otherwise, returns space.start + ptr.value.
 *
 * This function is intended to behave safely for arbitrary values of 'ptr',
 * 'size', and 'count', perhaps provided by untrusted code. 'size' and 'count'
 * are provided separately for this reason (to save the caller from worrying
 * about multiplication overflow).
 */
inline void* MapOutOfAddressSpace(AddressSpace const& space, ForeignPointer ptr,
                                  uint32_t size, uint32_t count) {
  // Because the element size and count are each 32 bits, we can't overflow a 64
  // bit total_size.
  uint64_t total_size = SafeMultiply(size, count);

  // The span in the embedded space is [ptr, ptr + total_size).
  uint64_t ptr_end;
  if (__builtin_add_overflow(ptr.value, total_size, &ptr_end)) {
    return nullptr;
  }

  // The embedded address space ranges from [0, space.size). We know that ptr >=
  // 0 and that ptr <= ptr_end, so it is sufficient to check that ptr_end <=
  // space.size. Note that this allows ptr == space.size, iff total_size == 0.
  if (ptr_end > space.size) {
    return nullptr;
  }

  // AddressSpace requires that start + size does not overflow.
  //  - Since ptr_end <= space.size, space.start + ptr_end does not overflow.
  //  - Since ptr <= ptr_end, space.start + ptr does not overflow.
  // Therefore, we can return the offset span space.start + [ptr, ptr_end).
  return reinterpret_cast<void*>(reinterpret_cast<uint64_t>(space.start) +
                                 ptr.value);
}

}  // namespace fcp

#endif  // FCP_BASE_BOUNDS_H_
