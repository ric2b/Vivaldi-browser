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
"""A library responsible for building Federated Compute plans.

This library builds TFF-backed plans, using the `MapReduceForm` object
output by the TFF compiler pipeline.
"""

import collections
from collections.abc import Callable, Iterable, Mapping, Sequence
import enum
from typing import Optional, TypeVar, Union

import attr
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from fcp.artifact_building import artifact_constants
from fcp.artifact_building import checkpoint_type
from fcp.artifact_building import checkpoint_utils
from fcp.artifact_building import data_spec
from fcp.artifact_building import graph_helpers
from fcp.artifact_building import proto_helpers
from fcp.artifact_building import tensor_utils
from fcp.artifact_building import type_checks
from fcp.artifact_building import variable_helpers
from fcp.protos import plan_pb2
from fcp.tensorflow import append_slices
from fcp.tensorflow import delete_file

SECURE_SUM_BITWIDTH_URI = 'federated_secure_sum_bitwidth'
SECURE_SUM_URI = 'federated_secure_sum'
SECURE_MODULAR_SUM_URI = 'federated_secure_modular_sum'

UNKNOWN_TF_DATATYPE = 0


class SecureAggregationTensorShapeError(Exception):
  """Error raised when secagg tensors do not have fully defined shape."""


@enum.unique
class ClientPlanType(enum.Enum):
  """Option adjusting client plan type during plan building.

  Values:
    TENSORFLOW: The default value. Uses a TF client graph for client
      computation.
    EXAMPLE_QUERY: Uses an example query containing client computation logic in
      the provided example selector(s).
  """

  TENSORFLOW = 'tensorflow'
  EXAMPLE_QUERY = 'example_query'


# A type representing a potentially nested struct of integers.
IntStruct = Union[
    int,
    Mapping[str, Union['IntStruct', int]],
    Sequence[Union['IntStruct', int]],
]


def _compute_secagg_parameters(
    mrf: tff.backends.mapreduce.MapReduceForm,
) -> tuple[IntStruct, IntStruct, IntStruct]:
  """Executes the TensorFlow logic that computes the SecAgg parameters.

  This function makes use of `mrf.secure_sum_bitwidth`,
  `mrf.secure_sum_max_input`, and `mrf.secure_modular_sum_modulus` to derive
  the parameters needed for the SecAgg protocol variants.

  Args:
    mrf: An instance of `tff.backends.mapreduce.MapReduceForm`.

  Returns:
    A 3-tuple of `bitwidth`, `max_input` and `moduli` structures of parameters
    for the associated SecAgg variants.
  """
  type_checks.check_type(mrf, tff.backends.mapreduce.MapReduceForm, name='mrf')
  secagg_parameters = []
  with tf.Graph().as_default() as g:
    for name, computation in [
        ('bitwidth', mrf.secure_sum_bitwidth),
        ('max_input', mrf.secure_sum_max_input),
        ('modulus', mrf.secure_modular_sum_modulus),
    ]:
      secagg_parameters.append(
          graph_helpers.import_tensorflow(name, computation)
      )
  with tf.compat.v1.Session(graph=g) as sess:
    flat_output = sess.run(fetches=tf.nest.flatten(secagg_parameters))
    return tf.nest.pack_sequence_as(secagg_parameters, flat_output)


# A side-channel through which one tensor is securely aggregated.
@attr.s
class SecAggSideChannel:
  # The name of the tensor being aggregated in the client and server graphs.
  tensor_name: str = attr.ib()
  # A proto describing how the side-channel is to be aggregated.
  side_channel_proto: plan_pb2.SideChannel = attr.ib()
  # A placeholder tensor into which the sidechannel aggregation is filled.
  placeholder: tf.Tensor = attr.ib()
  # The variable to feed into the server graph.
  update_var: tf.Variable = attr.ib()


SecAggParam = TypeVar('SecAggParam')


def _create_secagg_sidechannels(
    intrinsic_name: str,
    update_type: variable_helpers.AllowedTffTypes,
    get_modulus_scheme: Callable[
        [SecAggParam], plan_pb2.SideChannel.SecureAggregand
    ],
    params: list[SecAggParam],
) -> list[SecAggSideChannel]:
  """Returns `SecAggSideChannel`s for tensors aggregated with `intrinsic_name`.

  This method also creates variables for the securely-aggregated tensors within
  the current default graph using `create_vars_for_tff_type`.

  Args:
    intrinsic_name: The name of the intrinsic (e.g.
      `federated_secure_sum_bitwidth`) with which the tensors in `update_type`
      are being aggregated.
    update_type: The TFF type representing a structure of all tensors being
      aggregated with `intrinsic_name`.
    get_modulus_scheme: A function which will get the modulus scheme being used.
      This typically requires some additional per-tensor parameters which must
      be supplied using `params`.
    params: A list of arguments to pass to `set_modulus_scheme`. There must be
      exactly one element in this list per tensor in `update_type`.

  Returns:
    A list of `SecAggSideChannel`s describing how to aggregate each tensor.
  """
  # For secure aggregation, we don't use a saver (but still store metadata in a
  # CheckpointOp). Instead we create sidechannel tensors that get fed into the
  # server graph.
  update_vars = variable_helpers.create_vars_for_tff_type(
      update_type, f'{intrinsic_name}_update'
  )

  # For tensors aggregated by secagg, we make sure the tensor names are aligned
  # in both client and sever graph by getting the names from the same method.
  tensor_names = variable_helpers.get_shared_secagg_tensor_names(
      intrinsic_name, update_type
  )
  assert len(update_vars) == len(params) == len(tensor_names), (
      'The length of update_vars, params and tensor_names for'
      f' {{intrinsic_name}} should be all equal, but found: {len(update_vars)},'
      f' {len(params)}, and {len(tensor_names)}.'
  )

  results = []
  for param, update_var, tensor_name in zip(params, update_vars, tensor_names):
    secure_aggregand = get_modulus_scheme(param)
    secure_aggregand.dimension.extend(
        plan_pb2.SideChannel.SecureAggregand.Dimension(size=d.value)
        for d in update_var.shape.dims
    )
    secure_aggregand.dtype = update_var.dtype.base_dtype.as_datatype_enum
    placeholder = tf.compat.v1.placeholder(
        update_var.dtype, update_var.get_shape()
    )
    side_channel_proto = plan_pb2.SideChannel(
        secure_aggregand=secure_aggregand, restore_name=placeholder.name
    )
    results.append(
        SecAggSideChannel(
            tensor_name=tensor_name,
            side_channel_proto=side_channel_proto,
            placeholder=placeholder,
            update_var=update_var,
        )
    )
  return results


def _read_secagg_update_from_sidechannel_into_vars(
    *,  # Require parameters to be named.
    secagg_intermediate_update_vars: list[tf.Variable],
    secure_sum_bitwidth_update_type: (variable_helpers.AllowedTffTypes),
    bitwidths: list[int],
    secure_sum_update_type: (variable_helpers.AllowedTffTypes),
    max_inputs: list[int],
    secure_modular_sum_update_type: (variable_helpers.AllowedTffTypes),
    moduli: list[int],
) -> plan_pb2.CheckpointOp:
  """Creates the `read_secagg_update` op.

  `read_secagg_update` is a `plan_pb2.CheckpointOp` and used to restore the
  secagg tensors in server graph.

  Args:
    secagg_intermediate_update_vars: A list of variables to assign the
      secagg_update_data in the `after_restore_op`.
    secure_sum_bitwidth_update_type: The type of the tensors aggregated using
      `bitwidth`-based secure sum.
    bitwidths: The `bitwidth`s for the tensors that will be aggregated using
      `bitwidth`-based secure summation.
    secure_sum_update_type: The type of the tensors aggregated using
      `max_input`-based secure sum.
    max_inputs: The max_input`s for the tensors that will be aggregated using
      `max_input`-based secure summation.
    secure_modular_sum_update_type: The type of the tensors aggregated using
      modular secure summation.
    moduli: The `modulus`s for the tensors that will be aggregated using modular
      secure summation.

  Returns:
    A `plan_pb2.CheckpointOp` which performs the `read_secagg_update`.
  """
  side_channels: list[SecAggSideChannel] = []

  def _aggregand_for_bitwidth(bitwidth):
    return plan_pb2.SideChannel.SecureAggregand(
        quantized_input_bitwidth=bitwidth
    )

  side_channels += _create_secagg_sidechannels(
      SECURE_SUM_BITWIDTH_URI,
      secure_sum_bitwidth_update_type,
      _aggregand_for_bitwidth,
      bitwidths,
  )

  def _aggregand_for_max_input(max_input):
    # Note the +1-- `max_input` is inclusive, so `base_modulus == max_input`
    # would overflow maximum-valued inputs to zero.
    base_modulus = max_input + 1
    modulus_times_shard_size = (
        plan_pb2.SideChannel.SecureAggregand.ModulusTimesShardSize(
            base_modulus=base_modulus
        )
    )
    return plan_pb2.SideChannel.SecureAggregand(
        modulus_times_shard_size=modulus_times_shard_size
    )

  side_channels += _create_secagg_sidechannels(
      SECURE_SUM_URI,
      secure_sum_update_type,
      _aggregand_for_max_input,
      max_inputs,
  )

  def _aggregand_for_modulus(modulus):
    fixed_modulus = plan_pb2.SideChannel.SecureAggregand.FixedModulus(
        modulus=modulus
    )
    return plan_pb2.SideChannel.SecureAggregand(fixed_modulus=fixed_modulus)

  side_channels += _create_secagg_sidechannels(
      SECURE_MODULAR_SUM_URI,
      secure_modular_sum_update_type,
      _aggregand_for_modulus,
      moduli,
  )

  # Operations assigning from sidechannel placeholders to update variables.
  assign_placeholders_to_updates = []
  # Operations assigning from update variables to the result variables.
  assign_updates_to_intermediate = []
  read_secagg_update = plan_pb2.CheckpointOp()
  for intermediate_update_var, side_channel in zip(
      secagg_intermediate_update_vars, side_channels
  ):
    assign_placeholders_to_updates.append(
        side_channel.update_var.assign(side_channel.placeholder)
    )
    assign_updates_to_intermediate.append(
        intermediate_update_var.assign(side_channel.update_var)
    )
    read_secagg_update.side_channel_tensors[side_channel.tensor_name].CopyFrom(
        side_channel.side_channel_proto
    )

  read_secagg_update.before_restore_op = tf.group(
      *(assign_placeholders_to_updates)
  ).name
  read_secagg_update.after_restore_op = tf.group(
      *(assign_updates_to_intermediate)
  ).name

  return read_secagg_update


