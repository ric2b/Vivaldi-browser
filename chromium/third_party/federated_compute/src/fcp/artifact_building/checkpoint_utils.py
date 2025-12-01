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
"""Helper methods for working with demo server checkpoints."""

import collections
from collections.abc import Callable, Iterable, Mapping, Sequence
from typing import Any, Optional, Union

import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff
import tree

from fcp.artifact_building import artifact_constants
from fcp.artifact_building import tensor_utils
from fcp.artifact_building import type_checks
from fcp.artifact_building import variable_helpers
from fcp.protos import plan_pb2

SAVE_SERVER_SAVEPOINT_NAME = 'save_server_savepoint'


def create_server_checkpoint_vars_and_savepoint(
    *,
    server_state_type: federated_language.StructType,
    server_metrics_type: Optional[federated_language.StructType] = None,
    write_metrics_to_checkpoint: bool = True,
    additional_checkpoint_metadata_var_fn: Optional[
        Callable[
            [
                federated_language.StructType,
                federated_language.StructType,
                bool,
            ],
            list[tf.Variable],
        ]
    ] = None,
) -> tuple[
    list[tf.Variable],
    list[tf.Variable],
    list[tf.Variable],
    plan_pb2.CheckpointOp,
]:
  """Creates tf.Variables for a server checkpoint and the associated savepoint.

  The variables and the associated saver are constructed in the default graph.

  For now, only `server_state_type` is required. If metrics are to be saved in
  the server checkpoint, `server_metrics_type` and `server_result_type` must
  be provided. `server_state_type` refers to the server state portion of the
  checkpoint and is used in the `Restore` op of the savepoint. The
  `server_metrics_type` refers to the metrics saved in the checkpoint, and is
  not used in the `Restore` op of the savepoint. `server_result_type` refers to
  the complete round result structure stored in the checkpoint for a round.

  Args:
    server_state_type: A `federated_language.Type` with the type signature of
      the state. This is used to construct the server state variable names
      stored in the checkpoint and is used to create the metadata variables for
      the checkpoint if `server_result_type` is not provided.
    server_metrics_type: Optional. A `federated_language.Type` with the type
      signature of the metrics. If provided, this is used to construct the
      metric variable names that are stored in the checkpoint.
    write_metrics_to_checkpoint: If False, revert to legacy behavior where
      metrics and other non-state values were handled by post-processing
      separate from the outputted checkpoint.
    additional_checkpoint_metadata_var_fn: An optional method that takes in the
      server_state_type, server_metrics_type, and write_metrics_to_checkpoint to
      produce additional metadata variables.

  Returns:
    A tuple `(state_vars, metric_vars, metadata_vars, savepoint)`:
    - `state_vars` is a Python `list` of variables that hold the state.
    - `metric_vars` is a Python `list` of variables that hold the metrics.
    - `metadata_vars` is a Python `list` of variables that hold optional
      metadata.
    - `savepoint` is the associated savepoint, i.e., an instance of
      `plan_pb2.CheckpointOp` with a saver configured for saving the
      `state_vars`, `metadata_vars`, and, if write_metrics_to_checkpoint is
      True, `metric_vars`, and restoring the `state_vars` and
      `metadata_vars`.
  """
  has_metrics = False
  metric_vars = []
  save_tensor_name = None
  type_checks.check_type(
      server_state_type, federated_language.Type, name='server_state_type'
  )
  state_vars = variable_helpers.create_vars_for_tff_type(
      server_state_type, artifact_constants.SERVER_STATE_VAR_PREFIX
  )
  var_names = list(map(tensor_utils.bare_name, state_vars))
  metadata_vars = []
  if server_metrics_type is not None:
    type_checks.check_type(
        server_metrics_type, federated_language.Type, name='server_metrics_type'
    )
    metric_vars = variable_helpers.create_vars_for_tff_type(
        server_metrics_type, artifact_constants.SERVER_METRICS_VAR_PREFIX
    )
    if additional_checkpoint_metadata_var_fn:
      metadata_vars = additional_checkpoint_metadata_var_fn(
          state_vars,  # pytype: disable=wrong-arg-types
          metric_vars,
          write_metrics_to_checkpoint,
      )

    if isinstance(server_metrics_type, federated_language.StructType):
      flattened = tree.flatten(
          tff.structure.to_odict_or_tuple(server_metrics_type)
      )
    else:
      flattened = [server_metrics_type]
    has_metrics = bool(flattened)
    if has_metrics and write_metrics_to_checkpoint:
      var_names.extend(list(map(tensor_utils.bare_name, metric_vars)))

      temp_saver_for_all_vars = create_deterministic_saver(
          var_list=state_vars + metadata_vars + metric_vars,
          name=SAVE_SERVER_SAVEPOINT_NAME,
      )
      temp_saver_def = temp_saver_for_all_vars.as_saver_def()
      save_tensor_name = temp_saver_def.save_tensor_name
  else:
    if additional_checkpoint_metadata_var_fn:
      metadata_vars = additional_checkpoint_metadata_var_fn(
          state_vars,  # pytype: disable=wrong-arg-types
          None,
          write_metrics_to_checkpoint,
      )

  saver = create_deterministic_saver(
      var_list=state_vars + metadata_vars,
      name='{}_savepoint'.format(artifact_constants.SERVER_STATE_VAR_PREFIX),
  )
  savepoint = plan_pb2.CheckpointOp()
  savepoint.saver_def.CopyFrom(saver.as_saver_def())

  if save_tensor_name is not None:
    # Replace the save_tensor_name to the one in
    # temp_saver_for_all_vars so that we are additionally saving metrics vars
    # in the checkpoint that don't need to be restored as part of the input
    # computation state.
    # Once we create the server GraphDef, we will edit the GraphDef directly
    # to ensure the input filename links to the filename tensor from the
    # `savepoint`.
    savepoint.saver_def.save_tensor_name = save_tensor_name
  return state_vars, metric_vars, metadata_vars, savepoint


