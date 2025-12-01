# Copyright 2022 Google LLC
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
"""Tests for serve_slices_registry."""

from unittest import mock

from absl.testing import absltest
import numpy as np
import tensorflow as tf

from fcp.tensorflow import serve_slices
from fcp.tensorflow import serve_slices as serve_slices_registry

SERVER_VAL = (1, 2.0, b'foo')
SERVER_VAL_NP_DTYPE = (np.int32, np.float32, object)
MAX_KEY = 44
SELECT_FN_INITIALIZE_OP = 'init_the_things'
SELECT_FN_SERVER_VAL_INPUT_TENSOR_NAMES = ['a', 'b', 'c']
SELECT_FN_KEY_INPUT_TENSOR_NAME = 'bar'
SELECT_FN_FILENAME_TENSOR_NAME = 'goofy'
SELECT_FN_TARGET_TENSOR_NAME = 'goobler'


class ServeSlicesRegistryTest(absltest.TestCase):

  def test_register_serve_slices_callback(self):
    with tf.Graph().as_default() as graph:
      # Create a placeholder with a fixed name to allow the code running the
      # graph to provide input.
      callback_token = tf.compat.v1.placeholder(dtype=tf.string)
      served_at_id = serve_slices.serve_slices(
          callback_token=callback_token,
          server_val=SERVER_VAL,
          max_key=MAX_KEY,
          select_fn_initialize_op=SELECT_FN_INITIALIZE_OP,
          select_fn_server_val_input_tensor_names=SELECT_FN_SERVER_VAL_INPUT_TENSOR_NAMES,
          select_fn_key_input_tensor_name=SELECT_FN_KEY_INPUT_TENSOR_NAME,
          select_fn_filename_input_tensor_name=SELECT_FN_FILENAME_TENSOR_NAME,
          select_fn_target_tensor_name=SELECT_FN_TARGET_TENSOR_NAME)

    served_at_value = 'address.at.which.data.is.served'
    mock_callback = mock.Mock(return_value=served_at_value)
    with serve_slices_registry.register_serve_slices_callback(
        mock_callback) as token:
      with tf.compat.v1.Session(graph=graph) as session:
        served_at_out = session.run(
            served_at_id, feed_dict={callback_token: token})
    self.assertEqual(served_at_out, served_at_value.encode())
    mock_callback.assert_called_once_with(
        token,
        [
            np.array(v, dtype=dtype)
            for v, dtype in zip(SERVER_VAL, SERVER_VAL_NP_DTYPE)
        ],
        MAX_KEY,
        SELECT_FN_INITIALIZE_OP,
        SELECT_FN_SERVER_VAL_INPUT_TENSOR_NAMES,
        SELECT_FN_KEY_INPUT_TENSOR_NAME,
        SELECT_FN_FILENAME_TENSOR_NAME,
        SELECT_FN_TARGET_TENSOR_NAME,
    )


if __name__ == '__main__':
  absltest.main()
