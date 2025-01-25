/* Copyright 2024 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/backends/cpu/runtime/sort_thunk.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/base/optimization.h"
#include "absl/container/inlined_vector.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "xla/backends/cpu/runtime/thunk.h"
#include "xla/layout_util.h"
#include "xla/primitive_util.h"
#include "xla/runtime/buffer_use.h"
#include "xla/service/buffer_assignment.h"
#include "xla/shape.h"
#include "xla/shape_util.h"
#include "xla/stream_executor/device_memory.h"
#include "xla/tsl/concurrency/async_value_ref.h"
#include "xla/util.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/logging.h"
#include "tsl/platform/statusor.h"
#include "tsl/profiler/lib/traceme.h"

namespace xla::cpu {

static absl::Status VerifySortInputs(absl::Span<const SortThunk::Input> inputs,
                                     int64_t dimension) {
  // We should have at least one input buffer.
  if (inputs.empty()) {
    return Internal("Inputs must not be empty");
  }

  // All inputs must have the same shape and layout (ignoring element type).
  auto equal = Shape::Equal().IgnoreElementType();
  const Shape& shape = inputs[0].shape;

  for (const SortThunk::Input& input : inputs) {
    if (!equal(shape, input.shape)) {
      return Internal("Inputs must have the same shape");
    }
  }

  // Check that sort dimension is valid.
  int64_t sort_dimension =
      dimension >= 0 ? dimension : shape.rank() + dimension;
  if (shape.rank() <= sort_dimension) {
    return Internal(
        "Shape of dimensions [%s] can't be sorted along dimension %d",
        absl::StrJoin(shape.dimensions(), ","), dimension);
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<SortThunk>> SortThunk::Create(
    Info info, absl::Span<const Input> inputs, int64_t dimension,
    bool is_stable, LessThan less_than) {
  TF_RETURN_IF_ERROR(VerifySortInputs(inputs, dimension));
  return absl::WrapUnique(new SortThunk(std::move(info), inputs, dimension,
                                        is_stable, std::move(less_than)));
}

absl::StatusOr<std::unique_ptr<SortThunk>> SortThunk::Create(
    Info info, absl::Span<const Input> inputs, int64_t dimension,
    bool is_stable, std::string comparator_name) {
  TF_RETURN_IF_ERROR(VerifySortInputs(inputs, dimension));
  return absl::WrapUnique(new SortThunk(std::move(info), inputs, dimension,
                                        is_stable, std::move(comparator_name)));
}

SortThunk::SortThunk(Info info, absl::Span<const Input> inputs,
                     int64_t dimension, bool is_stable, LessThan less_than)
    : Thunk(Kind::kSort, std::move(info)),
      inputs_(inputs.begin(), inputs.end()),
      dimension_(dimension),
      is_stable_(is_stable),
      less_than_(std::move(less_than)),
      less_than_ptr_(&*less_than_) {}

SortThunk::SortThunk(Info info, absl::Span<const Input> inputs,
                     int64_t dimension, bool is_stable,
                     std::string comparator_name)
    : Thunk(Kind::kSort, std::move(info)),
      inputs_(inputs.begin(), inputs.end()),
      dimension_(dimension),
      is_stable_(is_stable),
      comparator_name_(std::move(comparator_name)),
      less_than_ptr_(nullptr) {}

namespace {

// We use a lot of template metaprogramming below to be able to construct
// iterators with statically known number of compared elements. We support a
// limited set of template instantiations that we need in practice.

// The size of the largest element we support (std::complex<double>).
static constexpr size_t kMaxElementSize = 16;

// Forward declare reference type defined below.
template <size_t n>
struct Ref;

// Value type to store values loaded from the input buffers.
template <size_t n>
struct Value {
  Value(const Ref<n>& ref);  // NOLINT

  const void* compared_value(size_t i) const { return value[i].data(); }

  // Use properly aligned byte array to store primitive values.
  using ValueStorage = std::array<std::byte, kMaxElementSize>;
  alignas(alignof(std::max_align_t)) std::array<ValueStorage, n> value;
  std::array<uint8_t, n> value_sizes;
};

// Reference to values stored in the input buffers.
template <size_t n>
struct Ref {
  Ref(std::array<std::byte*, n> ptr, std::array<uint8_t, n> ptr_sizes)
      : ptr(ptr), ptr_sizes(ptr_sizes) {}

  Ref& operator=(const Value<n>& value);
  Ref& operator=(const Ref<n>& other);

  const void* compared_value(size_t i) const { return ptr[i]; }

  std::array<std::byte*, n> ptr;
  std::array<uint8_t, n> ptr_sizes;
};

template <size_t n>
Value<n>::Value(const Ref<n>& ref) : value_sizes(ref.ptr_sizes) {
  for (size_t i = 0; i < n; ++i) {
    std::memcpy(value[i].data(), ref.ptr[i], ref.ptr_sizes[i]);
  }
}

template <size_t n>
Ref<n>& Ref<n>::operator=(const Value<n>& value) {
  DCHECK(ptr_sizes == value.value_sizes);
  for (size_t i = 0; i < n; ++i) {
    std::memcpy(ptr[i], value.value[i].data(), value.value_sizes[i]);
  }
  return *this;
}

template <size_t n>
Ref<n>& Ref<n>::operator=(const Ref<n>& other) {
  DCHECK(ptr_sizes == other.ptr_sizes);
  for (size_t i = 0; i < n; ++i) {
    std::memcpy(ptr[i], other.ptr[i], other.ptr_sizes[i]);
  }
  return *this;
}

// Swap function required by `std::sort` and `std::stable_sort` implementations.
template <size_t n>
void swap(const Ref<n>& lhs, const Ref<n>& rhs) {
  for (size_t i = 0; i < n; ++i) {
    std::array<std::byte, kMaxElementSize> tmp;
    std::memcpy(tmp.data(), lhs.ptr[i], lhs.ptr_sizes[i]);
    std::memcpy(lhs.ptr[i], rhs.ptr[i], rhs.ptr_sizes[i]);
    std::memcpy(rhs.ptr[i], tmp.data(), lhs.ptr_sizes[i]);
  }
}

// An array of pointers to the input data.
template <size_t n>
struct Ptr {
  using difference_type = std::ptrdiff_t;

  Ptr() = default;

  Ptr(std::array<std::byte*, n> ptr, std::array<uint8_t, n> ptr_sizes)
      : ptr(ptr), ptr_sizes(ptr_sizes) {}

  Ref<n> operator*() const { return Ref<n>{ptr, ptr_sizes}; }

  Ptr& operator+=(difference_type diff) {
    for (size_t i = 0; i < n; ++i) ptr[i] += diff * ptr_sizes[i];
    return *this;
  }

  Ptr& operator-=(difference_type diff) {
    for (size_t i = 0; i < n; ++i) ptr[i] -= diff * ptr_sizes[i];
    return *this;
  }

  Ptr operator+(difference_type diff) const {
    std::array<std::byte*, n> upd;
    for (size_t i = 0; i < n; ++i) upd[i] = ptr[i] + diff * ptr_sizes[i];
    return Ptr{upd, ptr_sizes};
  }

  Ptr operator-(difference_type diff) const {
    std::array<std::byte*, n> upd;
    for (size_t i = 0; i < n; ++i) upd[i] = ptr[i] - diff * ptr_sizes[i];
    return Ptr{upd, ptr_sizes};
  }

  // In all comparison operators defined below we use only the ptr at index 0,
  // because we know that all pointers change together and this is an
  // implementation detail of sort iterator.

  difference_type operator-(const Ptr& rhs) const {
    DCHECK(ptr_sizes == rhs.ptr_sizes);
    return (ptr[0] - rhs.ptr[0]) / ptr_sizes[0];
  }

  bool operator==(const Ptr& rhs) const { return ptr[0] == rhs.ptr[0]; }
  bool operator!=(const Ptr& rhs) const { return ptr[0] != rhs.ptr[0]; }
  bool operator>(const Ptr& rhs) const { return ptr[0] > rhs.ptr[0]; }
  bool operator<(const Ptr& rhs) const { return ptr[0] < rhs.ptr[0]; }
  bool operator>=(const Ptr& rhs) const { return ptr[0] >= rhs.ptr[0]; }
  bool operator<=(const Ptr& rhs) const { return ptr[0] <= rhs.ptr[0]; }

  std::array<std::byte*, n> ptr;     // pointers into the input buffers
  std::array<uint8_t, n> ptr_sizes;  // pointers sizes in bytes
};

// We rely on `std::sort` and `std::stable_sort` to sort the raw data. We sort
// multiple input buffers together using the same comparator function, so we
// need to provide a custom iterator that can access the data of all input
// buffers at the same time and swap elements in them.
template <size_t n>
class SortIterator {
 public:
  using iterator_category = std::random_access_iterator_tag;
  using difference_type = std::ptrdiff_t;

  using value_type = Value<n>;
  using reference = Ref<n>;
  using pointer = Ptr<n>;

  SortIterator() = default;
  SortIterator(pointer ptr, difference_type stride)
      : ptr_(ptr), stride_(stride) {}

  SortIterator(const SortIterator& other) = default;
  SortIterator& operator=(const SortIterator& other) = default;
  SortIterator(SortIterator&& other) = default;
  SortIterator& operator=(SortIterator&& other) = default;

  reference operator*() const { return *ptr_; }

  difference_type operator-(const SortIterator& rhs) const {
    return (ptr_ - rhs.ptr_) / stride_;
  }

  SortIterator& operator+=(difference_type diff) {
    ptr_ += diff * stride_;
    return *this;
  }

  SortIterator& operator-=(difference_type diff) {
    ptr_ -= diff * stride_;
    return *this;
  }

  SortIterator& operator++() {
    ptr_ += stride_;
    return *this;
  }

  SortIterator& operator--() {
    ptr_ -= stride_;
    return *this;
  }

  SortIterator operator+(difference_type diff) const {
    return SortIterator(ptr_ + diff * stride_, stride_);
  }

  SortIterator operator-(difference_type diff) const {
    return SortIterator(ptr_ - diff * stride_, stride_);
  }

  bool operator==(const SortIterator& rhs) const { return ptr_ == rhs.ptr_; }
  bool operator!=(const SortIterator& rhs) const { return ptr_ != rhs.ptr_; }
  bool operator>(const SortIterator& rhs) const { return ptr_ > rhs.ptr_; }
  bool operator<(const SortIterator& rhs) const { return ptr_ < rhs.ptr_; }
  bool operator>=(const SortIterator& rhs) const { return ptr_ >= rhs.ptr_; }
  bool operator<=(const SortIterator& rhs) const { return ptr_ <= rhs.ptr_; }

 private:
  pointer ptr_;
  difference_type stride_ = 1;
};

struct SortDims {
  int64_t outer_dim_size;
  int64_t sort_dim_size;
  int64_t inner_dim_size;
  int64_t num_iterations;
};

}  // namespace

// Conceptually we have a 3-dimensional shape:
//
//   [outer_dim_size, sort_dim_size, inner_dim_size]
//
// We sort `outer_dim_size * inner_dim_size` vectors of length
// `sort_dim_size`, by iterating over `data` memory and calling `std::sort`
// (or `std::stable_sort`) on each (strided) slice of the buffer.
static SortDims GetSortDims(const Shape& shape, int64_t dimension) {
  int64_t sort_dimension =
      dimension >= 0 ? dimension : shape.rank() + dimension;

  // We need to normalize shape + layout into a descending layout, so that we
  // can compute access strides according to the physical layout.
  Shape physical_shape =
      ShapeUtil::MakeShapeWithDescendingLayoutAndSamePhysicalLayout(shape);

  // Map `sort_dimension` from logical to physical.
  auto logical_to_physical = LayoutUtil::MakeLogicalToPhysical(shape.layout());
  sort_dimension = logical_to_physical[sort_dimension];

  auto product = [](absl::Span<const int64_t> dims) {
    return absl::c_accumulate(dims, int64_t{1}, std::multiplies<>());
  };

  // Use physical dimensions to compute access strides.
  absl::Span<const int64_t> dimensions = physical_shape.dimensions();

  int64_t outer_dim_size = product(dimensions.subspan(0, sort_dimension));
  int64_t sort_dim_size = dimensions[sort_dimension];
  int64_t inner_dim_size = product(dimensions.subspan(sort_dimension + 1));
  int64_t num_iterations = outer_dim_size * inner_dim_size;

  return SortDims{outer_dim_size, sort_dim_size, inner_dim_size,
                  num_iterations};
}

// Sorts `n` buffers in place.
template <size_t n>
static void SortInplace(const SortDims& sort_dims, int64_t offset,
                        absl::Span<se::DeviceMemoryBase> data,
                        absl::Span<const Shape> shapes, bool is_stable,
                        SortThunk::LessThan* less_than) {
  std::array<std::byte*, n> ptr;
  std::array<uint8_t, n> ptr_sizes;

  for (size_t i = 0; i < n; ++i) {
    std::byte* base = reinterpret_cast<std::byte*>(data[i].opaque());
    ptr_sizes[i] = primitive_util::ByteWidth(shapes[i].element_type());
    ptr[i] = base + offset * ptr_sizes[i];
  }

  auto compare = [&](const auto& a, const auto& b) {
    std::array<const void*, 2 * n> data;
    for (size_t i = 0, j = 0; i < n; i += 1, j += 2) {
      data[j] = a.compared_value(i);
      data[j + 1] = b.compared_value(i);
    }
    return (*less_than)(data.data());
  };

  SortIterator<n> begin(Ptr<n>(ptr, ptr_sizes),
                        /*stride=*/sort_dims.inner_dim_size);
  if (is_stable) {
    std::stable_sort(begin, begin + sort_dims.sort_dim_size, compare);
  } else {
    std::sort(begin, begin + sort_dims.sort_dim_size, compare);
  }
}