def create_state_vars_and_savepoint(
    type_spec: variable_helpers.AllowedTffTypes, name: str
) -> tuple[list[tf.Variable], plan_pb2.CheckpointOp]:
  """Creates state variables and their savepoint as a `plan_pb2.CheckpointOp`.

  The variables and the associated saver are constructed in the default graph.

  Args:
    type_spec: An instance of `federated_language.Type` with the type signature
      of the state.
    name: The string to use as a basis for naming the vars and the saver. The
      vars will be under `${name}_state`, and saver under `${name}_savepoint`.

  Returns:
    A tuple `(vars, savepoint)`, where `vars` is a Python `list` of variables
    that hold the state, and `savepoint` is the associated savepoint, i.e.,
    an instance of `plan_pb2.CheckpointOp` with a saver configured for saving
    and restoring the `vars`.

  Raises:
    ValueError: If the name is empty.
  """
  state_vars, saver = create_state_vars_and_saver(type_spec, name)
  savepoint = plan_pb2.CheckpointOp()
  savepoint.saver_def.CopyFrom(saver.as_saver_def())
  return state_vars, savepoint


def create_state_vars_and_saver(
    type_spec: variable_helpers.AllowedTffTypes, name: str
) -> tuple[list[tf.Variable], tf.compat.v1.train.Saver]:
  """Creates state variables and the associated saver.

  The variables and the associated saver are constructed in the default graph.

  Args:
    type_spec: An instance of `federated_language.Type` with the type signature
      of the state.
    name: The string to use as a basis for naming the vars and the saver. The
      vars will be under `${name}_state`, and saver under `${name}_savepoint`.

  Returns:
    A tuple `(vars, savepoint)`, where `vars` is a Python `list` of variables
    that hold the state, and `savepoint` is the associated
    `tf.compat.v1.train.Saver`.

  Raises:
    ValueError: If the name is empty.
  """
  type_checks.check_type(type_spec, federated_language.Type, name='type_spec')
  type_checks.check_type(name, str, name='name')
  if not name:
    raise ValueError('Name cannot be empty.')
  state_vars = variable_helpers.create_vars_for_tff_type(type_spec, name)
  saver = create_deterministic_saver(
      state_vars, name='{}_savepoint'.format(name)
  )
  return state_vars, saver


def restore_tensors_from_savepoint(
    tensor_specs: Iterable[tf.TensorSpec], filepath_tensor: tf.Tensor
) -> list[tf.Tensor]:
  """Restores tensors from a checkpoint designated by a tensor filepath.

  Args:
    tensor_specs: A `list` of `tf.TensorSpec`s with the names and dtypes of the
      tensors to restore.
    filepath_tensor: A placeholder tensor that contains file names with a given
      pattern.

  Returns:
    A list of restored tensors.
  """
  return [
      tensor_utils.restore(
          filepath_tensor, tensor_utils.bare_name(spec.name), spec.dtype
      )
      for spec in tensor_specs
  ]


