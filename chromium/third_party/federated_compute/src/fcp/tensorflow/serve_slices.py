# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Provides the `serve_slices` operation.

This wraps the generated op and ensures that necessary shared libraries
are loaded.
"""

import tensorflow as tf

from fcp.tensorflow import _serve_slices_op
from fcp.tensorflow import gen_serve_slices_py

_serve_slices_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile('./_serve_slices_op.so'))


def _to_tensor_list(list_of_python_values, dtype=None):
  return [
      tf.convert_to_tensor(subvalue, dtype=dtype)
      for subvalue in list_of_python_values
  ]


def serve_slices(callback_token, server_val, max_key, select_fn_initialize_op,
                 select_fn_server_val_input_tensor_names,
                 select_fn_key_input_tensor_name,
                 select_fn_filename_input_tensor_name,
                 select_fn_target_tensor_name):
  """Calls into a preregistered `callback_token` to serve slices of a value.

  In addition to the arguments to this function, `serve_slices` requires that
  a TensorFlow graph containing a selection function (`select_fn`) be provided
  to the server running `serve_slices`. `serve_slices` is responsible for
  providing the server with the names of the placeholder tensor inputs to the
  selection function (`select_fn_X_input_tensor_names`,
  `select_fn_key_input_tensor_name`, and `select_fn_filename_input_tensor_name`)
  and the target tensor to evalate to ensure that the slice is written to the
  provided filename (`select_fn_target_tensor_name`).

  Args:
    callback_token: An string ID corresponding to a callback registered with the
      `register_serve_slices_callback` function. This function will be invoked
      when `serve_slices` is called.
    server_val: A list of arbitrary-typed tensors from which slices may be
      generated using `select_fn`. These tensors must be passed into the
      `select_fn` by writing them to the placeholder tensors named by
      `select_fn_server_val_input_names`, which must contain exactly one tensor
      name for each tensor in `server_val`.
    max_key: An integer indicating the maxiumum slice index which may be
      requested. Slice indices start at zero and may go up to `max_key`
      (inclusive).
    select_fn_initialize_op: An op to run before each call to `select_fn` in
      order to reinitialize any state `select_fn` may contain.
    select_fn_server_val_input_tensor_names: A list of names of the tensors that
      make up the `server_val` portion of the inputs to `select_fn`. Must be the
      same length as the number of tensors in `server_val`.
    select_fn_key_input_tensor_name: The name of the tensor that is the `key`
      input to `select_fn`.
    select_fn_filename_input_tensor_name: The name of the placeholder tensor
      that is the `filename` input to `select_fn`. The `filename` is used to
      specify where the resulting slice should be written.
    select_fn_target_tensor_name: The name of the `target` tensor to run which
      will result in `select_fn`'s output being written to `filename`.

  Returns:
    A string identifier given by the underlying callback which can be used by
    clients to access the generated slices.
  """
  return gen_serve_slices_py.serve_slices(
      callback_token=tf.convert_to_tensor(callback_token, dtype=tf.string),
      server_val=_to_tensor_list(server_val),
      max_key=tf.convert_to_tensor(max_key, dtype=tf.int32),
      select_fn_initialize_op=tf.convert_to_tensor(
          select_fn_initialize_op, dtype=tf.string),
      select_fn_server_val_input_tensor_names=_to_tensor_list(
          select_fn_server_val_input_tensor_names, dtype=tf.string),
      select_fn_key_input_tensor_name=tf.convert_to_tensor(
          select_fn_key_input_tensor_name, dtype=tf.string),
      select_fn_filename_input_tensor_name=tf.convert_to_tensor(
          select_fn_filename_input_tensor_name, dtype=tf.string),
      select_fn_target_tensor_name=tf.convert_to_tensor(
          select_fn_target_tensor_name, dtype=tf.string))


def register_serve_slices_callback(callback):
  """Registers a callback to be invoked by the `ServeSlices` op."""
  def callback_adapter(callback_token, server_val, *args):
    # Convert the serialized TensorProtos to ndarrays.
    tensor_proto = tf.make_tensor_proto(0)
    converted_server_val = [
        tf.make_ndarray(tensor_proto.FromString(val)) for val in server_val
    ]
    return callback(callback_token, converted_server_val, *args)

  return _serve_slices_op.register_serve_slices_callback(callback_adapter)
