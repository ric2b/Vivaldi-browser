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

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "fcp/base/random_token.h"
#include "fcp/tensorflow/serve_slices_registry.h"
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/stringpiece.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/tsl/platform/errors.h"

namespace fcp {

namespace {

REGISTER_OP("ServeSlices")
    .Attr("NumTensorsInServerVal: int")
    .Attr("ServerValType: list(type)")
    .Input("callback_token: string")
    .Input("server_val: ServerValType")
    .Input("max_key: int32")
    .Input("select_fn_initialize_op: string")
    .Input(
        "select_fn_server_val_input_tensor_names: NumTensorsInServerVal * "
        "string")
    .Input("select_fn_key_input_tensor_name: string")
    .Input("select_fn_filename_input_tensor_name: string")
    .Input("select_fn_target_tensor_name: string")
    .Output("served_at_id: string")
    .SetIsStateful()
    .SetShapeFn(tensorflow::shape_inference::ScalarShape);

template <class T>
absl::Status get_scalar_input(tensorflow::OpKernelContext* context,
                              absl::string_view name, T* scalar_out) {
  const tensorflow::Tensor* tensor;
  TF_RETURN_IF_ERROR(context->input(name, &tensor));
  *scalar_out = tensor->scalar<T>()();
  return absl::OkStatus();
}

absl::Status get_arbitrary_input_list_as_tensor_vector(
    tensorflow::OpKernelContext* context, absl::string_view name,
    std::vector<tensorflow::Tensor>* out) {
  tensorflow::OpInputList input_list;
  TF_RETURN_IF_ERROR(context->input_list(name, &input_list));
  out->reserve(input_list.size());
  for (const tensorflow::Tensor& tensor : input_list) {
    out->push_back(tensor);
  }
  return absl::OkStatus();
}

absl::Status get_string_list_input(tensorflow::OpKernelContext* context,
                                   absl::string_view name,
                                   std::vector<std::string>* out) {
  tensorflow::OpInputList input_list;
  TF_RETURN_IF_ERROR(context->input_list(name, &input_list));
  out->reserve(input_list.size());
  for (const tensorflow::Tensor& tensor : input_list) {
    out->emplace_back(tensor.scalar<tensorflow::tstring>()());
  }
  return absl::OkStatus();
}

// ServeSlices op-kernel.
//
// The ServeSlicesOp registers values present on a federated computation server
// to be sliced and served to clients for a `federated_select`
//
// Inputs:
//   callback_token: The ID of the C++ callback to invoke in order to register
//   the
//     given value. Callbacks must first be registered using
//     `register_serve_slices_callback`.
//   server_val: A series of arbitrary-typed tensors from which slices may be
//     generated using a selection function (referred to as `select_fn`).
//     These tensors must be passed into the `select_fn` by writing them to the
//     placeholder tensors named by `select_fn_server_val_input_names`, which
//     must contain exactly one tensor name for each tensor in `server_val`.
//   max_key: An integer indicating the maximum slice index which may be
//     requested. Slice indices start at zero and may go up to `max_key`
//     (inclusive).
//   select_fn_initialize_op: An op to run before each call to `select_fn` in
//     order to reinitialize any state `select_fn` may contain.
//   select_fn_server_val_input_tensor_names: A list of names of the tensors
//     that make up the `server_val` portion of the inputs to `select_fn`. Must
//     be the same length as the number of tensors in `server_val`.
//   select_fn_key_input_tensor_name: The name of the tensor that is the `key`
//     input to `select_fn`.
//   select_fn_filename_input_tensor_name: The name of the placeholder tensor
//     that is the `filename` input to `select_fn`. The `filename` is used to
//     specify where the resulting slice should be written.
//   select_fn_target_tensor_name: The name of the `target` tensor to run which
//     will result in `select_fn`'s output being written to `filename`.
//
// Outputs:
//   served_at_id: A string ID under which the resulting slices will be served.
//     This can then be provided to the `FetchSlicesOp` running on clients.
class ServeSlicesOp : public tensorflow::OpKernel {
 public:
  explicit ServeSlicesOp(tensorflow::OpKernelConstruction* context)
      : OpKernel(context) {}

  void Compute(tensorflow::OpKernelContext* context) override {
    tensorflow::tstring callback_token_tensor;
    OP_REQUIRES_OK(context, get_scalar_input(context, "callback_token",
                                             &callback_token_tensor));
    absl::Span<char const> callback_token_bytes = callback_token_tensor;
    OP_REQUIRES(context, callback_token_bytes.size() == kRandomTokenSizeInBytes,
                tensorflow::errors::InvalidArgument(absl::StrFormat(
                    "Tokens have a fixed size. Expected: %d; Actual %d",
                    kRandomTokenSizeInBytes, callback_token_bytes.size())));
    RandomToken callback_token = RandomToken::FromBytes(callback_token_bytes);

    std::vector<tensorflow::Tensor> server_val;
    OP_REQUIRES_OK(context, get_arbitrary_input_list_as_tensor_vector(
                                context, "server_val", &server_val));

    int32_t max_key;
    OP_REQUIRES_OK(context, get_scalar_input(context, "max_key", &max_key));

    tensorflow::tstring select_fn_initialize_op;
    OP_REQUIRES_OK(context, get_scalar_input(context, "select_fn_initialize_op",
                                             &select_fn_initialize_op));

    std::vector<std::string> select_fn_server_val_input_tensor_names;
    OP_REQUIRES_OK(context,
                   get_string_list_input(
                       context, "select_fn_server_val_input_tensor_names",
                       &select_fn_server_val_input_tensor_names));

    tensorflow::tstring select_fn_key_input_tensor_name;
    OP_REQUIRES_OK(context,
                   get_scalar_input(context, "select_fn_key_input_tensor_name",
                                    &select_fn_key_input_tensor_name));

    tensorflow::tstring select_fn_filename_input_tensor_name;
    OP_REQUIRES_OK(context, get_scalar_input(
                                context, "select_fn_filename_input_tensor_name",
                                &select_fn_filename_input_tensor_name));

    tensorflow::tstring select_fn_target_tensor_name;
    OP_REQUIRES_OK(context,
                   get_scalar_input(context, "select_fn_target_tensor_name",
                                    &select_fn_target_tensor_name));

    std::optional<std::shared_ptr<ServeSlicesCallback>> callback =
        get_serve_slices_callback(callback_token);
    OP_REQUIRES(context, callback.has_value(),
                tensorflow::errors::InvalidArgument(
                    absl::StrCat("No `ServeSlices` callback found for token ",
                                 callback_token.ToPrintableString())));
    std::string served_at_id =
        (**callback)(callback_token, std::move(server_val), max_key,
                     std::move(select_fn_initialize_op),
                     std::move(select_fn_server_val_input_tensor_names),
                     std::move(select_fn_key_input_tensor_name),
                     std::move(select_fn_filename_input_tensor_name),
                     std::move(select_fn_target_tensor_name));

    tensorflow::Tensor* output_tensor;
    OP_REQUIRES_OK(context, context->allocate_output(0, {}, &output_tensor));
    output_tensor->scalar<tensorflow::tstring>()() = std::move(served_at_id);
  }
};

REGISTER_KERNEL_BUILDER(Name("ServeSlices").Device(tensorflow::DEVICE_CPU),
                        ServeSlicesOp);

}  // namespace

}  // namespace fcp