def _merge_secagg_vars(
    secure_sum_bitwidth_update_type: federated_language.Type,
    secure_sum_update_type: federated_language.Type,
    secure_modular_sum_update_type: federated_language.Type,
    flattened_moduli: list[int],
    variables: list[tf.Variable],
    tensors: list[tf.Variable],
) -> list[tf.Operation]:
  """Generates a set of ops to `merge` secagg `tensors` into `variables`."""
  if len(variables) != len(tensors):
    raise ValueError(
        'Expected an equal number of variables and tensors, but found '
        f'{len(variables)} variables and {len(tensors)} tensors.'
    )
  num_simple_add_vars = federated_language.framework.type_count(
      federated_language.to_type([
          secure_sum_bitwidth_update_type,
          secure_sum_update_type,
      ]),
      lambda x: not isinstance(x, federated_language.StructType),
  )
  num_modular_add_vars = federated_language.framework.type_count(
      secure_modular_sum_update_type,
      lambda x: not isinstance(x, federated_language.StructType),
  )
  # There must be one variable and tensor for each tensor in the secure update
  # types.
  num_vars_from_types = num_simple_add_vars + num_modular_add_vars
  if num_vars_from_types != len(variables):
    raise ValueError(
        'Expected one variable for each leaf element of the secagg update, but '
        f'found {len(variables)} variables and {num_vars_from_types} leaf '
        'elements in the following types:\n'
        f'secure_sum_bitwidth_update_type: {secure_sum_bitwidth_update_type}\n'
        f'secure_sum_update_type: {secure_sum_update_type}\n'
        f'secure_modular_sum_update_type: {secure_modular_sum_update_type}\n'
    )
  if num_modular_add_vars != len(flattened_moduli):
    raise ValueError(
        'Expected one modulus for each leaf element of the secure modular sum '
        f'update type. Found {len(flattened_moduli)} moduli and '
        f'{num_modular_add_vars} leaf elements in the secure modular sum '
        f'update type:\n{secure_modular_sum_update_type}'
    )
  # Add `tensors` to `vars`, using simple addition for the first
  # `num_secagg_simple_add_vars` variables and modular addition for the rest
  # (those coming from `secure_modular_sum`).
  ops = []
  simple_add_vars = variables[:num_simple_add_vars]
  simple_add_tensors = tensors[:num_simple_add_vars]
  for variable, tensor in zip(simple_add_vars, simple_add_tensors):
    ops.append(variable.assign_add(tensor))
  modular_add_vars = variables[num_simple_add_vars:]
  modular_add_tensors = tensors[num_simple_add_vars:]
  for modulus, (variable, tensor) in zip(
      flattened_moduli, zip(modular_add_vars, modular_add_tensors)
  ):
    new_sum = tf.math.add(variable.read_value(), tensor)
    modular_sum = tf.math.floormod(new_sum, modulus)
    ops.append(variable.assign(tf.reshape(modular_sum, tf.shape(variable))))
  return ops


def _is_empty_tff_value(type_signature: federated_language.Type) -> bool:
  """Determines whether this type signature represents an empty TFF value.

  Empty TFF values have a `federated_language.Type` that contains only
  `federated_language.StructType` (and
  structure of `federated_language.StructType`s). TFF values that contain any
  non-
  `federated_language.StructType`, e.g., `federated_language.TensorType`, are
  considered non-empty (even
  `federated_language.TensorType` with shape None or 0 since it may just not be
  possible to
  define the shape at compile time). For any `federated_language.FederatedType`
  instance, its
  `placement` information is ignored, and only the non-federated part (i.e.,
  its member `federated_language.Type`), will be checked.

  Args:
    type_signature: The TFF type signature to evaluate.

  Returns:
    Boolean indicating whether the TFF type signature is considered empty.
  """
  return federated_language.framework.type_contains_only(
      type_signature,
      lambda t: isinstance(
          t, (federated_language.StructType, federated_language.FederatedType)
      ),
  )


def _generate_server_aggregation_configs_for_intrinsic_call(
    intrinsic_uri: str,
    input_tensor_specs: list[list[tf.TensorSpec]],
    output_tensor_specs: list[tf.TensorSpec],
) -> list[plan_pb2.ServerAggregationConfig]:
  """Generates the `ServerAggregationConfig`s for an intrinsic call.

  Args:
    intrinsic_uri: The intrinsic uri for this intrinsic call.
    input_tensor_specs: A list where the ith entry represents the input
      `TensorSpec`s for the ith argument of the intrinsic call.
    output_tensor_specs: A list where the ith entry represents the output
      `TensorSpec` for the ith output of the intrinsic call.

  Returns:
    A list of `ServerAggregationConfig`s that will be used to execute the
    original intrinsic call.
  """
  aggregations = []
  max_input_struct_length = max([len(x) for x in input_tensor_specs])
  max_struct_length = max(max_input_struct_length, len(output_tensor_specs))
  for i in range(max_struct_length):
    intrinsic_args = []
    for j, _ in enumerate(input_tensor_specs):
      # Scale up any "smaller" structure args by reusing their last element.
      tensor_spec = input_tensor_specs[j][
          min(i, len(input_tensor_specs[j]) - 1)
      ]
      if tensor_spec.name.startswith('update'):
        intrinsic_args.append(
            plan_pb2.ServerAggregationConfig.IntrinsicArg(
                input_tensor=tensor_spec.experimental_as_proto()
            )
        )
      else:
        intrinsic_args.append(
            plan_pb2.ServerAggregationConfig.IntrinsicArg(
                state_tensor=tensor_spec.experimental_as_proto()
            )
        )
    aggregations.append(
        plan_pb2.ServerAggregationConfig(
            intrinsic_uri=intrinsic_uri,
            intrinsic_args=intrinsic_args,
            # Scale up the output structure by reusing the last element if
            # needed.
            output_tensors=[
                output_tensor_specs[
                    min(i, len(output_tensor_specs) - 1)
                ].experimental_as_proto()
            ],
        )
    )
  return aggregations


