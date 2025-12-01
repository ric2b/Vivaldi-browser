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

#include "fcp/protos/federated_api.pb.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/dataset.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/shape_inference.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/public/version.h"

namespace fcp {

using ::google::internal::federatedml::v2::TaskEligibilityInfo;
using ::google::internal::federatedml::v2::TaskWeight;

/**
 * CreateTaskEligibilityInfo op-kernel. Converts a set of input tensors into a
 * `TaskEligibilityInfo` proto serialized into a string tensor.
 *
 * This op is used to generate `TaskEligibilityInfo` protos from a model at
 * runtime, since TF Mobile does not support the standard TensorFlow ops for
 * encoding/decoding protos.
 */
class CreateTaskEligibilityInfoOp : public tensorflow::OpKernel {
 public:
  explicit CreateTaskEligibilityInfoOp(
      tensorflow::OpKernelConstruction* context)
      : OpKernel(context) {}

  void Compute(tensorflow::OpKernelContext* ctx) override {
    // Note: We use the tensorflow::data::ParseScalar/VectorArgument helpers
    // here, even though this op isn't strictly related to our tf.Dataset
    // integration. The helpers are public though, and we already use them in
    // our ExternalDataset implementation, so we might as well use them here
    // too.

    // Parse/validate the input arguments.
    tensorflow::int64 version;
    OP_REQUIRES_OK(
        ctx, tensorflow::data::ParseScalarArgument(ctx, "version", &version));
    std::vector<tensorflow::tstring> task_names;
    OP_REQUIRES_OK(ctx, tensorflow::data::ParseVectorArgument(ctx, "task_names",
                                                              &task_names));
    std::vector<float> task_weights;
    OP_REQUIRES_OK(ctx, tensorflow::data::ParseVectorArgument(
                            ctx, "task_weights", &task_weights));
    OP_REQUIRES(ctx, task_names.size() == task_weights.size(),
                tensorflow::errors::InvalidArgument(absl::StrCat(
                    "task_names length must match task_weights length: ",
                    task_names.size(), " vs. ", task_weights.size())));

    // Create the output proto, based on the inputs.
    TaskEligibilityInfo eligibility_info;
    eligibility_info.set_version(version);
    // Create a `TaskWeight` message for each pair of `task_names` and
    // `task_weights` elements.
    auto task_weight_it = task_weights.cbegin();
    for (const tensorflow::tstring& task_name : task_names) {
      float task_weight = *task_weight_it++;
      TaskWeight* task_weight_proto = eligibility_info.add_task_weights();
      task_weight_proto->set_task_name(std::string(task_name));
      task_weight_proto->set_weight(task_weight);
    }

    // Place the serialized output proto into the output tensor.
    tensorflow::Tensor* output_tensor;
    OP_REQUIRES_OK(ctx,
                   ctx->allocate_output("output", tensorflow::TensorShape({}),
                                        &output_tensor));
    output_tensor->scalar<tensorflow::tstring>()() =
        eligibility_info.SerializeAsString();
  }
};

REGISTER_OP("CreateTaskEligibilityInfo")
    .Input("version: int64")
    .Input("task_names: string")
    .Input("task_weights: float32")
    .Output("output: string")
    .SetShapeFn(tensorflow::shape_inference::ScalarShape);

REGISTER_KERNEL_BUILDER(
    Name("CreateTaskEligibilityInfo").Device(tensorflow::DEVICE_CPU),
    CreateTaskEligibilityInfoOp);

}  // namespace fcp
