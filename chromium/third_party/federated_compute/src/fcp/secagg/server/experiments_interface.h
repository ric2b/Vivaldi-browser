/*
 * Copyright 2020 Google LLC
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

#ifndef FCP_SECAGG_SERVER_EXPERIMENTS_INTERFACE_H_
#define FCP_SECAGG_SERVER_EXPERIMENTS_INTERFACE_H_

#include "absl/strings/string_view.h"

namespace fcp {
namespace secagg {

// Used to query named flags for experimental features.
class ExperimentsInterface {
 public:
  // Returns true if the specified experiment is enabled;
  // otherwise returns false.
  virtual bool IsEnabled(absl::string_view experiment_name) = 0;

  virtual ~ExperimentsInterface() = default;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SERVER_EXPERIMENTS_INTERFACE_H_