// Sorts `data` of the given `shape` along the `dimension` inplace.
static absl::Status SortInplace(absl::Span<se::DeviceMemoryBase> data,
                                absl::Span<const Shape> shapes,
                                int64_t dimension, bool is_stable,
                                SortThunk::LessThan* less_than) {
  // All inputs have the same dimensions and layout, so we can use the first
  // shape to get the sort dimensions.
  SortDims sort_dims = GetSortDims(shapes[0], dimension);

  // Iterate over all the 1-dimensional slices of the buffers and sort them.
  for (int64_t i = 0; i < sort_dims.num_iterations; ++i) {
    int64_t inner_idx = i % sort_dims.inner_dim_size;
    int64_t offset = inner_idx + (i - inner_idx) * sort_dims.sort_dim_size;

    auto sort = [&](auto num_inputs) {
      SortInplace<decltype(num_inputs)::value>(sort_dims, offset, data, shapes,
                                               is_stable, less_than);
    };

    // TODO(ezhulenev): We can replace statically known number of sorted inputs
    // with a dynamic value, however statically known number of inputs allows
    // compiler to generate better code. Benchmark if it really matters.
    switch (data.size()) {
      case 1:
        sort(std::integral_constant<size_t, 1>{});
        break;
      case 2:
        sort(std::integral_constant<size_t, 2>{});
        break;
      case 3:
        sort(std::integral_constant<size_t, 3>{});
        break;
      case 4:
        sort(std::integral_constant<size_t, 4>{});
        break;
      case 5:
        sort(std::integral_constant<size_t, 5>{});
        break;
      case 6:
        sort(std::integral_constant<size_t, 6>{});
        break;
      case 7:
        sort(std::integral_constant<size_t, 7>{});
        break;
      case 8:
        sort(std::integral_constant<size_t, 8>{});
        break;
      case 9:
        sort(std::integral_constant<size_t, 9>{});
        break;
      case 10:
        sort(std::integral_constant<size_t, 10>{});
        break;
      case 11:
        sort(std::integral_constant<size_t, 11>{});
        break;
      case 12:
        sort(std::integral_constant<size_t, 12>{});
        break;
      case 13:
        sort(std::integral_constant<size_t, 13>{});
        break;
      case 14:
        sort(std::integral_constant<size_t, 14>{});
        break;
      case 15:
        sort(std::integral_constant<size_t, 15>{});
        break;
      case 16:
        sort(std::integral_constant<size_t, 16>{});
        break;
      case 25:
        sort(std::integral_constant<size_t, 25>{});
        break;
      case 29:
        sort(std::integral_constant<size_t, 29>{});
        break;
      default:
        return Internal("Unsupported number of sorted inputs: %d", data.size());
    }
  }

  return absl::OkStatus();
}