def create_deterministic_saver(
    var_list: Union[Iterable[tf.Variable], Mapping[str, tf.Variable]],
    *args,
    **kwargs,
) -> tf.compat.v1.train.Saver:
  """Creates a `tf.compat.v1.Saver` that is deterministic.

  This method sorts the `var_list` to ensure a deterministic ordering which
  in turn ensures a deterministic checkpoint.

  Uses `tf.compat.v1.train.SaverDef.V1` version for writing checkpoints.

  Args:
    var_list: An `Iterable` or `str` keyed `Mapping` of `tf.Variables`. In the
      case of a `dict`, the keys become the names of the checkpoint variables
      (rather than reading the names off the `tf.Variable` values).
    *args: Positional arguments forwarded to the `tf.compat.v1.train.Saver`
      constructor.
    **kwargs: Keyword arguments forwarded to the `tf.compat.v1.train.Saver`
      constructor.

  Returns:
    A `tf.compat.v1.train.Saver` instance.
  """
  if isinstance(var_list, collections.abc.Mapping):
    determinisic_names = collections.OrderedDict(sorted(var_list.items()))
  elif isinstance(var_list, collections.abc.Iterable):
    determinisic_names = sorted(var_list, key=lambda v: v.name)
  else:
    raise ValueError(
        'Do not know how to make a deterministic saver for '
        '`var_list` of type [{t}]. Must be a Mapping or Sequence'.format(
            t=type(var_list)
        )
    )
  return tf.compat.v1.train.Saver(
      determinisic_names,
      write_version=tf.compat.v1.train.SaverDef.V1,
      *args,
      **kwargs,
  )


def tff_type_to_dtype_list(
    tff_type: variable_helpers.AllowedTffTypes,
) -> list[tf.DType]:
  """Creates a flat list of `tf.DType`s for tensors in a `federated_language.Type`.

  Args:
    tff_type: Either a `federated_language.StructType`,
      `federated_language.FederatedType`, or a `federated_language.TensorType`
      object.

  Returns:
    A flat list of `tf.DType`s.
  """
  type_checks.check_type(
      tff_type,
      (
          federated_language.TensorType,
          federated_language.FederatedType,
          federated_language.StructType,
      ),
  )
  if isinstance(tff_type, federated_language.TensorType):
    return [tf.as_dtype(tff_type.dtype)]
  elif isinstance(tff_type, federated_language.FederatedType):
    return tff_type_to_dtype_list(tff_type.member)
  else:  # federated_language.StructType
    elem_list = []
    for elem_type in tff_type:
      elem_list.extend(tff_type_to_dtype_list(elem_type))
    return elem_list


def tff_type_to_tensor_spec_list(
    tff_type: variable_helpers.AllowedTffTypes,
) -> list[tf.TensorSpec]:
  """Creates a flat list of tensor specs for tensors in a `federated_language.Type`.

  Args:
    tff_type: Either a `federated_language.StructType`,
      `federated_language.FederatedType` or a `federated_language.TensorType`
      object.

  Returns:
    A flat list of `tf.TensorSpec`s.
  """
  type_checks.check_type(
      tff_type,
      (
          federated_language.TensorType,
          federated_language.FederatedType,
          federated_language.StructType,
      ),
  )
  if isinstance(tff_type, federated_language.TensorType):
    return [tf.TensorSpec(tff_type.shape, dtype=tf.as_dtype(tff_type.dtype))]
  elif isinstance(tff_type, federated_language.FederatedType):
    return tff_type_to_tensor_spec_list(tff_type.member)
  else:  # federated_language.StructType
    elem_list = []
    for elem_type in tff_type:
      elem_list.extend(tff_type_to_tensor_spec_list(elem_type))
    return elem_list


