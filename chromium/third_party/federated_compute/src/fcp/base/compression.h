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

#ifndef FCP_BASE_COMPRESSION_H_
#define FCP_BASE_COMPRESSION_H_

#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace fcp {

absl::StatusOr<std::string> CompressWithGzip(
    absl::string_view uncompressed_data);
absl::StatusOr<absl::Cord> UncompressWithGzip(
    absl::string_view compressed_data);

}  // namespace fcp

#endif  // FCP_BASE_COMPRESSION_H_
