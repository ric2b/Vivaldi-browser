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

#ifndef FCP_TENSORFLOW_SERVE_SLICES_REGISTRY_H_
#define FCP_TENSORFLOW_SERVE_SLICES_REGISTRY_H_

#include <functional>
#include <string>
#include <utility>

#include "fcp/tensorflow/host_object.h"

// Forward declare Tensor to avoid an explicit dependency on the TensorFlow
// framework. Dependencies of custom ops (which this target is) are not able to
// depend on the full TensorFlow framework.
namespace tensorflow {

class Tensor;

}  // namespace tensorflow

namespace fcp {

// A callback to invoke when the `ServeSlices` custom op is called.
//
// Callbacks are responsible for ensuring that the provided `server_val` is
// sliced up using the provided selection function (`select_fn`) and that the
// resulting slices are made available to clients.
//
// May be invoked from other threads by the TensorFlow runtime.
//
// Inputs:
//   callback_token: The random token associated with this callback by the
//     `HostObjectRegistration` returned by
//     `register_serve_slices_callback(...)`.
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
using ServeSlicesCallback = std::function<std::string(
    /*callback_token=*/RandomToken,
    /*server_val=*/std::vector<tensorflow::Tensor>,
    /*max_key=*/int32_t,
    /*select_fn_initialize_op=*/std::string,
    /*select_fn_server_val_input_tensor_names=*/std::vector<std::string>,
    /*select_fn_key_input_tensor_name=*/absl::string_view,
    /*select_fn_filename_input_tensor_name=*/absl::string_view,
    /*select_fn_target_tensor_name=*/absl::string_view)>;

// Registers a callback to be invoked by the `ServeSlices` op.
//
// Inputs:
//   callback: The callback to register.
//
// Outputs:
//   A `HostObjectRegistration` value which owns the association of the callback
//   with the global callback registry. When this object is destroyed, the
//   callback will be unregistered. To refer to this callback in other methods,
//   use the `token()` method on this object.
inline HostObjectRegistration register_serve_slices_callback(
    ServeSlicesCallback callback) {
  return HostObjectRegistry<ServeSlicesCallback>::Register(
      std::make_shared<ServeSlicesCallback>(std::move(callback)));
}

// Returns the callback registered with the given `token` if one exists.
inline std::optional<std::shared_ptr<ServeSlicesCallback>>
get_serve_slices_callback(RandomToken token) {
  return HostObjectRegistry<ServeSlicesCallback>::TryLookup(token);
}

}  // namespace fcp

#endif  // FCP_TENSORFLOW_SERVE_SLICES_REGISTRY_H_
