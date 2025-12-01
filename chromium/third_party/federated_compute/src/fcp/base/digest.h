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

#ifndef FCP_BASE_DIGEST_H_
#define FCP_BASE_DIGEST_H_

#include <string>

#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace fcp {
// Returns the SHA256 hash for the given data. Note that the return value
// contains raw digest bytes, and not a human-readable hex-encoded string.
std::string ComputeSHA256(const absl::Cord& data);
// Returns the SHA256 hash for the given data. Note that the return value
// contains raw digest bytes, and not a human-readable hex-encoded string.
std::string ComputeSHA256(absl::string_view data);
}  // namespace fcp

#endif  // FCP_BASE_DIGEST_H_
