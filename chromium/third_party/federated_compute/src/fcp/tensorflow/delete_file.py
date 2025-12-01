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
"""Provides the `delete_file` operation.

This wraps the generated ops and ensures that necessary shared libraries
are loaded.
"""

import tensorflow as tf

from fcp.tensorflow import gen_delete_file_py

_delete_file_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile('./_delete_file_op.so'))


def delete_file(filename: tf.Tensor) -> tf.Operation:
  """Delete file if the filename exists.

  Args:
    filename: The filename to delete.

  Returns:
    The created `Operation`.
  """
  return gen_delete_file_py.delete_file(filename)


def delete_dir(dirname: tf.Tensor, recursively: bool = False) -> tf.Operation:
  """Delete directory if the dirname exists.

  Args:
    dirname: The directory to delete.
    recursively: If true the op attempts to delete also the content.

  Returns:
    The created `Operation`.
  """
  return gen_delete_file_py.delete_dir(dirname, recursively)
