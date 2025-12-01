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
"""Utilities specific to the manipulation of tensors and operators."""

from typing import Any, Callable, Optional, Union

import tensorflow as tf


######################################################################
# Helper functions for names and naming.
#
def bare_name(v) -> str:
  """Strips off the part after the colon in a tensor name."""
  name = name_or_str(v)
  if name[0] == '^':
    name = name[1:]
  # User specified names are everything up to the first colon. User supplied
  # names cannot contain colons, TensorFlow will raise an error on invalid name.
  colon = name.find(':')
  if colon >= 0:
    return name[:colon]
  else:
    return name


def name_or_str(v) -> str:
  """Returns the name of v, or if v has no name attr, str(op)."""
  if hasattr(v, 'name'):
    name = v.name
    assert isinstance(name, str)
    return name
  return str(v)


######################################################################
# Helper function for graphs.
#


def import_graph_def_from_any(an) -> tf.compat.v1.GraphDef:
  """Parses a tf.compat.v1.GraphDef from an Any message.

  Args:
    an: An 'Any' message, which contains a serialized tf.compat.v1.GraphDef. The
      type_url field of the Any message must identify a supported type;
      currently, the only supported type is 'type.googleapis.com/GraphDef'.

  Returns:
    A tf.compat.v1.GraphDef object.
  """
  assert an
  # The only kind of supported graph is a TensorFlow GraphDef.
  assert an.Is(tf.compat.v1.GraphDef.DESCRIPTOR)
  g = tf.compat.v1.GraphDef()
  an.Unpack(g)
  return g


######################################################################
# Helper functions for savers or saverdefs.
#


def save(
    filename: Union[tf.Tensor, str],
    tensor_names: list[str],
    tensors: list[tf.Tensor],
    tensor_slices: Optional[list[str]] = None,
    name: str = 'save',
    save_op: Callable[..., Any] = tf.raw_ops.SaveSlices,
) -> tf.Operation:
  """Saves a list of tensors to file.

  This function always passes a value for the `tensor_slices` argument in order
  to use the `SaveSlices` op (instead of a `Save` op).

  Args:
    filename: A string or a scalar tensor of dtype string that specifies the
      path to file.
    tensor_names: A list of strings.
    tensors: A list of tensors to be saved.
    tensor_slices: An optional list of strings, that specifies the shape and
      slices of a larger virtual tensor that each tensor is a part of. If not
      specified, each tensor is saved as a full slice.
    name: An optional name for the op.
    save_op: A callable that creates the op(s) to use for performing the tensor
      save. Defaults to `tf.raw_ops.SaveSlices`.

  Returns:
    A `SaveSlices` op in graph mode or None in eager mode.
  """
  tensor_slices = tensor_slices if tensor_slices else ([''] * len(tensors))
  return save_op(
      filename=filename,
      tensor_names=tensor_names,
      shapes_and_slices=tensor_slices,
      data=tensors,
      name=name,
  )


def restore(
    filename: Union[tf.Tensor, str],
    tensor_name: str,
    tensor_type: tf.DType,
    tensor_shape: Optional[tf.TensorShape] = None,
    name: str = 'restore',
) -> tf.Tensor:
  """Restores a tensor from the file.

  It is a wrapper of `tf.raw_ops.RestoreV2`. When used in graph mode, it adds a
  `RestoreV2` op to the graph.

  Args:
    filename: A string or a scalar tensor of dtype string that specifies the
      path to file.
    tensor_name: The name of the tensor to restore.
    tensor_type: The type of the tensor to restore.
    tensor_shape: Optional. The shape of the tensor to restore.
    name: An optional name for the op.

  Returns:
    A tensor of dtype `tensor_type`.
  """
  shape_str = ''
  slice_str = ''
  if tensor_shape is not None and tensor_shape.rank > 0:
    shape_str = ' '.join('%d' % d for d in tensor_shape) + ' '
    # Ideally we want to pass an empty string to slice, but this is not allowed
    # because the size of the slice string list (after the string is split by
    # separator ':') needs to match the rank of the tensor (see b/197779415 for
    # more information).
    slice_str = ':-' * tensor_shape.rank
  restored_tensors = tf.raw_ops.RestoreV2(
      prefix=filename,
      tensor_names=[tensor_name],
      shape_and_slices=[shape_str + slice_str],
      dtypes=[tensor_type],
      name=name,
  )
  return restored_tensors[0]