def _build_server_graphs_from_distribute_aggregate_form(
    daf: tff.backends.mapreduce.DistributeAggregateForm,
    grappler_config: tf.compat.v1.ConfigProto,
    write_metrics_to_checkpoint: bool = False,
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
    Optional[tf.compat.v1.GraphDef],
    tf.compat.v1.GraphDef,
    plan_pb2.ServerPhaseV2,
    set[tf.TensorSpec],
]:
  """Generates the server plan components based on DistributeAggregateForm.

  Derives the pre-broadcast, aggregation, and post-aggregation logical
  components in the ServerPhaseV2 message that will be executed on the server.
  The pre-broadcast and post-aggregation components are to be executed with a
  single TF sess.run call using the corresponding GraphDef. The aggregation
  component is to be executed natively (i.e. not using TensorFlow) according to
  the aggregation messages contained in the ServerPhaseV2 message.

  Args:
    daf: An instance of `tff.backends.mapreduce.DistributeAggregateForm`.
    grappler_config: The config specifying Grappler optimizations for TFF-
      generated graphs.
    write_metrics_to_checkpoint: If False, revert to legacy behavior where
      metrics values were handled by post-processing separate from the outputted
      checkpoint. Regardless, they will additionally continue to be written to
      recordio checkpoints as defined by the Plan proto.
    additional_checkpoint_metadata_var_fn: An optional method that takes in a
      server state type, a server metrics type, and a boolean determining
      whether to revert to legacy metrics behavior to produce additional
      metadata variables.

  Returns:
    A `tuple` containing the following (in order):
      - The server_prepare GraphDef (if needed),
      - The server_result GraphDef,
      - The ServerPhaseV2 message,
      - A set of the secagg tensor `TensorSpec`s expected to be delivered by the
        client.
  """
  uses_broadcast = not _is_empty_tff_value(
      daf.server_prepare.type_signature.result[0]  # pytype: disable=unsupported-operands
  )
  uses_intermediate_state = not _is_empty_tff_value(
      daf.server_prepare.type_signature.result[1]  # pytype: disable=unsupported-operands
  )
  # If the server state or intermediate state is non-empty, we can skip running
  # the server_prepare logic.
  run_prepare_logic = uses_broadcast or uses_intermediate_state

  if run_prepare_logic:
    # Generate the TensorFlow graph needed to execute the server_prepare step,
    # including reading input checkpoints and writing output checkpoints.
    server_prepare_input_tensors = []
    server_prepare_target_nodes = []
    with tf.Graph().as_default() as server_prepare_graph:
      # Create the placeholders for the input and output filenames needed by
      # the server_prepare step.
      server_prepare_server_state_input_filepath_placeholder = (
          tf.compat.v1.placeholder(
              name='server_state_input_filepath', shape=(), dtype=tf.string
          )
      )
      server_prepare_output_filepath_placeholder = tf.compat.v1.placeholder(
          name='server_prepare_output_filepath', shape=(), dtype=tf.string
      )
      server_prepare_intermediate_state_output_filepath_placeholder = (
          tf.compat.v1.placeholder(
              name='server_intermediate_state_output_filepath',
              shape=(),
              dtype=tf.string,
          )
      )
      server_prepare_input_tensors.extend([
          server_prepare_server_state_input_filepath_placeholder,
          server_prepare_output_filepath_placeholder,
          server_prepare_intermediate_state_output_filepath_placeholder,
      ])

      # Restore the server state.
      server_state_type = daf.server_prepare.type_signature.parameter
      server_state_vars = variable_helpers.create_vars_for_tff_type(
          server_state_type,  # pytype: disable=wrong-arg-types
          name='server',
      )
      server_state_tensor_specs = tf.nest.map_structure(
          variable_helpers.tensorspec_from_var, server_state_vars
      )
      server_state = checkpoint_utils.restore_tensors_from_savepoint(
          server_state_tensor_specs,
          server_prepare_server_state_input_filepath_placeholder,
      )

      # TODO(team): Add support for federated select slice generation.

      # Perform the server_prepare step.
      prepared_values, intermediate_state_values = (
          graph_helpers.import_tensorflow(
              'server_prepare',
              federated_language.framework.ConcreteComputation.from_building_block(
                  tff.backends.mapreduce.consolidate_and_extract_local_processing(
                      daf.server_prepare.to_building_block(), grappler_config
                  )
              ),
              server_state,
              split_outputs=True,
          )
      )

      # Create checkpoints storing the broadcast values and intermediate server
      # state, if needed
      if uses_broadcast:
        save_tensor_names = variable_helpers.variable_names_from_type(
            daf.server_prepare.type_signature.result[0],  # pytype: disable=unsupported-operands
            name='client',
        )
        prepared_values_save_op = tensor_utils.save(
            filename=server_prepare_output_filepath_placeholder,
            tensor_names=save_tensor_names,
            tensors=prepared_values,
            name='save_prepared_values_tensors',
        )
        server_prepare_target_nodes.append(prepared_values_save_op.name)

      if uses_intermediate_state:
        intermediate_state_values_save_op = tensor_utils.save(
            filename=server_prepare_intermediate_state_output_filepath_placeholder,
            tensor_names=variable_helpers.variable_names_from_type(
                daf.server_prepare.type_signature.result[1],  # pytype: disable=unsupported-operands
                'intermediate_state',
            ),
            tensors=intermediate_state_values,
            name='save_intermediate_state_values_tensors',
        )
        server_prepare_target_nodes.append(
            intermediate_state_values_save_op.name
        )

  # Build aggregations.
  (aggregations, secagg_client_output_tensor_specs) = build_aggregations(daf)

  assert isinstance(
      daf.server_result.type_signature.result[0],  # pytype: disable=unsupported-operands
      federated_language.FederatedType,
  )
  uses_updated_server_state = not _is_empty_tff_value(
      daf.server_result.type_signature.result[0]  # pytype: disable=unsupported-operands
  )
  uses_server_output = not _is_empty_tff_value(
      daf.server_result.type_signature.result[1]  # pytype: disable=unsupported-operands
  )
  assert (
      uses_updated_server_state or uses_server_output
  ), 'Expected at least some result from the round'

  # Generate the TensorFlow graph needed to execute the server_result step,
  # including reading input checkpoints, writing output checkpoints, and
  # generating output tensors. We always run the server_result logic when
  # given a DistributeAggregateForm, since it is non-trivial to determine
  # whether the updated server state contains exactly the same values as the
  # aggregated results.
  server_result_input_tensors = []
  server_result_output_tensors = []
  server_result_target_nodes = []
  with tf.Graph().as_default() as server_result_graph:
    # Create the placeholders for the input and output filenames needed by
    # the server_result step.
    server_result_intermediate_state_input_filepath_placeholder = (
        tf.compat.v1.placeholder(
            name='server_intermediate_state_input_filepath',
            shape=(),
            dtype=tf.string,
        )
    )
    server_result_aggregate_result_input_filepath_placeholder = (
        tf.compat.v1.placeholder(
            name='aggregate_result_input_filepath', shape=(), dtype=tf.string
        )
    )
    server_result_server_state_output_filepath_placeholder = (
        tf.compat.v1.placeholder(
            name='server_state_output_filepath', shape=(), dtype=tf.string
        )
    )
    server_result_input_tensors.extend([
        server_result_intermediate_state_input_filepath_placeholder,
        server_result_aggregate_result_input_filepath_placeholder,
        server_result_server_state_output_filepath_placeholder,
    ])

    # Restore the intermediate server state.
    intermediate_state = []
    if uses_intermediate_state:
      intermediate_state_type = daf.server_result.type_signature.parameter[0]  # pytype: disable=unsupported-operands
      intermediate_state_vars = variable_helpers.create_vars_for_tff_type(
          intermediate_state_type, 'intermediate_state'
      )
      intermediate_state_tensor_specs = tf.nest.map_structure(
          variable_helpers.tensorspec_from_var, intermediate_state_vars
      )
      intermediate_state = checkpoint_utils.restore_tensors_from_savepoint(
          intermediate_state_tensor_specs,
          server_result_intermediate_state_input_filepath_placeholder,
      )

    # Restore the aggregation results.
    aggregate_result_type = federated_language.StructType(
        [daf.server_result.type_signature.parameter[1]]  # pytype: disable=unsupported-operands
    )
    aggregate_result_vars = variable_helpers.create_vars_for_tff_type(
        aggregate_result_type, 'intermediate_update'
    )
    aggregate_result_tensor_specs = tf.nest.map_structure(
        variable_helpers.tensorspec_from_var, aggregate_result_vars
    )
    aggregate_result = checkpoint_utils.restore_tensors_from_savepoint(
        aggregate_result_tensor_specs,
        server_result_aggregate_result_input_filepath_placeholder,
    )

    # Perform the server_result step.
    server_state_values, server_output_values = graph_helpers.import_tensorflow(
        'server_result',
        federated_language.framework.ConcreteComputation.from_building_block(
            tff.backends.mapreduce.consolidate_and_extract_local_processing(
                daf.server_result.to_building_block(), grappler_config
            )
        ),
        (intermediate_state, aggregate_result),
        split_outputs=True,
    )

    # Generate the output TensorSpecProtos for the server metrics if some exist
    # and also prepare the desired metrics names. To maintain the pattern
    # established by MRF-based plans, we use the "metrics" prefix for metrics
    # tensors included in the updated server state checkpoint. We use the
    # "server" prefix for directly produced metrics (metrics that are accessed
    # outside of the checkpoint).
    metric_names_with_metric_prefix = []
    metric_tensors = []
    if uses_server_output:
      # To match the metric naming in the MRF pathway, turn the metric type into
      # a struct if it isn't already.
      metric_type = daf.server_result.type_signature.result[1]  # pytype: disable=unsupported-operands
      assert isinstance(metric_type, federated_language.FederatedType)
      if not isinstance(metric_type.member, federated_language.StructType):
        metric_type = federated_language.StructType([metric_type])
      metric_names_with_server_prefix = (
          variable_helpers.variable_names_from_type(
              metric_type,  # pytype: disable=unsupported-operands
              artifact_constants.SERVER_STATE_VAR_PREFIX,
          )
      )
      metric_names_with_metric_prefix = (
          variable_helpers.variable_names_from_type(
              metric_type,  # pytype: disable=unsupported-operands
              artifact_constants.SERVER_METRICS_VAR_PREFIX,
          )
      )
      metric_tensors = [
          tf.identity(tensor, name)
          for tensor, name in zip(
              server_output_values, metric_names_with_server_prefix
          )
      ]
      for metric in metric_tensors:
        server_result_output_tensors.append(
            proto_helpers.make_tensor_spec_from_tensor(
                metric
            ).experimental_as_proto()
        )

    # Prepare the desired names when saving the updated server state.
    state_names = variable_helpers.variable_names_from_type(
        daf.server_result.type_signature.result[0],  # pytype: disable=unsupported-operands
        artifact_constants.SERVER_STATE_VAR_PREFIX,
    )

    # Generate any extra metadata to include in the updated server state
    # checkpoint.
    metadata_vars = []
    metadata_control_dependencies = []
    if additional_checkpoint_metadata_var_fn:
      metadata_vars = additional_checkpoint_metadata_var_fn(  # pytype: disable=wrong-arg-types
          state_names,
          metric_names_with_metric_prefix,
          write_metrics_to_checkpoint,
      )
    if metadata_vars:
      metadata_vars_initializer = tf.compat.v1.variables_initializer(
          metadata_vars,
          'initialize_metadata_vars',
      )
      # Override the tff_type_signature portion of the checkpoint metadata to
      # contain the server state type to match the behavior of the MRF pipeline.
      with tf.control_dependencies([metadata_vars_initializer.name]):
        # TODO(team): Revisit the meaning of the type signature variable.
        checkpoint_type_signature_var = metadata_vars[0]
        assign_tff_signature_op = checkpoint_type_signature_var.assign(
            tff.types.serialize_type(
                daf.type_signature.result[0]
            ).SerializeToString()
        )
        metadata_control_dependencies.append(assign_tff_signature_op.name)

    # Write the output checkpoint containing the updated server state values
    # and potentially additional metadata and metrics values.
    tensors = server_state_values
    tensor_names = state_names
    tensors.extend(metadata_vars)
    tensor_names.extend([tensor_utils.bare_name(v) for v in metadata_vars])
    if write_metrics_to_checkpoint:
      tensors.extend(metric_tensors)
      tensor_names.extend(metric_names_with_metric_prefix)
    if tensors:
      with tf.control_dependencies(metadata_control_dependencies):
        server_state_save_op = tensor_utils.save(
            filename=server_result_server_state_output_filepath_placeholder,
            tensor_names=tensor_names,
            tensors=tensors,
            name='save_server_state_tensors',
        )
      server_result_target_nodes.append(server_state_save_op.name)

  # Create the TensorflowSpec and IORouter messages for the post-aggregation
  # (server_result) step.
  tensorflow_spec_result = plan_pb2.TensorflowSpec(
      input_tensor_specs=[
          proto_helpers.make_tensor_spec_from_tensor(t).experimental_as_proto()
          for t in server_result_input_tensors
      ],
      output_tensor_specs=server_result_output_tensors,
      target_node_names=server_result_target_nodes,
  )
  server_result_io_router = plan_pb2.ServerResultIORouter(
      result_intermediate_state_input_filepath_tensor_name=server_result_intermediate_state_input_filepath_placeholder.name,
      result_aggregate_result_input_filepath_tensor_name=server_result_aggregate_result_input_filepath_placeholder.name,
      result_server_state_output_filepath_tensor_name=server_result_server_state_output_filepath_placeholder.name,
  )

  server_phase_v2 = plan_pb2.ServerPhaseV2(
      aggregations=aggregations,
      tensorflow_spec_result=tensorflow_spec_result,
      result_router=server_result_io_router,
  )

  # Add the TensorflowSpec and IORouter messages for the pre-broadcast
  # (server_prepare) step, if needed.
  if run_prepare_logic:
    server_phase_v2.tensorflow_spec_prepare.CopyFrom(
        plan_pb2.TensorflowSpec(
            input_tensor_specs=[
                proto_helpers.make_tensor_spec_from_tensor(
                    t
                ).experimental_as_proto()
                for t in server_prepare_input_tensors
            ],
            target_node_names=server_prepare_target_nodes,
        )
    )
    server_phase_v2.prepare_router.CopyFrom(
        plan_pb2.ServerPrepareIORouter(
            prepare_server_state_input_filepath_tensor_name=server_prepare_server_state_input_filepath_placeholder.name,
            prepare_output_filepath_tensor_name=server_prepare_output_filepath_placeholder.name,
            prepare_intermediate_state_output_filepath_tensor_name=server_prepare_intermediate_state_output_filepath_placeholder.name,
        )
    )

  return (
      server_prepare_graph.as_graph_def() if run_prepare_logic else None,
      server_result_graph.as_graph_def(),
      server_phase_v2,
      set(secagg_client_output_tensor_specs),
  )


