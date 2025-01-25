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

#ifndef XLA_PYTHON_IFRT_DEVICE_LIST_H_
#define XLA_PYTHON_IFRT_DEVICE_LIST_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ExtensibleRTTI.h"
#include "xla/python/ifrt/device.h"
#include "xla/python/ifrt/device.pb.h"
#include "xla/tsl/concurrency/ref_count.h"

namespace xla {
namespace ifrt {

// Ordered immutable list of devices.
class DeviceList : public tsl::ReferenceCounted<DeviceList>,
                   public llvm::RTTIExtends<DeviceList, llvm::RTTIRoot> {
 public:
  // Function that matches the semantics of `Client::LookupDevice()`.
  // TODO(hyeontaek): Remove this type. In the future, a deserialization option
  // will take `Client*` to allow constructing a complex `DeviceList` that is
  // not just `BasicDeviceList`.
  using LookupDeviceFunc = absl::FunctionRef<absl::StatusOr<Device*>(DeviceId)>;

  // Not copyable or movable. `DeviceList` is a runtime object that may contain
  // runtime-specific state that cannot be trivially copied or moved.
  DeviceList(const DeviceList&) = delete;
  DeviceList(DeviceList&&) = delete;
  DeviceList& operator=(const DeviceList&) = delete;
  DeviceList& operator=(DeviceList&&) = delete;

  // Constructs `DeviceList` from `DeviceListProto`. Devices are looked up using
  // `lookup_device`. Device ids in the proto must be consistent with the
  // devices returned by `lookup_device`.
  static absl::StatusOr<tsl::RCReference<DeviceList>> FromProto(
      LookupDeviceFunc lookup_device, const DeviceListProto& proto);

  // Returns a `DeviceListProto` representation.
  DeviceListProto ToProto() const;

  // Returns the number of devices.
  // TODO(hyeontaek): Make this a virtual method and make it possible for a
  // subclass to lazily materialize devices for `devices()`.
  int size() const { return devices().size(); }

  // Returns a list of `Devices*` represented by this `DeviceList`.
  virtual absl::Span<Device* const> devices() const = 0;

  virtual bool operator==(const DeviceList& other) const = 0;
  bool operator!=(const DeviceList& other) const { return !(*this == other); }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const DeviceList& device_list) {
    sink.Append(device_list.ToString());
  }

  // Returns the hash of devices. This hash is stable only within the process.
  virtual uint64_t hash() const = 0;

  // TODO(hyeontaek): Remove this method in favor of AbslStringify.
  std::string DebugString() const { return ToString(); }

  static char ID;  // NOLINT

 protected:
  DeviceList() = default;

  virtual std::string ToString() const = 0;
};

// Simple implementation of `DeviceList` that contains a list of devices without
// creating any runtime object in the IFRT implementation.
//
// This is a transitory type that will be replaced with (1) a non-IFRT container
// defined by the user code (e.g., `std::vector<Device*>`) or (2) IFRT
// implementation's own `DeviceList` constructed from its `xla::ifrt::Client`
// API implementation.
class BasicDeviceList : public llvm::RTTIExtends<BasicDeviceList, DeviceList> {
 public:
  // Number of devices to inline in `Devices`.
  static constexpr int kInlineDeviceSize = 1;

  // TODO(hyeontaek): Consider using variant<Device*, std::vector<Device*>> for
  // better performance.
  using Devices = absl::InlinedVector<Device*, kInlineDeviceSize>;

  // Constructor with a pre-populated `devices`.
  static tsl::RCReference<DeviceList> Create(Devices devices);

  ~BasicDeviceList() override = default;

  // Constructs `DeviceList` from `DeviceListProto`. Devices are looked up
  // using `lookup_device`. Device ids in the proto must be consistent with
  // the devices returned by `lookup_device`.
  static absl::StatusOr<tsl::RCReference<DeviceList>> FromProto(
      LookupDeviceFunc lookup_device, const DeviceListProto& proto);

  // Returns a `DeviceListProto` representation.
  DeviceListProto ToProto() const;

  absl::Span<Device* const> devices() const override { return state().devices; }

  bool operator==(const DeviceList& other) const override {
    const auto* other_basic_device_list =
        llvm::dyn_cast<BasicDeviceList>(&other);
    if (other_basic_device_list == nullptr) {
      return false;
    }
    const std::shared_ptr<State>* lhs =
        std::get_if<std::shared_ptr<State>>(&state_);
    const std::shared_ptr<State>* rhs =
        std::get_if<std::shared_ptr<State>>(&other_basic_device_list->state_);
    if (lhs != nullptr && rhs != nullptr && lhs->get() == rhs->get()) {
      return true;
    }
    return devices() == other.devices();
  }

  uint64_t hash() const override;

  static char ID;  // NOLINT

 private:
  explicit BasicDeviceList(Devices devices);

  template <typename T, typename... Args>
  friend tsl::RCReference<T> tsl::MakeRef(Args&&... args);

  // Internal state that may be shared across `DeviceList` instances.
  struct State {
    Devices devices;
  };

  State& state() {
    return std::visit(
        [](auto& state) -> State& {
          using T = std::decay_t<decltype(state)>;
          if constexpr (std::is_same_v<T, State>) {
            return state;
          } else if constexpr (std::is_same_v<T, std::shared_ptr<State>>) {
            return *state;
          }
        },
        state_);
  }

  const State& state() const {
    return std::visit(
        [](auto& state) -> const State& {
          using T = std::decay_t<decltype(state)>;
          if constexpr (std::is_same_v<T, State>) {
            return state;
          } else if constexpr (std::is_same_v<T, std::shared_ptr<State>>) {
            return *state;
          }
        },
        state_);
  }

  std::string ToString() const override;

  std::variant<State, std::shared_ptr<State>> state_;

  // Cached hash. 0 indicates the hash needs to be computed and cached.
  // May be written multiple times with the same non-zero value.
  static constexpr uint64_t kUnsetHash = 0;
  mutable std::atomic<uint64_t> hash_;
};

// Returns the id of each device in `device_list`.
std::vector<DeviceId> GetDeviceIds(
    const tsl::RCReference<DeviceList>& device_list);

// Hash function for `DeviceList`. Assumes that every unique device has a unique
// `Device` object, not duplicate `Device` objects ("d1 == d2 if d1->id() ==
// d2->id()").
template <typename H>
H AbslHashValue(H h, const tsl::RCReference<DeviceList>& devices) {
  return H::combine(std::move(h), devices->hash());
}

// Prevent comparing two tsl::RCReference<DeviceList>. If attempted, the
// compilation will fail with an ambiguous use of these operators.
inline bool operator==(const tsl::RCReference<DeviceList>& lhs,
                       const tsl::RCReference<DeviceList>& rhs) {
  CHECK(false) << "Comparing two tsl::RCReference<DeviceList> directly is "
                  "typically unintended. Do a comparison after deferenceing "
                  "them, or compare their raw pointers.";
}
inline bool operator!=(const tsl::RCReference<DeviceList>& lhs,
                       const tsl::RCReference<DeviceList>& rhs) {
  CHECK(false) << "Comparing two tsl::RCReference<DeviceList> directly is "
                  "typically unintended. Do a comparison after deferenceing "
                  "them, or compare their raw pointers.";
}

}  // namespace ifrt
}  // namespace xla

#endif  // XLA_PYTHON_IFRT_DEVICE_LIST_H_