def pack_tff_value(
    tff_type: variable_helpers.AllowedTffTypes, value_list: Any
) -> Any:
  """Packs a list of values into a shape specified by a `federated_language.Type`.

  Args:
    tff_type: Either a `federated_language.StructType`,
      `federated_language.FederatedType`, or a `federated_language.TensorType`
      object.
    value_list: A flat list of `tf.Tensor` or `CheckpointTensorReference`.

  Returns:
    A Python container with a structure consistent with a
    `federated_language.Type`.

  Raises:
    ValueError: If the number of leaves in `tff_type` does not match the length
    of `value_list`, or `tff_type` is of a disallowed type.
  """
  type_checks.check_type(
      tff_type,
      (
          federated_language.TensorType,
          federated_language.FederatedType,
          federated_language.StructType,
      ),
  )

  # We must "unwrap" any FederatedTypes because the packing logic below will
  # fail to recurse into them. Instead, we remove all the FederatedTypes,
  # because we're only trying to build up a Python tree structure that matches
  # the struct/tensor types from a list of values.
  def remove_federated_types(
      type_spec: federated_language.Type,
  ) -> Union[federated_language.StructType, federated_language.TensorType]:
    """Removes `FederatedType` from a type tree, returning a new tree."""
    if isinstance(type_spec, federated_language.TensorType):
      return type_spec
    elif isinstance(type_spec, federated_language.FederatedType):
      return type_spec.member
    elif isinstance(type_spec, federated_language.StructType):
      return federated_language.StructType(
          (elem_name, remove_federated_types(elem_type))
          for elem_name, elem_type in type_spec.items()
      )
    else:
      raise ValueError(
          'Must be either federated_language.TensorType,'
          ' federated_language.FederatedType, or'
          f' federated_language.StructType. Got a {type(type_spec)}'
      )

  try:
    tff_type = remove_federated_types(tff_type)
  except ValueError as e:
    raise ValueError(
        '`tff_type` is not packable, see earlier error. '
        f'Attempted to pack type: {tff_type}'
    ) from e

  ordered_dtypes = tff_type_to_dtype_list(tff_type)
  if len(ordered_dtypes) != len(value_list):
    raise ValueError(
        'The number of leaves in `tff_type` must equals the length'
        ' of `value_list`. Found `tff_type` with'
        f' {len(ordered_dtypes)} leaves and `value_list` of length'
        f' {len(value_list)}.'
    )

  if isinstance(tff_type, federated_language.TensorType):
    return value_list[0]
  elif isinstance(tff_type, federated_language.StructType):

    def _unflatten_as(type_spec, flat_sequence):

      def _pack(type_spec, value_iter):
        if isinstance(type_spec, federated_language.StructType):
          elements = [(n, _pack(v, value_iter)) for n, v in type_spec.items()]
          return tff.structure.Struct(elements)
        else:
          return next(value_iter)

      value_iter = iter(flat_sequence)
      return _pack(type_spec, value_iter)

    return _unflatten_as(tff_type, value_list)
  else:
    raise ValueError(
        '`tff_type` must be either federated_language.TensorType or '
        'federated_language.StructType, reaching here is an internal coding '
        'error, please file a bug.'
    )