def _build_server_graph(
    mrf: tff.backends.mapreduce.MapReduceForm,
    broadcast_tff_type: variable_helpers.AllowedTffTypes,
    is_broadcast_empty: bool,
    flattened_bitwidths: list[int],
    flattened_max_inputs: list[int],
    flattened_moduli: list[int],
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
    experimental_client_update_format: checkpoint_type.CheckpointFormatType = checkpoint_type.CheckpointFormatType.TF1_SAVE_SLICES,
) -> tuple[
    tf.compat.v1.GraphDef,
    plan_pb2.CheckpointOp,
    plan_pb2.ServerPhase,
    list[tf.TensorSpec],
]:
  """Builds the `tf.Graph` that will run on the server.

  Args:
    mrf: A `MapReduceForm` object containing the different computations to
      combine into a single server graph.
    broadcast_tff_type: A `federated_language.Type` object that specifies the
      tensors in the model that are broadcasted and aggregated.
    is_broadcast_empty: boolean indicating whether the broadcasted value from
      the server was initially empty.
    flattened_bitwidths: The `bitwidth`s for the tensors that will be aggregated
      using `bitwidth`-based secure summation.
    flattened_max_inputs: The max_input`s for the tensors that will be
      aggregated using `max_input`-based secure summation.
    flattened_moduli: The `modulus`s for the tensors that will be aggregated
      using modular secure summation.
    write_metrics_to_checkpoint: If False, revert to legacy behavior where
      metrics values were handled by post-processing separate from the outputted
      checkpoint. Regardless, they will additionally continue to be written to
      recordio and accumulator checkpoints as defined by the Plan proto.
    additional_checkpoint_metadata_var_fn: An optional method that takes in a
      server state type, a server metrics type, and a boolean determining
      whether to revert to legacy metrics behavior to produce additional
      metadata variables.
    experimental_client_update_format: Determines how the client update will be
      interpreted. The value has to match experimental_checkpoint_write argument
      of the  _build_client_graph_with_tensorflow_spec call.

  Returns:
    A `tuple` containing the following (in order):
      - A server `tf.GraphDef`,
      - A server checkpoint,
      - A server phase proto message, and
      - A list of `tf.TensorSpec`s for the broadcasted values.
  """
  (
      simpleagg_update_type,
      secure_sum_bitwidth_update_type,
      secure_sum_update_type,
      secure_modular_sum_update_type,
  ) = mrf.work.type_signature.result  # pytype: disable=attribute-error
  with tf.Graph().as_default() as server_graph:
    # Creates all server-side variables and savepoints for both the coordinator
    # and the intermediate aggregators.
    # server_state_type will be a SERVER-placed federated type.
    server_state_type, server_metrics_type = mrf.type_signature.result  # pytype: disable=attribute-error
    assert isinstance(
        server_state_type, federated_language.FederatedType
    ), server_state_type
    assert (
        server_state_type.placement is federated_language.SERVER
    ), server_state_type

    def _predicate(type_spec):
      return not isinstance(
          type_spec,
          (federated_language.FederatedType, federated_language.StructType),
      )

    has_server_metrics = federated_language.framework.type_contains(
        server_metrics_type,
        _predicate,
    )
    if isinstance(server_metrics_type, federated_language.TensorType) or (
        isinstance(server_metrics_type, federated_language.FederatedType)
        and isinstance(
            server_metrics_type.member, federated_language.TensorType
        )
    ):
      # Single tensor; must be wrapped inside of a NamedTuple for proper
      # variable initialization.
      server_metrics_type = federated_language.StructType([server_metrics_type])
    (
        server_state_vars,
        server_metrics_vars,
        metadata_vars,
        server_savepoint,
    ) = checkpoint_utils.create_server_checkpoint_vars_and_savepoint(
        server_state_type=server_state_type,
        server_metrics_type=server_metrics_type,
        write_metrics_to_checkpoint=write_metrics_to_checkpoint,
        additional_checkpoint_metadata_var_fn=(
            additional_checkpoint_metadata_var_fn
        ),
    )

    # TODO(team): Switch to `tf.save()` in lieu of savers to avoid the
    # need to create client variables on the server.
    client_vars_on_server, write_client = (
        checkpoint_utils.create_state_vars_and_savepoint(
            broadcast_tff_type, 'client'
        )
    )

    secure_sum_update_types = [
        secure_sum_bitwidth_update_type,
        secure_sum_update_type,
        secure_modular_sum_update_type,
    ]
    combined_intermediate_update_type = federated_language.StructType(
        [mrf.zero.type_signature.result] + secure_sum_update_types
    )

    combined_intermediate_update_vars, write_intermediate_update = (
        checkpoint_utils.create_state_vars_and_savepoint(
            combined_intermediate_update_type, 'intermediate_update'
        )
    )

    num_simpleagg_vars = len(
        combined_intermediate_update_vars
    ) - federated_language.framework.type_count(
        federated_language.to_type(secure_sum_update_types),
        lambda x: not isinstance(x, federated_language.StructType),
    )
    intermediate_update_vars = combined_intermediate_update_vars[
        :num_simpleagg_vars
    ]
    secagg_intermediate_update_vars = combined_intermediate_update_vars[
        num_simpleagg_vars:
    ]

    read_secagg_update = _read_secagg_update_from_sidechannel_into_vars(
        secagg_intermediate_update_vars=secagg_intermediate_update_vars,
        secure_sum_bitwidth_update_type=secure_sum_bitwidth_update_type,
        bitwidths=flattened_bitwidths,
        secure_sum_update_type=secure_sum_update_type,
        max_inputs=flattened_max_inputs,
        secure_modular_sum_update_type=secure_modular_sum_update_type,
        moduli=flattened_moduli,
    )

    combined_aggregated_update_vars, write_accumulators = (
        checkpoint_utils.create_state_vars_and_savepoint(
            combined_intermediate_update_type, 'aggregated_update'
        )
    )
    aggregated_update_vars = combined_aggregated_update_vars[
        :num_simpleagg_vars
    ]
    secagg_aggregated_update_vars = combined_aggregated_update_vars[
        num_simpleagg_vars:
    ]

    # Throws in the initializer for all state variables, to be executed prior
    # to restoring the savepoint. Run this variable initializer prior to
    # restoring from the savepoint to allow the vars to be overwritten by the
    # savepoint in this case, and so they do not get re-executed after being
    # overwritten. Also include the metrics vars here in case the execution
    # environment wants to read those in.
    server_vars_initializer = tf.compat.v1.variables_initializer(
        server_state_vars + metadata_vars + server_metrics_vars,
        'initialize_server_state_and_non_state_vars',
    )
    server_savepoint.before_restore_op = server_vars_initializer.name

    # In graph mode, TensorFlow does not allow creating a
    # `tf.compat.v1.train.Saver` when there are no variables. As a result,
    # calling `create_state_vars_and_savepoint` below will fail when there are
    # no SimpleAgg variables (e.g., all results are aggregated via SecAgg). In
    # this case, there are no client checkpoints, and hence, no need to populate
    # the `read_update` field.
    if num_simpleagg_vars > 0:
      # Run the initializer for update vars prior to restoring the client update
      update_vars, read_update = (
          checkpoint_utils.create_state_vars_and_savepoint(
              simpleagg_update_type, artifact_constants.UPDATE
          )
      )
      update_vars_initializer = tf.compat.v1.variables_initializer(
          update_vars, 'initialize_update_vars'
      )
      if (
          experimental_client_update_format
          == checkpoint_type.CheckpointFormatType.APPEND_SLICES_MERGE_READ
      ):
        graph = tf.compat.v1.get_default_graph()
        checkpoint_pl = graph.get_tensor_by_name(
            read_update.saver_def.filename_tensor_name
        )
        merge_checkpoint_slices = append_slices.merge_appended_slices(
            checkpoint_pl, 'merge_checkpoint_slices'
        )
        init_merge = tf.group(update_vars_initializer, merge_checkpoint_slices)
        read_update.before_restore_op = init_merge.name
      else:
        read_update.before_restore_op = update_vars_initializer.name
    else:
      # Create a empty list for `update_vars` when there are no SimpleAgg
      # variables, to be compatible with the `accumulated_values` defined below.
      update_vars = []

    # Copy the intermediate aggregator's update saver for use on coordinator.
    read_intermediate_update = plan_pb2.CheckpointOp()
    read_intermediate_update.CopyFrom(write_intermediate_update)

    # Condition all the remaining logic on the variable initializers, since
    # intermediate aggregators are supposed to be stateless (no savepoint, and
    # therefore no `before_restore_op`, either).
    with tf.control_dependencies(
        [
            tf.compat.v1.variables_initializer(
                (intermediate_update_vars + aggregated_update_vars),
                'initialize_accumulator_vars',
            )
        ]
    ):
      # Embeds the `zero` logic and hooks it up to `after_restore_op` of
      # server's checkpointed state (shared between the coordinator and the
      # intermediate aggregators). The zeros get assigned to
      # `intermediate_update_vars` and to the `aggregated_update_vars` at the
      # very beginning, right after restoring from `server_savepoint`.
      zero_values = graph_helpers.import_tensorflow('zero', mrf.zero)
      assign_zero_ops = tf.nest.map_structure(
          lambda variable, value: variable.assign(value),
          intermediate_update_vars,
          zero_values,
      ) + tf.nest.map_structure(
          lambda variable, value: variable.assign(value),
          aggregated_update_vars,
          zero_values,
      )

    # Embeds the `prepare` logic, and hooks it up to `before_save_op` of
    # client state (to be checkpointed and sent to the clients at the
    # beginning of the round by the central coordinator).
    with tf.control_dependencies(
        [
            tf.compat.v1.variables_initializer(
                client_vars_on_server, 'initialize_client_vars_on_server'
            )
        ]
    ):
      # Configure the session token for `write_client` so that the `prepare`
      # operation may be fed the callback ID for the `SaveSlices` op
      # (necessary for plans containing `federated_select`).
      write_client_session_token = tf.compat.v1.placeholder_with_default(
          input='', shape=(), name='write_client_session_token'
      )
      prepared_values = graph_helpers.import_tensorflow(
          'prepare',
          mrf.prepare,
          server_state_vars,
          session_token_tensor=write_client_session_token,
      )
      if is_broadcast_empty:
        # If the broadcast was empty, don't assigning the sample incoming
        # tf.int32 to anything.
        client_state_assign_ops = [tf.no_op()]
      else:
        client_state_assign_ops = tf.nest.map_structure(
            lambda variable, tensor: variable.assign(tensor),
            client_vars_on_server,
            prepared_values,
        )
    write_client.before_save_op = tf.group(*client_state_assign_ops).name
    write_client.session_token_tensor_name = write_client_session_token.name

    # Embeds the `accumulate` logic, and hooks up the assignment of a client
    # update to the intermediate update to `aggregate_into_accumulators_op`.
    accumulated_values = graph_helpers.import_tensorflow(
        'accumulate', mrf.accumulate, (intermediate_update_vars, update_vars)
    )
    intermediate_update_assign_ops = tf.nest.map_structure(
        lambda variable, tensor: variable.assign(tensor),
        intermediate_update_vars,
        accumulated_values,
    )
    aggregate_into_accumulators_op = tf.group(
        *intermediate_update_assign_ops
    ).name

    secagg_aggregated_update_init = tf.compat.v1.variables_initializer(
        secagg_aggregated_update_vars
    )

    # Reset the accumulators in `phase_init_op`, after variable initializers
    # and after restoring from the savepoint.
    phase_init_op = tf.group(
        *(assign_zero_ops + [secagg_aggregated_update_init])
    ).name

    # Embeds the `merge` logic, and hooks up the assignment of an intermediate
    # update to the top-level aggregate update at the coordinator to
    # `intermediate_aggregate_into_accumulators_op`.
    merged_values = graph_helpers.import_tensorflow(
        'merge', mrf.merge, (aggregated_update_vars, intermediate_update_vars)
    )
    aggregated_update_assign_ops = tf.nest.map_structure(
        lambda variable, tensor: variable.assign(tensor),
        aggregated_update_vars,
        merged_values,
    )

    secagg_aggregated_update_ops = _merge_secagg_vars(
        secure_sum_bitwidth_update_type,
        secure_sum_update_type,
        secure_modular_sum_update_type,
        flattened_moduli,
        secagg_aggregated_update_vars,
        secagg_intermediate_update_vars,
    )

    intermediate_aggregate_into_accumulators_op = tf.group(
        *(aggregated_update_assign_ops + secagg_aggregated_update_ops)
    ).name

    # Embeds the `report` and `update` logic, and hooks up the assignments of
    # the results of the final update to the server state and metric vars, to
    # be triggered by `apply_aggregrated_updates_op`.
    simpleagg_reported_values = graph_helpers.import_tensorflow(
        'report', mrf.report, aggregated_update_vars
    )

    # NOTE: In MapReduceForm, the `update` method takes in the simpleagg vars
    # and SecAgg vars as a tuple of two separate lists. However, here, as
    # above, we concatenate the simpleagg values and the secure values into a
    # single list. This mismatch is not a problem because the variables are all
    # flattened either way when traveling in and out of the tensorflow graph.
    combined_update_vars = (
        simpleagg_reported_values + secagg_aggregated_update_vars
    )
    new_server_state_values, server_metrics_values = (
        graph_helpers.import_tensorflow(
            artifact_constants.UPDATE,
            mrf.update,
            (server_state_vars, combined_update_vars),
            split_outputs=True,
        )
    )

    assign_server_state_ops = tf.nest.map_structure(
        lambda variable, tensor: variable.assign(tensor),
        server_state_vars,
        new_server_state_values,
    )
    assign_non_state_ops = tf.nest.map_structure(
        lambda variable, value: variable.assign(value),
        server_metrics_vars,
        server_metrics_values,
    )
    all_assign_ops = assign_server_state_ops + assign_non_state_ops

    checkpoint_state_type_value = (
        tff.types.serialize_type(server_state_type).SerializeToString()
    )

    apply_aggregrated_updates_op = tf.group(*all_assign_ops).name

    # Constructs the metadata for server metrics to be included in the plan.
    server_metrics = [
        proto_helpers.make_metric(v, artifact_constants.SERVER_STATE_VAR_PREFIX)
        for v in server_metrics_vars
    ]

  server_phase_kwargs = collections.OrderedDict(
      phase_init_op=phase_init_op,
      write_client_init=write_client,
      read_aggregated_update=read_secagg_update,
      write_intermediate_update=write_intermediate_update,
      read_intermediate_update=read_intermediate_update,
      intermediate_aggregate_into_accumulators_op=(
          intermediate_aggregate_into_accumulators_op
      ),
      write_accumulators=write_accumulators,
      apply_aggregrated_updates_op=apply_aggregrated_updates_op,
      metrics=server_metrics,
  )

  if num_simpleagg_vars > 0:
    # The `read_update` loads SimpleAgg updates from client checkpoints. The
    # `aggregate_into_accumulators_op` aggregates SimpleAgg data after loading
    # the client updates. No need to populate the two fields if there are no
    # SimpleAgg variables (e.g., if all results are aggregated via SecAgg).
    server_phase_kwargs['read_update'] = read_update
    server_phase_kwargs['aggregate_into_accumulators_op'] = (
        aggregate_into_accumulators_op
    )

  server_phase = plan_pb2.ServerPhase(**server_phase_kwargs)

  broadcasted_tensor_specs = tf.nest.map_structure(
      variable_helpers.tensorspec_from_var, client_vars_on_server
  )
  server_graph_def = server_graph.as_graph_def()

  if write_metrics_to_checkpoint:
    server_graph_def = _redirect_save_saver_to_restore_saver_placeholder(
        server_graph_def
    )

  return (
      server_graph_def,
      server_savepoint,
      server_phase,
      broadcasted_tensor_specs,
  )


