/*
 * Copyright 2019 Google LLC
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

#ifndef FCP_TENSORFLOW_HOST_OBJECT_H_
#define FCP_TENSORFLOW_HOST_OBJECT_H_

#include <memory>
#include <optional>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/random_token.h"
#include "fcp/base/unique_value.h"

namespace fcp {

/**
 * Op-kernels are instantiated by TensorFlow, and can only be parameterized by
 * graph 'attrs' and tensor inputs. So, op-kernels which access the 'outside
 * world' tend to use ambient, process-global resources - for example, consider
 * op-kernels which interpret a string tensor as a filesystem path.
 *
 * In some uses, we'd like to parameterize an op-kernel on some 'host'-side,
 * non-Tensor objects (for example, a virtual filesystem) at the site of
 * Session::Run (i.e. maintaining functional composition).
 *
 * This file defines a mechanism to register 'host objects' (in a
 * HostObjectRegistry) outside of a session, pass them to Session::Run, and
 * refer to them inside of the graph (and op-kernel implementations) using
 * DT_STRING scalars ('tokens'). We could instead use DT_VARIANT tensors (which
 * can wrap C++ objects directly), but DT_STRING is much more convenient to
 * marshal (for example, Python's Session::Run wrapper accepts Python strings
 * for placeholder bindings, but not existing Tensor objects).
 *
 * To register a host object:
 *   Use HostObjectRegistry<I> for some interface type 'I'. This returns a
 *   HostObjectRegistration object, which de-registers on destruction.
 * To pass in a host object:
 *   Bind the token() (from the HostObjectRegistration) to some placeholder,
 *   when calling Session::Run.
 * To access a host object in an op-kernel:
 *   Use HostObjectRegistry<I>::TryLookup (the op should take a DT_STRING scalar
 *   for the token to use).
 */

namespace host_object_internal {

/**
 * HostObjectRegistry implementation for a particular interface type.
 *
 * For each I, HostObjectRegistry<I> defines a HostObjectRegistryImpl with
 * static storage duration.
 */
class HostObjectRegistryImpl {
 public:
  std::optional<std::shared_ptr<void>> TryLookup(RandomToken token);
  void Register(RandomToken token, std::shared_ptr<void> p);
  void Unregister(RandomToken token);
 private:
  absl::Mutex mutex_;
  absl::flat_hash_map<RandomToken, std::shared_ptr<void>> objects_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace host_object_internal

/**
 * Active registration of a host object, under token(). To reference this object
 * in a TensorFlow graph, pass in token() as a DT_STRING tensor.
 *
 * De-registers when destructed. Note that the registered object *may* stay
 * alive; an op-kernel can retain an std::shared_ptr ref from TryLookup.
 */
class HostObjectRegistration final {
 public:
  HostObjectRegistration(HostObjectRegistration&&) = default;
  HostObjectRegistration& operator=(HostObjectRegistration&&) = default;

  ~HostObjectRegistration() {
    if (token_.has_value()) {
      registry_->Unregister(*token_);
    }
  }

  /**
   * Token under which the object is registered. It can be passed into a graph
   * (as a string tensor) and used to look up the object.
   */
  RandomToken token() const { return *token_; }

 private:
  template<typename T>
  friend class HostObjectRegistry;

  HostObjectRegistration(host_object_internal::HostObjectRegistryImpl* registry,
                         RandomToken token)
      : registry_(registry), token_(token) {}

  host_object_internal::HostObjectRegistryImpl* registry_;
  UniqueValue<RandomToken> token_;
};

/**
 * Registry of host objects, for a particular interface type.
 * See file remarks.
 */
template<typename T>
class HostObjectRegistry {
 public:
  /**
   * Registers the provided host object, yielding a new HostObjectRegistration
   * with a unique token(). The object is de-registered when the
   * HostObjectRegistration is destructed.
   */
  static HostObjectRegistration Register(std::shared_ptr<T> p) {
    RandomToken token = RandomToken::Generate();
    GetImpl()->Register(token, std::move(p));
    return HostObjectRegistration(GetImpl(), token);
  }

  /**
   * Looks up a host object. Returns std::nullopt if nothing is currently
   * registered for the provided token (and interface T).
   */
  static std::optional<std::shared_ptr<T>> TryLookup(RandomToken token) {
    std::optional<std::shared_ptr<void>> maybe_p = GetImpl()->TryLookup(token);
    if (maybe_p.has_value()) {
      std::shared_ptr<void> p = *std::move(maybe_p);
      return std::static_pointer_cast<T>(std::move(p));
    } else {
      return std::nullopt;
    }
  }

 private:
  HostObjectRegistry();

  static host_object_internal::HostObjectRegistryImpl* GetImpl() {
    static auto* global_registry =
        new host_object_internal::HostObjectRegistryImpl();
    return global_registry;
  }
};

}  // namespace fcp

#endif  // FCP_TENSORFLOW_HOST_OBJECT_H_
