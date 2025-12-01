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
"""Provides the `append_slices` and `merge_appended_slices operations.

This wraps the generated ops and ensures that necessary shared libraries
are loaded.
"""

import tensorflow as tf

from fcp.tensorflow import gen_append_slices_py

_append_slices_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile('./_append_slices_op.so'))


def append_slices(filename, tensor_names, shapes_and_slices, data, name=None):
  """Append slices to `filename`.

  Must be paired with `merge_appended_slices`.

  This op is identical to `tf.raw_ops.SaveSlices`, except that it appends the
  resulting checkpoint to `filename` rather than erasing the contents of
  `filename`.

  Note: the resulting file at `filename` will not be in checkpoint format until
  `merge_appended_slices` has been called.

  Args:
    filename: A `Tensor` fo type `string`. Must have a single element. The name
      of the file to which the tensor should be appended.
    tensor_names: A `Tensor` of type `string`. Shape `[N]`. The names of the
      tensors to be saved.
    shapes_and_slices: A `Tensor` of type `string`. Shape `[N]`. The shapes and
      slice specifications to use when saving the tensors.
    data: A list of `Tensor` objects. `N` tensors to save.
    name: A name for the operation (optional).

  Returns:
    The created `Operation`.
  """
  return gen_append_slices_py.append_slices(
      filename, tensor_names, shapes_and_slices, data, name=name)


def merge_appended_slices(filename, name=None):
  """Merges the appended file created by `append_slices` to a single checkpoint.

  The immediate file output of `append_slices` is not in checkpoint format. It
  must be converted to a checkpoint using this function `merge_appended_slices`.

  Note: Users must call `control_dependencies` or other mechanisms to ensure
  that the `append_slices` calls have executed prior to the execution of
  `merge_appended_slices`.

  Args:
    filename: The name of a file appended to by calls to `append_slices`.
    name: A name for the operation (optional).

  Returns:
    The created `Operation`.
  """
  return gen_append_slices_py.merge_appended_slices(filename, name)
