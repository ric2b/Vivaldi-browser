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
#include "absl/status/status.h"
#include "fcp/tensorflow/tensor_crc32.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/hash/crc32c.h"

namespace fcp {
namespace tensorflow {
namespace checksums {

using ::tensorflow::DEVICE_CPU;
using ::tensorflow::OpKernel;
using ::tensorflow::OpKernelConstruction;
using ::tensorflow::OpKernelContext;
using ::tensorflow::StringPiece;
using ::tensorflow::Tensor;
using ::tensorflow::shape_inference::InferenceContext;

REGISTER_OP("CRC32")
    .Input("input: T")
    .Attr("T: type")
    .Output("checksum: uint32")
    .SetShapeFn([](InferenceContext* c) {
      c->set_output(0, c->Scalar());
      return absl::OkStatus();
    });

class CRC32Op : public OpKernel {
 public:
  explicit CRC32Op(OpKernelConstruction* context) : OpKernel(context) {}

  void Compute(OpKernelContext* context) override {
    // Create an output tensor
    Tensor* output_tensor = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(0, {}, &output_tensor));

    // Store CRC32 of input tensor in output.
    output_tensor->scalar<uint32_t>()() = TensorToCRC32(context->input(0));
  }
};

REGISTER_KERNEL_BUILDER(Name("CRC32").Device(DEVICE_CPU), CRC32Op);
}  // namespace checksums
}  // namespace tensorflow
}  // namespace fcp
