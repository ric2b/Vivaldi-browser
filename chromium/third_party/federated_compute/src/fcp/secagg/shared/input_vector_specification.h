/*
 * Copyright 2018 Google LLC
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

#ifndef FCP_SECAGG_SHARED_INPUT_VECTOR_SPECIFICATION_H_
#define FCP_SECAGG_SHARED_INPUT_VECTOR_SPECIFICATION_H_

#include <cstdint>
#include <string>

#include "absl/base/attributes.h"

namespace fcp {
namespace secagg {

// Used to specify the name and either:
//
// 1. For the original protocol, the length and bit width of each input vector
//    which the protocol will aggregate.
// 2. For the RLWE version, the length, the polynomial degree, and the modulus
//    for each input vector.  In this case the length must be a multiple of the
//    degree.
class InputVectorSpecification {
 public:
  InputVectorSpecification(const std::string& name, int length,
                           uint64_t modulus);

  virtual ~InputVectorSpecification() = default;

  ABSL_MUST_USE_RESULT inline const std::string& name() const { return name_; }

  ABSL_MUST_USE_RESULT inline int length() const { return length_; }

  ABSL_MUST_USE_RESULT inline uint64_t modulus() const { return modulus_; }

 private:
  const std::string name_;
  const int length_;
  const uint64_t modulus_;
};

}  // namespace secagg
}  // namespace fcp

#endif  // FCP_SECAGG_SHARED_INPUT_VECTOR_SPECIFICATION_H_
