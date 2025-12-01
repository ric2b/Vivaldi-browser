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
#include "fcp/tensorflow/tensor_crc32.h"

namespace fcp {
namespace tensorflow {
namespace checksums {

using ::tensorflow::StringPiece;
using ::tensorflow::Tensor;
using ::tensorflow::crc32c::Value;

uint32_t TensorToCRC32(const Tensor& tensor) {
  StringPiece tensor_data = tensor.tensor_data();
  return Value(tensor_data.data(), tensor_data.size());
}

}  // namespace checksums
}  // namespace tensorflow
}  // namespace fcp
