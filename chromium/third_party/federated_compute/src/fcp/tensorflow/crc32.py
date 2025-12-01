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
"""Provides the `crc32` operation.

This wraps the generated op and ensures that necessary shared libraries
are loaded.
"""

from typing import Optional

import tensorflow as tf

from fcp.tensorflow import gen_crc32_py

_crc32_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile('./_crc32_op.so'))


def crc32(tensor: tf.Tensor, name: Optional[str] = None) -> tf.Operation:
  """Computes the CRC32 checksum of a Tensor.

  Args:
    tensor: The input `Tensor`.
    name: A name for the operation (optional).

  Returns:
    The created `Operation`.
  """
  return gen_crc32_py.crc32(tensor, name=name)