def variable_names_from_structure(
    tff_structure: Union[
        Mapping[str, object],
        Sequence[object],
        tff.structure.Struct,
        tf.Tensor,
    ],
    name: str = 'v',
) -> list[str]:
  """Creates a flattened list of variable names for the given structure.

  If the `tff_structure` is a `tf.Tensor`, the name is the `name` parameter if
  specified, otheriwse a default name: `v`. If `tff_structure` is a
  `tff.structure.Struct` then '/' is used between inner and outer fields
  together with the tuple name or index of the element in the tuple.

  Some examples:
  1. If the `tff_structure` is `<'a'=tf.constant(1.0), 'b'=tf.constant(0.0)>`
     and name is not specified, the returned variable name list is
     ['v/a', 'v/b'].
  2. If the `tff_structure` is `<None=tf.constant(1.0), None=tf.constant(0.0)>`
     and `name` is `update`, the returned variable name list is
     ['update/0', 'update/1'].
  3. If the `tff_structure` is
     `<'a'=<'b'=tf.constant(1.0), 'c'=tf.constant(0.0)>>` and `name` is
     `update`, the returned variable name list is ['update/a/b', 'update/a/c'].
  4. If the `tff_structure` is
     `<'a'=<'b'=tf.constant(1.0), 'c'=tf.constant(1.0), tf.constant(0.0)>>` and
     `name` is `update`, the returned variable name list is ['update/a/b',
    'update/a/c', 'update/a/2'].

  Args:
    tff_structure: Either a `tff.structure.Struct` or a `tf.Tensor` object.
    name: The preferred name to use at the top-most level (if not None, must be
      a string). If `tff_structure` is a `tff.structure.Struct`, the names of
      the inner fields will be scoped under `name`, e.g. `some_name/field_name`.

  Returns:
    A flat Python `list` of `str` names.

  Raises:
    TypeError: If either argument is of the wrong type.
  """
  type_checks.check_type(
      tff_structure,
      (Mapping, Sequence, tff.structure.Struct, tf.Tensor),
      name='structure_type',
  )
  type_checks.check_type(name, str, name='name')
  if isinstance(tff_structure, tf.Tensor):
    return [name]
  elif isinstance(tff_structure, tff.structure.Struct):
    return variable_names_from_structure(
        tff.structure.to_odict_or_tuple(tff_structure),
        name,
    )
  elif isinstance(tff_structure, Mapping):
    result = []
    for field_name, field_type in tff_structure.items():
      result.extend(
          variable_names_from_structure(
              field_type,
              name=name + '/' + field_name,
          )
      )
    return result
  elif isinstance(tff_structure, Sequence):
    result = []
    for index, field_type in enumerate(tff_structure):
      # Default the name of the element to its index so that we don't wind up
      # with multiple child fields listed under `/v/`
      field_name = str(index)
      result.extend(
          variable_names_from_structure(
              field_type,
              name=name + '/' + field_name,
          )
      )
    return result
  else:
    raise TypeError(
        'Cannot create variable names from [{t}] type. Short-hand: {s}'.format(
            t=type(tff_structure), s=tff_structure
        )
    )


def is_structure_of_allowed_types(
    structure: Union[
        tff.structure.Struct,
        Mapping[str, object],
        Sequence[object],
        tf.Tensor,
        np.ndarray,
        np.number,
        int,
        float,
        str,
        bytes,
    ],
) -> bool:
  """Checks if each node in `structure` is an allowed type for serialization."""
  if isinstance(structure, tff.structure.Struct):
    structure = tff.structure.to_odict_or_tuple(structure)
  flattened = tree.flatten(structure)
  for item in flattened:
    if not (
        tf.is_tensor(item)
        or isinstance(item, (np.ndarray, np.number, int, float, str, bytes))
    ):
      return False
  return True


def save_tff_structure_to_checkpoint(
    structure: Union[tff.structure.Struct, tf.Tensor],
    ordered_var_names: list[str],
    output_checkpoint_path: str,
) -> None:
  """Saves a TFF structure to a checkpoint file.

  The input `structure` is a either `tff.structure.Struct` or a single
  `tf.Tensor`. This function saves `structure` to a checkpoint file using
  variable names supplied via the `ordered_var_names` argument.

  Args:
    structure: A `tff.structure.Struct` of values or a single value. Each leaf
      in the structure must be a value serializable to a TensorFlow checkpoint.
    ordered_var_names: The list of variable names for the values that appear in
      `structure` after flattening.
    output_checkpoint_path: A string specifying the path to the output
      checkpoint file.

  Raises:
    TypeError: If not all leaves in `structure` are of allowed types.
    ValueError: If the number of `tf.Tensor`s in `structure` does not match
      the size of `ordered_var_names`.
  """
  if isinstance(structure, tff.structure.Struct):
    structure = tff.structure.to_odict_or_tuple(structure)

  if not is_structure_of_allowed_types(structure):
    raise TypeError(
        'Not all leaves in `structure` are `tf.Tensor`s, '
        '`np.ndarray`s, `np.number`s, or Python scalars. Got: '
        f'{tree.map_structure(type, structure)!r})'
    )

  def _to_sequence(obj):
    if isinstance(obj, Mapping):
      return list(obj.values())
    else:
      return None

  # IMPORTANT: Removing `Mapping` types is required because for `Mapping` types
  # `tree.flatten` will sort the items by key before return the values and this
  # function only care about the order of the values not their keys.
  structure = tree.traverse(_to_sequence, structure, top_down=False)
  tensors = tree.flatten(structure)

  if len(tensors) != len(ordered_var_names):
    raise ValueError(
        'The length of `ordered_var_names` does not match the '
        'number of tensors in `structure`:'
        f'{len(ordered_var_names)} != {len(tensors)}'
    )

  tensor_utils.save(
      output_checkpoint_path, tensor_names=ordered_var_names, tensors=tensors
  )
