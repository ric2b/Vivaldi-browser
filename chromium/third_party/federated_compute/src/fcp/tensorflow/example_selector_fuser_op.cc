/*
 * Copyright 2021 Google LLC
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
#include <string>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/platform/tstring.h"

namespace fcp {

using ::google::internal::federated::plan::ExampleSelector;
using ::tensorflow::DEVICE_CPU;
using ::tensorflow::OpKernel;
using ::tensorflow::OpKernelConstruction;
using ::tensorflow::OpKernelContext;
using ::tensorflow::Tensor;
using ::tensorflow::data::ParseScalarArgument;

/**
 * ExampleSelectorFuserOp op-kernel.
 *
 * ExampleSelectorFuser fills the resumption token field for an existing
 * ExampleSelector protobuf message. The resumption token field is an Any proto
 * which can be any user defined protobuf message. The user needs to provide the
 * type url and content for the resumption token.
 *
 * Inputs:
 *   example_selector: A string scalar encodes an ExampleSelector protobuf
 *   message.
 *   resumption_token_type_url: String scalar. The type_url for the resumption
 *   token.
 *   resumption_token_content: String scalar.  The bytes for the resumption
 *   token message.
 *
 * Output:
 *   A string tensor contains the fused ExampleSelector message serialized to
 * string.
 */
class ExampleSelectorFuserOp : public OpKernel {
 public:
  explicit ExampleSelectorFuserOp(OpKernelConstruction* context)
      : OpKernel(context) {}

  void Compute(OpKernelContext* ctx) override {
    tensorflow::tstring example_selector_str;
    OP_REQUIRES_OK(ctx, ParseScalarArgument<tensorflow::tstring>(
                            ctx, "example_selector", &example_selector_str));
    tensorflow::tstring resumption_token_type_url_str;
    OP_REQUIRES_OK(ctx, ParseScalarArgument<tensorflow::tstring>(
                            ctx, "resumption_token_type_url",
                            &resumption_token_type_url_str));
    tensorflow::tstring resumption_token_content_str;
    OP_REQUIRES_OK(ctx, ParseScalarArgument<tensorflow::tstring>(
                            ctx, "resumption_token_content",
                            &resumption_token_content_str));
    ExampleSelector example_selector;
    if (!example_selector.ParseFromString(
            std::string(example_selector_str.data()))) {
      ctx->SetStatus(absl::Status(absl::StatusCode::kInvalidArgument,
                                  "Cannot parse ExampleSelector"));
      return;
    }
    example_selector.mutable_resumption_token()->set_type_url(
        std::string(resumption_token_type_url_str.data()));
    example_selector.mutable_resumption_token()->set_value(
        std::string(resumption_token_content_str.data()));

    Tensor* output_tensor = nullptr;
    OP_REQUIRES_OK(ctx, ctx->allocate_output(0, {}, &output_tensor));
    output_tensor->flat<tensorflow::tstring>()(0) =
        example_selector.SerializeAsString();
  }
};

REGISTER_OP("ExampleSelectorFuser")
    .Input("example_selector: string")
    .Input("resumption_token_type_url: string")
    .Input("resumption_token_content: string")
    .Output("fused_example_selector: string")
    .SetShapeFn(tensorflow::shape_inference::ScalarShape);
REGISTER_KERNEL_BUILDER(Name("ExampleSelectorFuser").Device(DEVICE_CPU),
                        ExampleSelectorFuserOp);
}  // namespace fcp
