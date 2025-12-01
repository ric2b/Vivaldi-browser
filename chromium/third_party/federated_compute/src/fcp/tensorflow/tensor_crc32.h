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
#ifndef FCP_TENSORFLOW_TENSOR_CRC32_H_
#define FCP_TENSORFLOW_TENSOR_CRC32_H_

#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/lib/hash/crc32c.h"

namespace fcp {
namespace tensorflow {
namespace checksums {

/* Computes the CRC32c checksum of the in-memory representation of a Tensor. */
uint32_t TensorToCRC32(const ::tensorflow::Tensor& tensor);

}  // namespace checksums
}  // namespace tensorflow
}  // namespace fcp

#endif  // FCP_TENSORFLOW_TENSOR_CRC32_H_