def _redirect_save_saver_to_restore_saver_placeholder(
    graph_def: tf.compat.v1.GraphDef,
) -> tf.compat.v1.GraphDef:
  """Updates save Saver's savepoint to point to restore Saver's placeholder.

  NOTE: mutates the GraphDef passed in and returns the mutated GraphDef.

  When we created the server_savepoint Saver when we are outputting all of
  the metrics to the output checkpoint as well, we set different nodes for
  saving and restoring so that we could save state + metrics and restore
  just state. However, the only way to do so was to make two Savers and
  splice them together. This meant that the save and restore operations
  depend on two different placeholders for the checkpoint filename. To
  avoid server changes that pass the same checkpoint name in twice to both
  placeholders, we make a few changes to the server GraphDef so that the
  saving op connects back to the placeholder for the restore operation.
  Once this is done, the original save placeholder node will still exist in
  the graph, but it won't be used by any part of the graph that connects to
  an operation we care about.

  Args:
    graph_def: A `tf.compat.v1.GraphDef` to mutate.

  Returns:
    The mutated `tf.compat.v1.GraphDef` that was passed in as graph_def.
  """
  old_const_node = f'{checkpoint_utils.SAVE_SERVER_SAVEPOINT_NAME}/Const'
  new_const_node = (
      f'{artifact_constants.SERVER_STATE_VAR_PREFIX}_savepoint/Const'
  )
  nodes_to_change = [
      f'{checkpoint_utils.SAVE_SERVER_SAVEPOINT_NAME}/save',
      f'{checkpoint_utils.SAVE_SERVER_SAVEPOINT_NAME}/control_dependency',
      f'{checkpoint_utils.SAVE_SERVER_SAVEPOINT_NAME}/RestoreV2',
  ]
  num_changed_nodes = 0
  for node in graph_def.node:
    if node.name in nodes_to_change:
      input_index = 0
      for input_index, input_node in enumerate(node.input):
        if input_node == old_const_node:
          node.input[input_index] = new_const_node
          break
      assert input_index != len(
          node.input
      ), 'Missed input arg in saver GraphDef rewriting.'
      num_changed_nodes = num_changed_nodes + 1
    if num_changed_nodes == len(nodes_to_change):
      # Once we've changed all of the callsites, we stop.
      return graph_def
  return graph_def


def _add_client_work(
    client_phase: plan_pb2.ClientPhase,
    client_work_comp: federated_language.framework.ConcreteComputation,
    dataspec,
    broadcast_tensor_specs: list[tf.TensorSpec],
) -> graph_helpers.MaybeSplitOutputs:
  """Adds logic to the client graph to execute the client work computation.

  Also populates the `ClientPhase` with the input tensor information that will
  ultimately be needed for running the TF session. Information about the
  client checkpoint input file (if needed) is added to the `ClientPhase`
  `TensorflowSpec` `input_tensor_specs` and the `FederatedComputeIORouter`. Any
  required configuration information for the input dataset is also added to the
  `ClientPhase` `TensorflowSpec`.

  Args:
    client_phase: The `plan_pb2.ClientPhase` message to populate.
    client_work_comp: A `federated_language.framework.ConcreteComputation` that
      represents the TensorFlow logic to run on-device.
    dataspec: Either an instance of `data_spec.DataSpec` or a nested structure
      of these that matches the structure of the first element of the input to
      `client_work_comp`.
    broadcast_tensor_specs: A list of `tf.TensorSpec` containing the name and
      dtype of the variables arriving via the broadcast checkpoint that need to
      be loaded.

  Returns:
    The TensorFlow outputs that result from running the `client_work_comp`.
  """
  broadcast_vals = []
  # Restore the broadcast values, if necessary.
  if broadcast_tensor_specs:
    input_filepath_placeholder = tf.compat.v1.placeholder(
        name=artifact_constants.INPUT_FILEPATH, shape=(), dtype=tf.string
    )
    broadcast_vals = checkpoint_utils.restore_tensors_from_savepoint(
        broadcast_tensor_specs, input_filepath_placeholder
    )
    client_phase.tensorflow_spec.input_tensor_specs.append(
        proto_helpers.make_tensor_spec_from_tensor(
            input_filepath_placeholder
        ).experimental_as_proto()
    )
    client_phase.federated_compute.input_filepath_tensor_name = (
        input_filepath_placeholder.name
    )

  # Add the custom Dataset ops to the graph.
  token_placeholder, data_values, example_selector_placeholders = (
      graph_helpers.embed_data_logic(
          client_work_comp.type_signature.parameter[0],  # pytype: disable=unsupported-operands
          dataspec,
      )
  )
  if token_placeholder is not None:
    client_phase.tensorflow_spec.dataset_token_tensor_name = (
        token_placeholder.name
    )
  if example_selector_placeholders:
    for placeholder in example_selector_placeholders:
      # Generating the default TensorProto will create a TensorProto with an
      # DT_INVALID DType. This identifies that there is a placeholder that is
      # needed. In order to have the Plan proto be completely runnable, the
      # value will need to be filled in with a real TensorProto that matches
      # the shape/type of the expected input.
      client_phase.tensorflow_spec.constant_inputs[placeholder.name].dtype = (
          UNKNOWN_TF_DATATYPE
      )

  # Perform the client_work step.
  client_output_values = graph_helpers.import_tensorflow(
      'work',
      client_work_comp,
      (data_values, broadcast_vals),
      session_token_tensor=token_placeholder,
  )

  return client_output_values


