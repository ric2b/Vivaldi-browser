/*
 * Copyright 2024 Google LLC
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

// Class that serves as a registry of functions.
//
// A FunctionRegistry maps keys to functions. The key and function types are
// specified in the template arguments.
//
// Here's a simple usage example:
//
//   // Maps string keys to functions that take two ints and return an int.
//   FunctionRegistry<string, int(int, int)> r;
//
//   // Registers the "add" function
//   r.Register("add", [](int a, int b) { return a + b; });
//
//   // Tries to get "add" from the registry and runs it on success.
//   std::function<int(int, int)> f = r.Get("add");
//   if (f) {
//     int answer = f(4, 2);
//     // answer == 6
//   }
//
// FunctionRegistry allows clients to store objects into some central data
// structure used in a library they don't own. You can think of the registered
// functions as factories for instantiating "plugins". In such a case, the
// registry owner will expose a global registry, and the client will register an
// implementation with it in the dynamic initialization phase of the program. In
// this way, the client does not need to modify the registry owner's code.
//
// All operations on the FunctionRegistry are thread safe.
#ifndef FCP_BASE_FUNCTION_REGISTRY_H_
#define FCP_BASE_FUNCTION_REGISTRY_H_

#include <algorithm>
#include <functional>
#include <utility>

#include "absl/base/log_severity.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "fcp/base/monitoring.h"

namespace fcp {

// A registry that maps keys of type K to functions of type Fn.
//
// Example:
//
//   FunctionRegistry<string, int(int, int)> my_registry;
//
template <typename K, typename Fn>
class FunctionRegistry {
 public:
  using Key = K;
  using Function = std::function<Fn>;

  FunctionRegistry() = default;
  FunctionRegistry(const FunctionRegistry&) = delete;
  FunctionRegistry& operator=(const FunctionRegistry&) = delete;

  // Adds the given key and function to the registry. Returns true if we
  // successfully registered a unique key.
  //
  // See docs above for more details.
  template <typename KeyArg = Key>
  bool Register(const KeyArg& key, Function fn) {
    absl::MutexLock lock(&mu_);
    auto insert_result = functions_.emplace(key, std::move(fn));

    if (!insert_result.second) {
      FCP_LOG(ERROR) << "Registration failed; key already exists in registry";
    }

    return insert_result.second;
  }

  // Gets the function associated with key from the registry. If no such key
  // exists, returns a default-constructed (empty) function.
  //
  // See docs above for more details.
  template <typename KeyArg = Key>
  Function Get(const KeyArg& key) const {
    absl::ReaderMutexLock lock(&mu_);
    Function fn;
    auto it = functions_.find(key);
    if (it != functions_.end()) {
      fn = it->second;
    }
    return fn;
  }

 private:
  mutable absl::Mutex mu_;
  absl::flat_hash_map<Key, Function> functions_ ABSL_GUARDED_BY(mu_);
};

// Adds the given key and function to the registry. FCP_CHECK-fails if
// registration fails. The return value is only provided for the convenience of
// initializing a static variable, and is otherwise meaningless.
template <typename Registry, typename Key = typename Registry::Key>
bool RegisterOrDie(Registry& registry, const Key& key,
                   typename Registry::Function fn) {
  FCP_CHECK(registry.Register(key, std::move(fn)))
      << "Registration failed, see error log";
  return true;
}

}  // namespace fcp

#endif  // FCP_BASE_FUNCTION_REGISTRY_H_