tsl::AsyncValueRef<SortThunk::ExecuteEvent> SortThunk::Execute(
    const ExecuteParams& params) {
  tsl::profiler::TraceMe trace([&] { return TraceMeEncode(); });

  VLOG(3) << absl::StreamFormat(
      "Sort %d inputs along dimension %d (is_stable=%v)", inputs_.size(),
      dimension_, is_stable_);

  absl::InlinedVector<se::DeviceMemoryBase, 8> data;
  data.reserve(inputs_.size());

  absl::InlinedVector<Shape, 8> shapes;
  shapes.reserve(inputs_.size());

  for (const Input& input : inputs_) {
    size_t idx = data.size();
    TF_ASSIGN_OR_RETURN(
        data.emplace_back(),
        params.buffer_allocations->GetDeviceAddress(input.slice));
    shapes.push_back(input.shape);

    // Annotate memory that might have been initialized by jit-compiled code.
    ABSL_ANNOTATE_MEMORY_IS_INITIALIZED(data.back().opaque(),
                                        data.back().size());

    VLOG(3) << absl::StreamFormat("  sort input #%d: %s in slice %s (%p)", idx,
                                  input.shape.ToString(/*print_layout=*/true),
                                  input.slice.ToString(), data.back().opaque());
  }

  LessThan* less_than = less_than_ptr_.load();

  // Because thunks are owned by a parent CpuExecutable, we can safely assume
  // that comparator pointer will not change after we find it the first time,
  // and we can create a comparator adaptor to a LessThan function.
  if (ABSL_PREDICT_FALSE(less_than == nullptr)) {
    TF_ASSIGN_OR_RETURN(
        FunctionRegistry::Comparator comparator,
        params.function_registry->FindComparator(comparator_name_));

    absl::MutexLock lock(&mutex_);
    less_than_ = [comparator](const void** data) {
      bool result;
      comparator(&result, nullptr, data, nullptr, nullptr, nullptr);
      ABSL_ANNOTATE_MEMORY_IS_INITIALIZED(&result, sizeof(result));
      return result;
    };
    less_than_ptr_.store(less_than = &*less_than_);
  }

  TF_RETURN_IF_ERROR(SortInplace(absl::MakeSpan(data), shapes, dimension_,
                                 is_stable_, less_than));

  return OkExecuteEvent();
}

SortThunk::BufferUses SortThunk::buffer_uses() const {
  BufferUses buffer_uses;
  buffer_uses.reserve(inputs_.size());
  for (const Input& input : inputs_) {
    buffer_uses.emplace_back(BufferUse::Write(input.slice));
  }
  return buffer_uses;
}

}  // namespace xla::cpu
