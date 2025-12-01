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

#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/strings/str_format.h"
#include "fcp/client/federated_select.h"
#include "fcp/protos/plan.pb.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/status.h"
#include "tensorflow/core/platform/stringpiece.h"

namespace fcp {

namespace {

REGISTER_OP("MakeSlicesSelectorExampleSelector")
    .Input("served_at_id: string")
    .Input("keys: int32")
    .Output("serialized_proto: string")
    .SetShapeFn(tensorflow::shape_inference::ScalarShape);

class MakeSlicesSelectorExampleSelectorOp : public tensorflow::OpKernel {
 public:
  explicit MakeSlicesSelectorExampleSelectorOp(
      tensorflow::OpKernelConstruction* context)
      : OpKernel(context) {}
  void Compute(tensorflow::OpKernelContext* context) override {
    const tensorflow::Tensor* served_at_id_tensor;
    OP_REQUIRES_OK(context,
                   context->input("served_at_id", &served_at_id_tensor));
    std::string served_at_id =
        served_at_id_tensor->scalar<tensorflow::tstring>()();

    const tensorflow::Tensor* keys_tensor;
    OP_REQUIRES_OK(context, context->input("keys", &keys_tensor));
    tensorflow::TTypes<int32_t>::ConstFlat keys = keys_tensor->flat<int32_t>();

    google::internal::federated::plan::SlicesSelector slices_selector;
    slices_selector.set_served_at_id(std::move(served_at_id));
    slices_selector.mutable_keys()->Reserve(keys.size());
    for (size_t i = 0; i < keys.size(); i++) {
      slices_selector.add_keys(keys(i));
    }

    google::internal::federated::plan::ExampleSelector example_selector;
    example_selector.mutable_criteria()->PackFrom(slices_selector);
    example_selector.set_collection_uri(
        fcp::client::kFederatedSelectCollectionUri);
    // `resumption_token` not set.

    tensorflow::Tensor* output_tensor;
    OP_REQUIRES_OK(context, context->allocate_output(0, {}, &output_tensor));
    output_tensor->scalar<tensorflow::tstring>()() =
        example_selector.SerializeAsString();
  }
};

REGISTER_KERNEL_BUILDER(
    Name("MakeSlicesSelectorExampleSelector").Device(tensorflow::DEVICE_CPU),
    MakeSlicesSelectorExampleSelectorOp);

}  // namespace

}  // namespace fcp