def _save_client_output_tensors(
    client_phase: plan_pb2.ClientPhase,
    simpleagg_tensors: list[tf.Tensor],
    simpleagg_serialization_names: list[str],
    secagg_tensors: list[tf.Tensor],
    secagg_tensor_specs: list[tf.TensorSpec],
    experimental_checkpoint_write: checkpoint_type.CheckpointFormatType,
):
  """Adds logic to the client graph to produce the output ckpt and/or tensors.

  Also populates the `ClientPhase` with the configuration information that will
  ultimately be needed for running the TF session. The `output_tensor_specs` and
  `target_node_names` for the `ClientPhase` `TensorflowSpec` message are set,
  and information about the output filepath (if needed) is added to the
  `TensorflowSpec` message `input_tensor_specs` and `FederatedComputeIORouter`.
  `AggregationConfig` messages are also added to the `FederatedComputeIORouter`
  for the tensors that will be aggregated via secagg.

  Args:
    client_phase: The `plan_pb2.ClientPhase` message to populate.
    simpleagg_tensors: The client output tensors that will be aggregated with
      simpleagg.
    simpleagg_serialization_names: The tensor names to use when saving the
      `simpleagg_tensors` to the client output checkpoint. The ith tensor in
      `simpleagg_tensors` should be saved with the ith name in
      `simpleagg_serialization_names`.
    secagg_tensors: The client output tensors that will be aggregated with
      secagg.
    secagg_tensor_specs: The TensorSpecs to use when generating the secagg
      tensor outputs. The ith TensorSpec in `secagg_tensor_specs` should be used
      for the ith tensor in `secagg_tensors`.
    experimental_checkpoint_write: Determines the format of the final client
      update checkpoint. The value affects required operations and might have
      performance implications.

  Raises:
    SecureAggregationTensorShapeError: If SecAgg tensors do not have all
      dimensions of their shape fully defined.
    ValueError: If any of the arguments are found to be in an unexpected form.
  """
  assert len(simpleagg_tensors) == len(simpleagg_serialization_names)
  assert len(secagg_tensors) == len(secagg_tensor_specs)

  # Save the simpleagg tensors to a checkpoint, if needed.
  if simpleagg_tensors:
    output_filepath_placeholder = tf.compat.v1.placeholder(
        name=artifact_constants.OUTPUT_FILEPATH, dtype=tf.string, shape=()
    )
    client_phase.tensorflow_spec.input_tensor_specs.append(
        proto_helpers.make_tensor_spec_from_tensor(
            output_filepath_placeholder
        ).experimental_as_proto()
    )
    client_phase.federated_compute.output_filepath_tensor_name = (
        output_filepath_placeholder.name
    )

    # Use the requested checkpoint format when saving tensors to the client
    # output checkpoint file. See the `CheckpointFormatType` enum definition
    # for a description of each format.
    if experimental_checkpoint_write in [
        checkpoint_type.CheckpointFormatType.APPEND_SLICES_MERGE_WRITE,
        checkpoint_type.CheckpointFormatType.APPEND_SLICES_MERGE_READ,
    ]:
      delete_op = delete_file.delete_file(output_filepath_placeholder)
      with tf.control_dependencies([delete_op]):
        append_ops = []
        for tensor_name, tensor in zip(
            simpleagg_serialization_names, simpleagg_tensors
        ):
          append_ops.append(
              tensor_utils.save(
                  filename=output_filepath_placeholder,
                  tensor_names=[tensor_name],
                  tensors=[tensor],
                  save_op=append_slices.append_slices,
              )
          )
      if (
          experimental_checkpoint_write
          == checkpoint_type.CheckpointFormatType.APPEND_SLICES_MERGE_WRITE
      ):
        with tf.control_dependencies(append_ops):
          save_op = append_slices.merge_appended_slices(
              filename=output_filepath_placeholder
          )
      else:
        # APPEND_SLICES_MERGE_READ
        save_op = tf.group(*append_ops)

    elif (
        experimental_checkpoint_write
        == checkpoint_type.CheckpointFormatType.TF1_SAVE_SLICES
    ):
      save_op = tensor_utils.save(
          filename=output_filepath_placeholder,
          tensor_names=simpleagg_serialization_names,
          tensors=simpleagg_tensors,
          name=artifact_constants.SAVE_CLIENT_UPDATE_TENSORS,
      )
    else:
      raise NotImplementedError(
          f'Unsupported CheckpointFormatType {experimental_checkpoint_write}.'
      )
    client_phase.tensorflow_spec.target_node_names.append(save_op.name)

  # Output the secagg tensors with the desired names and include aggregation
  # config info in the plan.
  if secagg_tensor_specs:
    # Verify that SecAgg Tensors have all dimensions fully defined.
    for tensor_spec in secagg_tensor_specs:
      if not tf.TensorShape(tensor_spec.shape).is_fully_defined():
        raise SecureAggregationTensorShapeError(
            '`TensorflowSpec.output_tensor_specs` has unknown dimension.'
        )
    secagg_tensors = [
        tf.identity(tensor, name=tensor_utils.bare_name(spec.name))
        for tensor, spec in zip(secagg_tensors, secagg_tensor_specs)
    ]

    for secagg_tensor_spec in secagg_tensor_specs:
      client_phase.tensorflow_spec.output_tensor_specs.append(
          secagg_tensor_spec.experimental_as_proto()
      )
      client_phase.federated_compute.aggregations[
          secagg_tensor_spec.name
      ].CopyFrom(
          plan_pb2.AggregationConfig(
              secure_aggregation=plan_pb2.SecureAggregationConfig()
          )
      )


def _build_client_graph_with_tensorflow_spec(
    client_work_comp: federated_language.framework.ConcreteComputation,
    dataspec,
    broadcasted_tensor_specs: Iterable[tf.TensorSpec],
    is_broadcast_empty: bool,
    *,
    experimental_checkpoint_write: checkpoint_type.CheckpointFormatType = checkpoint_type.CheckpointFormatType.TF1_SAVE_SLICES,
) -> tuple[tf.compat.v1.GraphDef, plan_pb2.ClientPhase]:
  """Builds the client graph and ClientPhase with TensorflowSpec populated.

  This function builds a client phase with tensorflow specs proto.

  Args:
    client_work_comp: A `federated_language.framework.ConcreteComputation` that
      represents the TensorFlow logic run on-device.
    dataspec: Either an instance of `data_spec.DataSpec` or a nested structure
      of these that matches the structure of the first element of the input to
      `client_work_comp`.
    broadcasted_tensor_specs: A list of `tf.TensorSpec` containing the name and
      dtype of the variables arriving via the broadcast checkpoint.
    is_broadcast_empty: A boolean indicating whether the MapReduce form
      initially called for an empty broadcast. In this case the
      broadcasted_tensor_specs will contain a single tf.int32, but it will be
      ignored.
    experimental_checkpoint_write: Determines the format of the final client
      update checkpoint. The value affects required operations and might have
      performance implications.

  Returns:
    A `tuple` of the client TensorFlow GraphDef and the client phase protocol
      message.

  Raises:
    ValueError: If any of the arguments are found to be in an unexpected form.
  """
  if (
      not isinstance(
          client_work_comp.type_signature.parameter,
          federated_language.StructType,
      )
  ) or len(client_work_comp.type_signature.parameter) < 1:
    raise ValueError(
        'client_work_comp.type_signature.parameter should be a '
        '`federated_language.StructType` with length >= 1, but found: {p}.'
        .format(p=client_work_comp.type_signature.parameter)
    )

  if (
      not isinstance(
          client_work_comp.type_signature.result, federated_language.StructType
      )
  ) or len(client_work_comp.type_signature.result) != 4:
    raise ValueError(
        'client_work_comp.type_signature.result should be a '
        '`federated_language.StructType` with length == 4, but found: {r}.'
        .format(r=client_work_comp.type_signature.result)
    )

  client_phase = plan_pb2.ClientPhase()

  with tf.Graph().as_default() as client_graph:
    (
        simpleagg_update_type,
        secure_sum_bitwidth_update_type,
        secure_sum_update_type,
        secure_modular_sum_update_type,
    ) = client_work_comp.type_signature.result

    combined_update_tensors = _add_client_work(
        client_phase,
        client_work_comp,
        dataspec,
        [] if is_broadcast_empty else broadcasted_tensor_specs,
    )

    num_simpleagg_tensors = federated_language.framework.type_count(
        simpleagg_update_type,
        lambda x: not isinstance(x, federated_language.StructType),
    )
    simpleagg_tensors = combined_update_tensors[:num_simpleagg_tensors]
    simpleagg_serialization_names = variable_helpers.variable_names_from_type(
        simpleagg_update_type, name=artifact_constants.UPDATE
    )

    # For tensors aggregated by secagg, we make sure the tensor names are
    # aligned in both client and sever graph by getting the names from the same
    # method.
    secagg_tensors = combined_update_tensors[num_simpleagg_tensors:]
    secagg_tensor_names = []
    secagg_tensor_types = []
    for uri, update_type in [
        (SECURE_SUM_BITWIDTH_URI, secure_sum_bitwidth_update_type),
        (SECURE_SUM_URI, secure_sum_update_type),
        (SECURE_MODULAR_SUM_URI, secure_modular_sum_update_type),
    ]:
      secagg_tensor_names += variable_helpers.get_shared_secagg_tensor_names(
          uri, update_type
      )
      secagg_tensor_types += tff.structure.flatten(update_type)
    secagg_tensor_specs = [
        tf.TensorSpec(name=name, shape=type_spec.shape, dtype=type_spec.dtype)
        for name, type_spec in zip(secagg_tensor_names, secagg_tensor_types)
    ]

    _save_client_output_tensors(
        client_phase,
        simpleagg_tensors,
        simpleagg_serialization_names,
        secagg_tensors,
        secagg_tensor_specs,
        experimental_checkpoint_write,
    )

  return client_graph.as_graph_def(), client_phase


def _build_client_graph_with_tensorflow_spec_from_distribute_aggregate_form(
    client_work: federated_language.framework.ConcreteComputation,
    dataspec,
    grappler_config: Optional[tf.compat.v1.ConfigProto],
    secagg_client_output_tensor_specs: set[tf.TensorSpec],
    experimental_checkpoint_write: checkpoint_type.CheckpointFormatType = checkpoint_type.CheckpointFormatType.TF1_SAVE_SLICES,
) -> tuple[tf.compat.v1.GraphDef, plan_pb2.ClientPhase]:
  """Builds the client graph and ClientPhase from DistributeAggregateForm.

  This method builds the `ClientPhase` for a client computation using TF on-
  device. This means the `ClientPhase` will have a `TensorflowSpec` as opposed
  to a different type of on-device computation (e.g., a SQL query).

  Args:
    client_work: A `federated_language.framework.ConcreteComputation` that
      represents the client portion of the DistributeAggregateForm.
    dataspec: Either an instance of `data_spec.DataSpec` or a nested structure
      of these that matches the structure of the first element of the input to
      `daf.client_work`.
    grappler_config: The config specifying Grappler optimizations for TFF-
      generated graphs.
    secagg_client_output_tensor_specs: A set containining the `TensorSpec`s of
      tensors produced by the client that will be aggregated using secagg.
    experimental_checkpoint_write: Determines the format of the final client
      update checkpoint. The value affects required operations and might have
      performance implications.

  Returns:
    A `tuple` of the client TensorFlow GraphDef and the `plan_pb2.ClientPhase`
      protocol message.

  Raises:
    ValueError: If any of the arguments are found to be in an unexpected form.
  """
  if (
      not isinstance(
          client_work.type_signature.parameter,
          federated_language.StructType,
      )
  ) or len(client_work.type_signature.parameter) < 1:
    raise ValueError(
        'client_work.type_signature.parameter should be a '
        '`federated_language.StructType` with length >= 1, but found: {p}.'
        .format(p=client_work.type_signature.parameter)
    )

  if not isinstance(
      client_work.type_signature.result,
      federated_language.StructType,
  ):
    raise ValueError(
        'client_work.type_signature.result should be a '
        '`federated_language.StructType`, but found: {r}.'.format(
            r=client_work.type_signature.result
        )
    )

  client_phase = plan_pb2.ClientPhase()
  with tf.Graph().as_default() as client_graph:
    # Import the client work computation into the TF graph, including any
    # pre-work for restoring broadcast values.
    broadcast_tensor_specs = []
    if not _is_empty_tff_value(client_work.type_signature.parameter[1]):
      broadcast_type = client_work.type_signature.parameter[1]
      broadcast_vars = variable_helpers.create_vars_for_tff_type(
          broadcast_type, 'client'
      )
      broadcast_tensor_specs = tf.nest.map_structure(
          variable_helpers.tensorspec_from_var, broadcast_vars
      )
    client_work_comp = (
        federated_language.framework.ConcreteComputation.from_building_block(
            tff.backends.mapreduce.consolidate_and_extract_local_processing(
                client_work.to_building_block(), grappler_config
            )
        )
    )
    client_output_values = _add_client_work(
        client_phase, client_work_comp, dataspec, broadcast_tensor_specs
    )

    # Add logic for storing the results of running the client work computation.
    simpleagg_tensors = []
    simpleagg_serialization_names = []
    secagg_tensors = []
    secagg_tensor_specs = []
    # Regardless of whether the tensors are delivered via the output checkpoint
    # or the sidechannel, their names must match what the server_result TF
    # graph is expecting.
    client_output_names = variable_helpers.variable_names_from_type(
        client_work.type_signature.result, artifact_constants.UPDATE
    )
    assert len(client_output_values) == len(client_output_names)
    secagg_client_output_tensor_specs_dict = {}
    for output_tensor_spec in secagg_client_output_tensor_specs:
      secagg_client_output_tensor_specs_dict[
          tensor_utils.bare_name(output_tensor_spec.name)
      ] = output_tensor_spec
    for tensor, name in zip(client_output_values, client_output_names):
      if name in secagg_client_output_tensor_specs_dict:
        # This tensor should be aggregated with secagg.
        secagg_tensors.append(tensor)
        secagg_tensor_specs.append(secagg_client_output_tensor_specs_dict[name])
      else:
        # This tensor should be aggregated with simpleagg.
        simpleagg_tensors.append(tensor)
        simpleagg_serialization_names.append(name)
    _save_client_output_tensors(
        client_phase,
        simpleagg_tensors,
        simpleagg_serialization_names,
        secagg_tensors,
        secagg_tensor_specs,
        experimental_checkpoint_write,
    )

  return client_graph.as_graph_def(), client_phase


