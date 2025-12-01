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
"""Provides the `tensor_name` operation.

This wraps the generated op and ensures that necessary shared libraries
are loaded.
"""

import tensorflow as tf

from fcp.tensorflow import gen_tensor_name_py

_tensor_name_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile('./_tensor_name_op.so'))


def tensor_name(tensor):
  """Returns the final graph name of a tensor as a string tensor."""
  if not tf.is_tensor(tensor):
    raise TypeError('`tensor_name` expected a tensor, found object of type '
                    f'{type(tensor)}.')
  return gen_tensor_name_py.tensor_name(input_tensor=tensor)
