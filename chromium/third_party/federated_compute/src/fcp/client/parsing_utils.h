/*
 * Copyright 2023 Google LLC
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

#ifndef FCP_CLIENT_PARSING_UTILS_H_
#define FCP_CLIENT_PARSING_UTILS_H_

#include <string>
#include <variant>

#include "absl/strings/cord.h"

namespace fcp {
namespace client {

// Parses a proto from either an std::string or an absl::Cord. This allows the
// proto data to be provided in either format.
template <typename MessageT>
bool ParseFromStringOrCord(MessageT& proto,
                           std::variant<std::string, absl::Cord> data) {
  if (std::holds_alternative<std::string>(data)) {
    return proto.ParseFromString(std::get<std::string>(data));
  } else {
    return proto.ParseFromString(std::string(std::get<absl::Cord>(data)));
  }
}

}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_PARSING_UTILS_H_
