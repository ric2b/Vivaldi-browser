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
"""Helper functions for writing tests."""

import tempfile
from typing import Any, Mapping

import tensorflow as tf


def create_checkpoint(data: Mapping[str, Any]) -> bytes:
  """Creates a TensorFlow checkpoint."""
  with tempfile.NamedTemporaryFile() as tmpfile:
    with tf.compat.v1.Session() as session:
      session.run(
          tf.raw_ops.Save(
              filename=tmpfile.name,
              tensor_names=list(data.keys()),
              data=list(data.values())))
    with open(tmpfile.name, 'rb') as f:
      return f.read()


def read_tensor_from_checkpoint(checkpoint: bytes, tensor_name: str,
                                dt: tf.DType) -> Any:
  """Reads a single tensor from a checkpoint."""
  with tempfile.NamedTemporaryFile('wb') as tmpfile:
    tmpfile.write(checkpoint)
    tmpfile.flush()
    with tf.compat.v1.Session() as session:
      return session.run(
          tf.raw_ops.Restore(
              file_pattern=tmpfile.name, tensor_name=tensor_name, dt=dt))