def build_client_phase_with_example_query_spec(
    example_query_spec: plan_pb2.ExampleQuerySpec,
    vector_names_expected_by_aggregator: set[str],
) -> plan_pb2.ClientPhase:
  """Builds the ClientPhase with `ExampleQuerySpec` populated.

  Args:
    example_query_spec: Field containing output vector information for client
      example query. The output vector names listed in the spec are expected to
      be consistent with the output names we would produce in the
      `MapReduceForm` client work computation, if we were to build a TF-based
      plan from that `MapReduceForm`.
    vector_names_expected_by_aggregator: The set of vector names used as inputs
      by the aggregator and that are thus expected to be present in the
      ExampleQuery key sets.

  Raises:
    ValueError: If vector_names_expected_by_aggregator contains a name that is
      not present in the ExampleQuery key sets or if there are duplicate names
      found across the ExampleQuery key sets. Note that it is ok if the
      ExampleQuery key sets contain names that aren't present in
      vector_names_expected_by_aggregator.

  Returns:
    A client phase proto message.
  """
  used_names = set()
  io_router = plan_pb2.FederatedExampleQueryIORouter()
  for example_query in example_query_spec.example_queries:
    vector_names = set(example_query.output_vector_specs.keys())
    if not all(
        [name in vector_names_expected_by_aggregator for name in vector_names]
    ):
      raise ValueError(
          'Found unexpected vector names in supplied `example_query_spec`.'
          f' Expected names: {vector_names_expected_by_aggregator}. Found'
          ' unexpected names:'
          f' {vector_names-vector_names_expected_by_aggregator}.'
      )

    if any([name in used_names for name in vector_names]):
      raise ValueError(
          'Duplicate vector names found in supplied `example_query_spec`. '
          f'Duplicates: {vector_names.intersection(used_names)}'
      )

    used_names.update(vector_names)

    for vector_name in vector_names:
      io_router.aggregations[vector_name].CopyFrom(
          plan_pb2.AggregationConfig(
              tf_v1_checkpoint_aggregation=plan_pb2.TFV1CheckpointAggregation()
          )
      )

  if used_names != vector_names_expected_by_aggregator:
    raise ValueError(
        'Not all expected vector names were in supplied `example_query_spec`.'
        f' Expected names: {vector_names_expected_by_aggregator}. Names not'
        ' present in `example_query_spec`:'
        f' {vector_names_expected_by_aggregator-vector_names}'
    )
  return plan_pb2.ClientPhase(
      example_query_spec=example_query_spec, federated_example_query=io_router
  )


def build_aggregations(
    daf: tff.backends.mapreduce.DistributeAggregateForm,
) -> tuple[list[plan_pb2.ServerAggregationConfig], list[tf.TensorSpec]]:
  """Build aggregations for a computation in DistributeAggregateForm.

  Args:
    daf: A TFF computation in DistributeAggregateForm.

  Returns:
   A `tuple` containing the following:
     A list of plan_pb2.ServerAggregationConfig representing the aggregation
     intrinsics to run in the server aggregation step.
     A list of `tf.TensorSpec` giving the secagg tensor specs that should be
     produced by the client for the intrinsic calls.
  """
  # The client_to_server_aggregation computation is guaranteed to conform to
  # a specific structure. It is a lambda computation whose result block contains
  # locals that are exclusively aggregation-type intrinsics.
  aggregations_bb = daf.client_to_server_aggregation.to_building_block()

  if not isinstance(aggregations_bb, tff.framework.Lambda):
    raise ValueError(
        f'Expected a `tff.framework.Lambda`, found {type(aggregations_bb)}.'
    )

  aggregations_result = aggregations_bb.result

  if not isinstance(aggregations_result, tff.framework.Block):
    raise ValueError(
        'Expected a `tff.framework.Block`,'
        f'found {type(aggregations_result)}.'
    )

  # Get lists of the TensorSpecProtos for the inputs and outputs of all
  # intrinsic calls. These lists are formatted such that the ith entry
  # represents the TensorSpecProtos for the ith intrinsic in the aggregation
  # computation. Since intrinsics may have one or more args, the ith entry in
  # the input TensorSpecProto list is itself a list, where the jth entry
  # represents the TensorSpecProtos corresponding to the jth argument of the
  # ith intrinsic.
  grouped_input_tensor_specs = variable_helpers.get_grouped_input_tensor_specs_for_aggregations(
      aggregations_bb,
      artifact_constants.AGGREGATION_INTRINSIC_ARG_SELECTION_INDEX_TO_NAME_DICT,
  )
  grouped_output_tensor_specs = (
      variable_helpers.get_grouped_output_tensor_specs_for_aggregations(
          aggregations_bb
      )
  )
  assert len(grouped_input_tensor_specs) == len(grouped_output_tensor_specs)

  intrinsic_uris = [
      local_value.function.intrinsic_def().uri  # pytype: disable=attribute-error
      for _, local_value in aggregations_result.locals
  ]
  assert len(intrinsic_uris) == len(grouped_output_tensor_specs)

  # Each intrinsic input arg can be a struct or even a nested struct, which
  # requires the intrinsic to be applied independently to each element (e.g. a
  # federated_language.federated_sum call applied to a struct will result in a
  # federated_sum aggregation message for each element of the struct). Note that
  # elements of structs can themselves be multi-dimensional tensors. When an
  # intrinsic call has multiple args with mismatching structure (e.g. a
  # federated_weighted_mean intrinsic applied to a 2D struct value arg and
  # scalar weight arg), some args will need to be "scaled up" via repetition to
  # match the args with the "largest" structure.
  aggregations = []
  secagg_client_output_tensor_specs = []
  for intrinsic_index, (input_tensor_specs, output_tensor_specs) in enumerate(
      zip(grouped_input_tensor_specs, grouped_output_tensor_specs)
  ):
    # Generate the aggregation messages for this intrinsic call.
    aggregations.extend(
        _generate_server_aggregation_configs_for_intrinsic_call(
            intrinsic_uris[intrinsic_index],
            input_tensor_specs,
            output_tensor_specs,
        )
    )

    # Generate the list of secagg tensor names that should be produced by the
    # client for this intrinsic call.
    if intrinsic_uris[intrinsic_index] in set(
        [SECURE_SUM_URI, SECURE_MODULAR_SUM_URI, SECURE_SUM_BITWIDTH_URI]
    ):
      for input_tensor_spec_sublist in input_tensor_specs:
        for input_tensor_spec in input_tensor_spec_sublist:
          if input_tensor_spec.name.startswith(artifact_constants.UPDATE):
            secagg_client_output_tensor_specs.append(
                tf.TensorSpec(
                    name=input_tensor_spec.name + ':0',
                    dtype=input_tensor_spec.dtype,
                    shape=input_tensor_spec.shape,
                )
            )

  return (aggregations, secagg_client_output_tensor_specs)


