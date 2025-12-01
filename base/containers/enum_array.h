// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef BASE_CONTAINERS_ENUM_ARRAY_H_
#define BASE_CONTAINERS_ENUM_ARRAY_H_

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"

namespace base {

// An EnumArray is an array that uses enum values between a min and a max value
// (inclusive of both) as indices. It's essentially a wrapper around
// std::array<> with stronger type enforcement, more descriptive member function
// names, and a dedicated iterator interface.
//
// If the enum has gaps in the MinEnumValue..MaxEnumValue range, the array will
// have elements for those undefined values. This does not seem to be an issue
// from the point of view of the C++ standard

template <typename T, typename E, E MinEnumValue, E MaxEnumValue>
class EnumArray {
 private:
  static_assert(
      std::is_enum_v<E>,
      "Second template parameter of EnumArray must be an enumeration type");

  static constexpr bool InRange(E value) {
    return (value >= MinEnumValue) && (value <= MaxEnumValue);
  }

 public:
  using EnumType = E;
  using ValueType = T;
  static const E kMinValue = MinEnumValue;
  static const E kMaxValue = MaxEnumValue;
  static const size_t kValueCount =
      to_underlying(kMaxValue) - to_underlying(kMinValue) + 1;

  static_assert(kMinValue <= kMaxValue,
                "min value must be no greater than max value");

  // Allow use with ::testing::ValuesIn, which expects a value_type defined.
  using value_type = ValueType;

 private:
  // Declaration needed by Iterator.
  using ArrayType = std::array<T, kValueCount>;
  using ConstArrayType = std::array<T, kValueCount>;

 public:
  // Iterator is a bidirectiomal  iterator for EnumArray. It follows the common
  // STL input iterator interface (like std::unordered_set).
  //
  // Example usage, using a range-based for loop:
  //
  // EnumArray<SomeType> array; for (SomeType val : array) {
  // Process(val.second);
  // }
  //
  // Or using an explicit iterator (not recommended):
  //
  // for (EnumArray<...>::Iterator it = array.begin(); it != array.end(); it++)
  // {
  //   Process(it->second);
  // }
  //
  // The iteratoris generally subject to the same constraint as an std::arry
  // iterator.
  template <typename IteratorType, typename IteratorArrayType>
  class Iterator {
   public:
    using value_type = std::pair<EnumType, IteratorType>;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::bidirectional_iterator_tag;

    Iterator() : array_(nullptr), i_(kValueCount) {}
    ~Iterator() = default;

    Iterator(const Iterator&) = default;
    Iterator& operator=(const Iterator&) = default;

    Iterator(Iterator&&) = default;
    Iterator& operator=(Iterator&&) = default;

    friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
      return lhs.i_ == rhs.i_;
    }

    value_type operator*() const {
      DCHECK(Good());
      return {FromIndex(i_), array_->at(i_)};
    }

    Iterator& operator++() {
      CHECK(Good());
      i_++;
      return *this;
    }

    Iterator operator++(int) {
      CHECK(Good());
      Iterator old(*this);
      i_++;
      return std::move(old);
    }

    Iterator& operator--() {
      CHECK(Good());
      i_--;
      return *this;
    }

    Iterator operator--(int) {
      CHECK(Good());
      Iterator old(*this);
      i_--;
      return std::move(old);
    }

   private:
    friend Iterator<ValueType&, ArrayType> EnumArray::begin();
    friend Iterator<const ValueType&, const ArrayType> EnumArray::begin() const;
    friend Iterator<const ValueType&, const ArrayType> EnumArray::c_begin()
        const;

    explicit Iterator<ValueType&, ArrayType>(EnumArray& array)
        : array_(&array.array_), i_(0) {}
    explicit Iterator<const ValueType&, const ArrayType>(const EnumArray& array)
        : array_(&array.array_), i_(0) {}

    // Returns true iff the iterator points within an array and it.
    bool Good() const { return array_ && i_ >= 0 && i_ < array_->size(); }

    raw_ptr<IteratorArrayType> array_;
    size_t i_;
  };

  EnumArray() = default;

  ~EnumArray() = default;

  template <typename... Args>
  explicit constexpr EnumArray(Args&&... args)
      : array_(std::array<T, kValueCount>({args...})) {}

  EnumArray(const EnumArray&) = default;
  EnumArray& operator=(const EnumArray&) = default;
  EnumArray(EnumArray&&) = default;
  EnumArray& operator=(EnumArray&&) = default;

  /* Element access */
  T& at(E pos) { return array_.at(ToIndex(pos)); }
  const T& at(E pos) const { return array_.at(ToIndex(pos)); }

  T& operator[](E pos) { return array_[ToIndex(pos)]; }
  const T& operator[](E pos) const { return array_[ToIndex(pos)]; }

  T& front() { return array_.front(); }
  const T& front() const { return array_.front(); }

  T& back() { return array_.back(); }
  const T& back() const { return array_.back(); }

  T* data() { return array_.data(); }
  const T* data() const { return array_.data(); }

  /* Capacity */
  constexpr bool empty() const { return array_.empty(); }
  constexpr size_t size() const { return array_.size(); }
  constexpr size_t max_size() const { return array_.max_size(); }

  /* Operations */
  void fill(const T& value) { array_.fill(value); }
  void swap(EnumArray<T, E, MinEnumValue, MaxEnumValue>& other) {
    array_.swap(other.array_);
  }

  // Returns an iterator pointing to the first element (if any).
  Iterator<ValueType&, ArrayType> begin() {
    return Iterator<ValueType&, ArrayType>(*this);
  }
  Iterator<const ValueType&, const ArrayType> begin() const {
    return Iterator<const ValueType&, const ArrayType>(*this);
  }
  Iterator<const ValueType&, const ArrayType> c_begin() const {
    return Iterator<const ValueType&, const ArrayType>(*this);
  }

  // Returns an iterator that does not point to any element, but to the position
  // that follows the last element in the array.
  Iterator<ValueType&, ArrayType> end() {
    return Iterator<ValueType&, ArrayType>();
  }
  Iterator<const ValueType&, const ArrayType> end() const {
    return Iterator<const ValueType&, const ArrayType>();
  }
  Iterator<const ValueType&, const ArrayType> c_end() const {
    return Iterator<const ValueType&, const ArrayType>();
  }

  /* Comparisons*/
  friend bool operator==(const EnumArray&, const EnumArray&) = default;
  friend bool operator<=>(const EnumArray&, const EnumArray&) = default;

 private:
  // Converts a value to/from an index into |enums_|.
  static constexpr size_t ToIndex(E value) {
    CHECK(InRange(value));
    return static_cast<size_t>(to_underlying(value)) -
           static_cast<size_t>(to_underlying(MinEnumValue));
  }

  static E FromIndex(size_t i) {
    DCHECK_LT(i, kValueCount);
    return static_cast<E>(to_underlying(MinEnumValue) + i);
  }

  ArrayType array_;
};
}  // namespace base

#endif  // BASE_CONTAINERS_ENUM_ARRAY_H_
