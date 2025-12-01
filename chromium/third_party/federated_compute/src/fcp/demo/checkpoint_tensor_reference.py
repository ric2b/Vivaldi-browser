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
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expresus or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""MaterializableValueReference that reads from a TensorFlow checkpoint."""

import asyncio
import typing
from typing import Any, Optional
import uuid

import federated_language
import tensorflow as tf
import tensorflow_federated as tff


class CheckpointTensorReference(
    federated_language.program.MaterializableValueReference
):
  """A reference to a tensor in a TF checkpoint file."""

  def __init__(
      self,
      tensor_name: str,
      dtype: tf.dtypes.DType,
      shape: Any,
      checkpoint_future: asyncio.Task,
  ):
    """Constructs a new CheckpointTensorReference object.

    Args:
      tensor_name: The name of the tensor in the TF checkpoint.
      dtype: The type of the tensor.
      shape: The shape of the tensor, expressed as a value convertible to
        `tf.TensorShape`.
      checkpoint_future: A `asyncio.Task` that resolves to the TF checkpoint
        bytes once they're available.
    """
    self._tensor_name = tensor_name
    type_signature = tff.tensorflow.to_type((dtype, shape))
    self._type_signature = typing.cast(
        federated_language.TensorType, type_signature
    )
    self._checkpoint_future = checkpoint_future
    self._tensor: Optional[tf.Tensor] = None

  @property
  def type_signature(self) -> federated_language.Type:
    return self._type_signature

  async def get_value(self) -> federated_language.program.MaterializedValue:
    if self._tensor is None:
      checkpoint = await self._checkpoint_future
      # Write to a file in TensorFlow's RamFileSystem to avoid disk I/O.
      tmpfile = f'ram://{uuid.uuid4()}.ckpt'
      with tf.io.gfile.GFile(tmpfile, 'wb') as f:
        f.write(checkpoint)
      try:
        self._tensor = tf.raw_ops.RestoreV2(
            prefix=tmpfile,
            tensor_names=[self._tensor_name],
            shape_and_slices=[''],
            dtypes=[self._type_signature.dtype])[0]
      finally:
        tf.io.gfile.remove(tmpfile)

    try:
      return self._tensor.numpy()
    except AttributeError as e:
      raise ValueError('get_value is only supported in eager mode.') from e