def build_plan(
    mrf: Optional[tff.backends.mapreduce.MapReduceForm] = None,
    daf: Optional[tff.backends.mapreduce.DistributeAggregateForm] = None,
    dataspec: Optional[data_spec.NestedDataSpec] = None,
    example_query_spec: Optional[plan_pb2.ExampleQuerySpec] = None,
    grappler_config: Optional[tf.compat.v1.ConfigProto] = None,
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
    experimental_client_checkpoint_write: checkpoint_type.CheckpointFormatType = checkpoint_type.CheckpointFormatType.TF1_SAVE_SLICES,
    write_metrics_to_checkpoint: bool = True,
) -> plan_pb2.Plan:
  """Constructs an instance of `plan_pb2.Plan`.

  A `MapReduceForm` and/or `DistributeAggregateForm` instance must be supplied.

  Plans generated by this method are executable, but a number of features have
  yet to be implemented.

  These include:

  - Setting metrics' `stat_name` field based on externally-supplied metadata,
    such as that from the model stampers. Currently, these names are based on
    the names of TensorFlow variables, which in turn are based on the TFF
    type signatures.

  - Populating the client `example_selector` field. Currently not set.

  - Populating client-side `savepoint`. Currently not set.

  - Populating the plan's `tensorflow_config_proto`. Currently not set.

  - Setting a field in the plan that represets a token to drive the custom op
    that iplements the client-side dataset. There is no such field in the plan
    at the time of this writing.

  - Populating plan fields related to secure aggregation and side channels,
    such as the `read_aggregated_update` checkpoint op.

  Args:
    mrf: Optional. An instance of `MapReduceForm`. Exactly one of `mrf` or `daf`
      must be supplied. If provided, the plan will be derived from `mrf`.
    daf: Optional. An instance of `DistributeAggregateForm`. Exactly one of
      `mrf` or `daf` must be supplied. If provided, the plan will be derived
      from `daf`.
    dataspec: If provided, either an instance of `data_spec.DataSpec` or a
      nested structure of these that matches the structure of the first element
      of the input to the client-side processing computation. If not provided
      and `example_query_spec` is also not provided, then placeholders are added
      to the client graph via `embed_data_logic()` and the example selectors
      will need to be passed to the client via the `constant_inputs` part of the
      `TensorflowSpec`. The constant_inputs field needs to be populated outside
      of `build_plan()`. Can only provide one of `dataspec` or
      `example_query_spec`.
    example_query_spec: An instance of `plan_pb2.ExampleQuerySpec`. If provided
      it is assumed a light weight client plan should be constructed. No client
      graph will be included in the produced plan object. Instead the generated
      plan will have an `ExampleQuerySpec` and `FederatedExampleQueryIORouter`.
      Can only supply one of `dataspec` or `example_query_spec`.
    grappler_config: The config specifying Grappler optimizations for TFF-
      generated graphs. Should be provided if daf is provided.
    additional_checkpoint_metadata_var_fn: An optional method that takes in a
      server state type, a server metrics type, and a boolean determining
      whether to revert to legacy metrics behavior to produce additional
      metadata variables.
    experimental_client_checkpoint_write: Determines the style of writing of the
      client checkpoint (client->server communication). The value affects the
      operation used and might have impact on overall task performance.
    write_metrics_to_checkpoint: If False, revert to legacy behavior where
      metrics values were handled by post-processing separate from the outputted
      checkpoint. Regardless, they will additionally continue to be written to
      recordio and accumulator checkpoints as defined by the Plan proto.

  Returns:
    An instance of `plan_pb2.Plan`.

  Raises:
    TypeError: If the arguments are of the wrong types.
    ValueError: If any of the arguments are found to be in an unexpected form or
      do not meet required preconditions.
  """
  if mrf:
    type_checks.check_type(
        mrf, tff.backends.mapreduce.MapReduceForm, name='mrf'
    )
  if daf:
    type_checks.check_type(
        daf, tff.backends.mapreduce.DistributeAggregateForm, name='daf'
    )
  if bool(mrf) == bool(daf):
    raise ValueError('Exactly one of `mrf` or `daf` must be provided.')

  client_plan_type = (
      ClientPlanType.TENSORFLOW
      if example_query_spec is None
      else ClientPlanType.EXAMPLE_QUERY
  )
  if daf and client_plan_type != ClientPlanType.TENSORFLOW:
    raise ValueError(
        'Only TensorFlow plan types are supported when DistributeAggregateForm'
        ' is being used.'
    )

  if example_query_spec is not None:
    if dataspec is not None:
      raise ValueError(
          '`example_query_spec` or `dataspec` cannot both be specified.'
      )

  with federated_language.framework.get_context_stack().install(
      federated_language.framework.RuntimeErrorContext()
  ):
    if daf:
      assert grappler_config
      (
          server_graph_def_prepare,
          server_graph_def_result,
          server_phase_v2,
          secagg_client_output_tensor_specs,
      ) = _build_server_graphs_from_distribute_aggregate_form(
          daf,
          grappler_config,
          write_metrics_to_checkpoint,
          additional_checkpoint_metadata_var_fn,
      )
      client_graph_def, client_phase = (
          _build_client_graph_with_tensorflow_spec_from_distribute_aggregate_form(
              daf.client_work,
              dataspec,
              grappler_config,
              secagg_client_output_tensor_specs,
              experimental_client_checkpoint_write,
          )
      )
      plan = plan_pb2.Plan(
          version=1,
          phase=[
              plan_pb2.Plan.Phase(
                  client_phase=client_phase, server_phase_v2=server_phase_v2
              )
          ],
      )
      plan.client_graph_bytes.Pack(client_graph_def)
      if server_graph_def_prepare:
        plan.server_graph_prepare_bytes.Pack(server_graph_def_prepare)
      plan.server_graph_result_bytes.Pack(server_graph_def_result)
    else:
      assert mrf
      is_broadcast_empty = (
          isinstance(
              mrf.prepare.type_signature.result, federated_language.StructType
          )
          and not mrf.prepare.type_signature.result
      )
      if is_broadcast_empty:
        # This MapReduceForm does not send any server state to clients, however
        # we need something to satisfy current restrictions from the FCP server.
        # Use a placeholder scalar int.
        broadcast_tff_type = federated_language.TensorType(np.int32)
      else:
        broadcast_tff_type = mrf.prepare.type_signature.result

      # Execute the bitwidths TFF computation using the default TFF executor.
      bitwidths, max_inputs, moduli = _compute_secagg_parameters(mrf)
      # Note: the variables below are flat lists, even though
      # `secure_sum_bitwidth_update_type`
      # could potentially represent a large group of nested tensors. In order
      # for each var to line up with the appropriate bitwidth, we must also
      # flatten the list of bitwidths.
      flattened_bitwidths = tff.structure.flatten(bitwidths)
      flattened_max_inputs = tff.structure.flatten(max_inputs)
      flattened_moduli = tff.structure.flatten(moduli)

      (
          server_graph_def,
          server_savepoint,
          server_phase,
          broadcasted_tensor_specs,
      ) = _build_server_graph(
          mrf,
          broadcast_tff_type,  # pytype: disable=wrong-arg-types
          is_broadcast_empty,
          flattened_bitwidths,
          flattened_max_inputs,
          flattened_moduli,
          write_metrics_to_checkpoint,
          additional_checkpoint_metadata_var_fn,
          experimental_client_update_format=experimental_client_checkpoint_write,
      )

      if client_plan_type == ClientPlanType.TENSORFLOW:
        client_graph_def, client_phase = (
            _build_client_graph_with_tensorflow_spec(
                mrf.work,
                dataspec,
                broadcasted_tensor_specs,
                is_broadcast_empty,
                experimental_checkpoint_write=experimental_client_checkpoint_write,
            )
        )
      elif client_plan_type == ClientPlanType.EXAMPLE_QUERY:
        vector_names_expected_by_aggregator = set(
            variable_helpers.variable_names_from_type(
                mrf.work.type_signature.result[0],  # pytype: disable=unsupported-operands
                artifact_constants.UPDATE,
            )
        )
        client_phase = build_client_phase_with_example_query_spec(
            example_query_spec, vector_names_expected_by_aggregator
        )
      else:
        raise ValueError(
            f'Unexpected value for `client_plan_type`: {client_plan_type}'
        )

      plan = plan_pb2.Plan(
          version=1,
          phase=[
              plan_pb2.Plan.Phase(
                  client_phase=client_phase, server_phase=server_phase
              )
          ],
      )
      if client_plan_type == ClientPlanType.TENSORFLOW:
        plan.client_graph_bytes.Pack(client_graph_def)
      plan.server_graph_bytes.Pack(server_graph_def)
      plan.server_savepoint.CopyFrom(server_savepoint)

    return plan


def build_cross_round_aggregation_execution(
    mrf: tff.backends.mapreduce.MapReduceForm,
) -> bytes:
  """Constructs an instance of `plan_pb2.CrossRoundAggregationExecution`.

  Args:
    mrf: An instance of `tff.backends.mapreduce.MapReduceForm`.

  Returns:
    A serialized instance of `plan_pb2.CrossRoundAggregationExecution` for given
    `mrf`.
  """
  type_checks.check_type(mrf, tff.backends.mapreduce.MapReduceForm, name='mrf')

  server_metrics_type = mrf.update.type_signature.result[1]  # pytype: disable=unsupported-operands
  (
      simpleagg_update_type,
      secure_sum_bitwidth_update_type,
      secure_sum_update_type,
      secure_modular_sum_update_type,
  ) = mrf.work.type_signature.result  # pytype: disable=attribute-error
  # We don't ever work directly on `simpleagg_update_type` because client
  # updates are transformed by `accumulate` and `merge` before ever being passed
  # into cross-round aggregation.
  del simpleagg_update_type
  simpleagg_merge_type = mrf.merge.type_signature.result
  flattened_moduli = tff.structure.flatten(mrf.secure_modular_sum_modulus())

  if not server_metrics_type:
    # No metrics to aggregrate; will initialize to no-op.
    server_metrics_type = federated_language.StructType([])
  elif isinstance(server_metrics_type, federated_language.TensorType):
    # Single tensor metric; must be wrapped inside of a NamedTuple for proper
    # variable initialiazation.
    server_metrics_type = federated_language.StructType([server_metrics_type])
  combined_aggregated_update_type = federated_language.StructType([
      simpleagg_merge_type,
      secure_sum_bitwidth_update_type,
      secure_sum_update_type,
      secure_modular_sum_update_type,
  ])

  with tf.Graph().as_default() as cross_round_aggregation_graph:
    server_state_vars = variable_helpers.create_vars_for_tff_type(
        mrf.update.type_signature.parameter[0],  # pytype: disable=unsupported-operands
        artifact_constants.SERVER_STATE_VAR_PREFIX,
    )

    combined_aggregated_update_vars, read_aggregated_update = (
        checkpoint_utils.create_state_vars_and_savepoint(
            combined_aggregated_update_type, 'aggregated_update'
        )
    )

    num_simpleagg_vars = federated_language.framework.type_count(
        simpleagg_merge_type,
        lambda x: not isinstance(x, federated_language.StructType),
    )

    aggregated_update_vars = combined_aggregated_update_vars[
        :num_simpleagg_vars
    ]
    secagg_aggregated_update_vars = combined_aggregated_update_vars[
        num_simpleagg_vars:
    ]

    # Add a new output for metrics_loader `merge` and `report`.
    combined_final_accumulator_vars, read_write_final_accumulators = (
        checkpoint_utils.create_state_vars_and_savepoint(
            combined_aggregated_update_type, 'final_accumulators'
        )
    )

    final_accumulator_vars = combined_final_accumulator_vars[
        :num_simpleagg_vars
    ]
    secagg_final_accumulator_vars = combined_final_accumulator_vars[
        num_simpleagg_vars:
    ]

    var_init_op = tf.compat.v1.initializers.variables(
        server_state_vars
        + combined_aggregated_update_vars
        + combined_final_accumulator_vars
    )

    # Embeds the MapReduce form `merge` logic.
    merged_values = graph_helpers.import_tensorflow(
        'merge', mrf.merge, (final_accumulator_vars, aggregated_update_vars)
    )
    final_accumulator_assign_ops = tf.nest.map_structure(
        lambda variable, tensor: variable.assign(tensor),
        final_accumulator_vars,
        merged_values,
    )

    # SecAgg tensors' aggregation is not provided in the imported TensorFlow,
    # but is instead fixed based on the operator (e.g. `assign_add` for
    # variables passed into `secure_sum`).
    secagg_final_accumulator_ops = _merge_secagg_vars(
        secure_sum_bitwidth_update_type,
        secure_sum_update_type,
        secure_modular_sum_update_type,
        flattened_moduli,
        secagg_final_accumulator_vars,
        secagg_aggregated_update_vars,
    )
    final_accumulator_op = tf.group(
        *(final_accumulator_assign_ops + secagg_final_accumulator_ops)
    ).name

    # Embeds the `report` and `update` logic, and hooks up the assignments of
    # the results of the final update to the server state and metric vars, to
    # be triggered by `apply_aggregrated_updates_op`.
    simpleagg_reported_values = graph_helpers.import_tensorflow(
        'report', mrf.report, final_accumulator_vars
    )
    combined_final_vars = (
        simpleagg_reported_values + secagg_final_accumulator_vars
    )
    (_, server_metric_values) = graph_helpers.import_tensorflow(
        artifact_constants.UPDATE,
        mrf.update,
        (server_state_vars, combined_final_vars),
        split_outputs=True,
    )

    server_metrics_names = variable_helpers.variable_names_from_type(
        server_metrics_type, name=artifact_constants.SERVER_STATE_VAR_PREFIX
    )

    flattened_metrics_types = tff.structure.flatten(server_metrics_type)
    measurements = [
        proto_helpers.make_measurement(v, s, a)
        for v, s, a in zip(
            server_metric_values, server_metrics_names, flattened_metrics_types
        )
    ]

  cross_round_aggregation_execution = plan_pb2.CrossRoundAggregationExecution(
      init_op=var_init_op.name,
      read_aggregated_update=read_aggregated_update,
      merge_op=final_accumulator_op,
      read_write_final_accumulators=read_write_final_accumulators,
      measurements=measurements,
  )

  cross_round_aggregation_execution.cross_round_aggregation_graph_bytes.Pack(
      cross_round_aggregation_graph.as_graph_def()
  )

  return cross_round_aggregation_execution.SerializeToString()
