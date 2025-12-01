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
"""Helper methods for proto creation logic."""

from typing import Optional

import federated_language
import tensorflow as tf

import tensorflow_federated as tff

from fcp.artifact_building import tensor_utils
from fcp.artifact_building import type_checks
from fcp.protos import plan_pb2


def make_tensor_spec_from_tensor(
    t: tf.Tensor, shape_hint: Optional[tf.TensorShape] = None
) -> tf.TensorSpec:
  """Creates a `TensorSpec` from Tensor w/ optional shape hint.

  Args:
    t: A `tf.Tensor` instance to be used to create a `TensorSpec`.
    shape_hint: A `tf.TensorShape` that provides a fully defined shape in the
      case that `t` is partially defined. If `t` has a fully defined shape,
      `shape_hint` is ignored. `shape_hint` must be compatible with the
      partially defined shape of `t`.

  Returns:
    A `tf.TensorSpec` instance corresponding to the input `tf.Tensor`.

  Raises:
    NotImplementedError: If the input `tf.Tensor` type is not supported.
    TypeError: if `shape_hint` is not `None` and is incompatible with the
      runtime shape of `t`.
  """
  if not tf.is_tensor(t):
    raise NotImplementedError(
        'Cannot handle type {t}: {v}'.format(t=type(t), v=t)
    )
  derived_shape = tf.TensorShape(t.shape)
  if not derived_shape.is_fully_defined() and shape_hint is not None:
    if derived_shape.is_compatible_with(shape_hint):
      shape = shape_hint
    else:
      raise TypeError(
          'shape_hint is not compatible with tensor ('
          f'{shape_hint} vs {derived_shape})'
      )
  else:
    shape = derived_shape
  return tf.TensorSpec(shape, t.dtype, name=t.name)


def make_measurement(
    t: tf.Tensor, name: str, tff_type: federated_language.TensorType
) -> plan_pb2.Measurement:
  """Creates a `plan_pb.Measurement` descriptor for a tensor.

  Args:
    t: A tensor to create the measurement for.
    name: The name of the measurement (e.g. 'server/loss').
    tff_type: The `federated_language.Type` of the measurement.

  Returns:
    An instance of `plan_pb.Measurement`.

  Raises:
    ValueError: If the `dtype`s or `shape`s of the provided tensor and TFF type
      do not match.
  """
  type_checks.check_type(tff_type, federated_language.TensorType)
  if tff_type.dtype != t.dtype:
    raise ValueError(
        f'`tff_type.dtype`: {tff_type.dtype} does not match '
        f"provided tensor's dtype: {t.dtype}."
    )

  if (
      tff.types.is_shape_fully_defined(tff_type.shape)
      and t.shape.is_fully_defined()
      and tff_type.shape != tuple(t.shape.as_list())
  ):
    raise ValueError(
        f'`tff_type.shape`: {tff_type.shape} does not match '
        f"provided tensor's shape: {t.shape}."
    )
  return plan_pb2.Measurement(
      read_op_name=t.name,
      name=name,
      tff_type=tff.types.serialize_type(tff_type).SerializeToString(),
  )


def make_metric(v: tf.Variable, stat_name_prefix: str) -> plan_pb2.Metric:
  """Creates a `plan_pb.Metric` descriptor for a resource variable.

  The stat name is formed by stripping the leading `..../` prefix and any
  colon-based suffix.

  Args:
    v: A variable to create the metric descriptor for.
    stat_name_prefix: The prefix (string) to use in formulating a stat name,
      excluding the trailing slash `/` (added automatically).

  Returns:
    An instance of `plan_pb.Metric` for `v`.

  Raises:
    TypeError: If the arguments are of the wrong types.
    ValueError: If the arguments are malformed (e.g., no leading name prefix).
  """
  type_checks.check_type(stat_name_prefix, str, name='stat_name_prefix')
  if not hasattr(v, 'read_value'):
    raise TypeError('Expected a resource variable, found {!r}.'.format(type(v)))
  bare_name = tensor_utils.bare_name(v.name)
  if '/' not in bare_name:
    raise ValueError(
        'Expected a prefix in the name, found none in {}.'.format(bare_name)
    )
  stat_name = '{}/{}'.format(
      stat_name_prefix, bare_name[(bare_name.find('/') + 1) :]
  )
  return plan_pb2.Metric(variable_name=v.read_value().name, stat_name=stat_name)
