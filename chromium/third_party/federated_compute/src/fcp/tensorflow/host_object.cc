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

#include "fcp/tensorflow/host_object.h"

#include <utility>

#include "fcp/base/monitoring.h"

namespace fcp {

namespace host_object_internal {

std::optional<std::shared_ptr<void>> HostObjectRegistryImpl::TryLookup(
    RandomToken token) {
  std::shared_ptr<void> p = nullptr;

  {
    absl::ReaderMutexLock lock{&mutex_};
    auto it = objects_.find(token);
    if (it != objects_.end()) {
      p = it->second;
    }
  }

  if (p == nullptr) {
    return std::nullopt;
  } else {
    return p;
  }
}

void HostObjectRegistryImpl::Register(RandomToken token,
                                      std::shared_ptr<void> p) {
  absl::WriterMutexLock lock{&mutex_};
  auto r = objects_.insert({token, std::move(p)});
  FCP_CHECK(r.second)
      << "An object has already been registered with the provided token";
}

void HostObjectRegistryImpl::Unregister(RandomToken token) {
  absl::WriterMutexLock lock{&mutex_};
  size_t erased = objects_.erase(token);
  FCP_CHECK(erased == 1)
      << "An object is not currently registered for the provided token";
}

}  // namespace host_object_internal

}  // namespace fcp
