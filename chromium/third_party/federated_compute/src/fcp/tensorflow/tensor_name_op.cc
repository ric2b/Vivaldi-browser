/*
 * Copyright 2022 Google LLC
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

#include <utility>

#include "absl/strings/str_format.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/stringpiece.h"

namespace fcp {

namespace {

REGISTER_OP("TensorName")
    .Attr("InputType: type")
    .Input("input_tensor: InputType")
    .Output("tensor_name: string")
    .SetShapeFn(tensorflow::shape_inference::ScalarShape);

class TensorNameOp : public tensorflow::OpKernel {
 public:
  explicit TensorNameOp(tensorflow::OpKernelConstruction* context)
      : OpKernel(context) {
    const tensorflow::NodeDef& def = context->def();
    // Note: more than one input is allowed since the "true" input node may be
    // followed by any number of control inputs.
    OP_REQUIRES(
        context, def.input_size() >= 1,
        tensorflow::errors::InvalidArgument("Expected an input, found none."));
    input_name_ = def.input(0);
  }

  void Compute(tensorflow::OpKernelContext* context) override {
    tensorflow::Tensor* output_tensor;
    OP_REQUIRES_OK(context, context->allocate_output(0, {}, &output_tensor));
    output_tensor->scalar<tensorflow::tstring>()() = input_name_;
  }

 private:
  tensorflow::tstring input_name_;
};

REGISTER_KERNEL_BUILDER(Name("TensorName").Device(tensorflow::DEVICE_CPU),
                        TensorNameOp);

}  // namespace

}  // namespace fcp
