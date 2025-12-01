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
"""Utilities related to plan protos."""

from typing import TypeVar

import tensorflow as tf
from fcp.artifact_building import tensor_utils
from fcp.protos import plan_pb2


_PlanT = TypeVar('_PlanT', plan_pb2.Plan, plan_pb2.ClientOnlyPlan)


# TODO(team): Remove in favor of save_from_checkpoint_op.
def write_checkpoint(sess, checkpoint_op, checkpoint_filename):
  """Writes from a CheckpointOp, without executing before/after restore ops."""
  if not isinstance(checkpoint_op, plan_pb2.CheckpointOp):
    raise ValueError('A CheckpointOp is required.')
  if (
      checkpoint_op
      and checkpoint_op.saver_def
      and checkpoint_op.saver_def.save_tensor_name
  ):
    sess.run(
        checkpoint_op.saver_def.save_tensor_name,
        {checkpoint_op.saver_def.filename_tensor_name: checkpoint_filename},
    )


# TODO(team): Remove in favor of restore_from_checkpoint_op.
def read_checkpoint(sess, checkpoint_op, checkpoint_filename):
  """Reads from a CheckpointOp, without executing before/after restore ops."""
  if not isinstance(checkpoint_op, plan_pb2.CheckpointOp):
    raise ValueError('A CheckpointOp is required.')
  if (
      checkpoint_op
      and checkpoint_op.saver_def
      and checkpoint_op.saver_def.restore_op_name
  ):
    sess.run(
        checkpoint_op.saver_def.restore_op_name,
        {checkpoint_op.saver_def.filename_tensor_name: checkpoint_filename},
    )


def convert_graphdef_to_flatbuffer(
    graph: tf.compat.v1.GraphDef,
    spec: plan_pb2.TensorflowSpec,
    guarantee_all_funcs_one_use: bool = False,
):
  """Converts a tf.Graph to a serialized TFLite model FlatBuffer."""

  def create_input(input_tensor):
    return (input_tensor.name, [item.size for item in input_tensor.shape.dim])

  inputs = [(spec.dataset_token_tensor_name, [])]
  for input_tensor in spec.input_tensor_specs:
    inputs.append(create_input(input_tensor))
  converter = tf.compat.v1.lite.TFLiteConverter(
      graph,
      input_tensors=None,
      output_tensors=None,
      input_arrays_with_shape=inputs,
      output_arrays=[item.name for item in spec.output_tensor_specs],
  )

  # pylint: disable=protected-access
  # Sets the control output node names. This is used when converting a tf.Graph
  # with no output tensors.
  converter._control_output_arrays = spec.target_node_names
  # Set this flag to true so that flatbuffer size can be reduced.
  converter._experimental_unfold_large_splat_constant = True
  # Exclude conversion metadata generation to reduce conversion time.
  converter.exclude_conversion_metadata = True
  converter.target_spec.supported_ops = [
      tf.lite.OpsSet.TFLITE_BUILTINS,
      tf.lite.OpsSet.SELECT_TF_OPS,
  ]
  converter._experimental_allow_all_select_tf_ops = True
  converter._experimental_guarantee_all_funcs_one_use = (
      guarantee_all_funcs_one_use
  )
  # Instructs the TF Lite converter to not eliminate Assert ops, since the
  # client code needs this op to verify result correctness.
  converter._experimental_preserve_assert_op = True
  # pylint: enable=protected-access
  converter.experimental_enable_resource_variables = True
  return converter.convert()


def generate_and_add_flat_buffer_to_plan(plan: _PlanT) -> _PlanT:
  """Generates and adds a TFLite model to the specified Plan.

  Note: This method mutates the plan argument.

  Args:
    plan: An input plan_pb2.Plan object.

  Returns:
    The input Plan mutated to include a TFLite model when TFLite conversion
    succeeds, or the Plan without any mutation if TFLite conversion does not
    succeed.

  Raises:
    RuntimeError: if TFLite conversion fails and
      forgive_tflite_conversion_failure is set to False.
  """

  def convert(graph_def, tensorflow_spec, guarantee_all_funcs_one_use=False):
    stateful_partitioned_call_err = (
        "'tf.StatefulPartitionedCall' op is"
        + ' neither a custom op nor a flex op'
    )
    # Pack the TFLite flatbuffer into a BytesValue proto.
    try:
      return convert_graphdef_to_flatbuffer(
          graph_def, tensorflow_spec, guarantee_all_funcs_one_use
      )
    except Exception as e:  # pylint: disable=broad-except
      # Try to handle conversion errors and run converter again.
      if (
          stateful_partitioned_call_err in str(e)
          and not guarantee_all_funcs_one_use
      ):
        return convert(graph_def, tensorflow_spec, True)
      else:
        raise RuntimeError(
            f'Failure during TFLite conversion of the client graph: {str(e)}'
        ) from e

  if isinstance(plan, plan_pb2.Plan):
    client_graph_def = tensor_utils.import_graph_def_from_any(
        plan.client_graph_bytes
    )
    plan.client_tflite_graph_bytes = convert(
        client_graph_def, plan.phase[0].client_phase.tensorflow_spec
    )
    # The TFLite flatbuffer replaces the legacy TF GraphDef, so we clear out the
    # client_graph_bytes field.
    plan.ClearField('client_graph_bytes')
  elif isinstance(plan, plan_pb2.ClientOnlyPlan):
    client_graph_def = tf.compat.v1.GraphDef.FromString(plan.graph)
    plan.tflite_graph = convert(client_graph_def, plan.phase.tensorflow_spec)
    # The TFLite flatbuffer replaces the legacy TF GraphDef, so we clear out the
    # graph field.
    plan.ClearField('graph')
  else:
    raise NotImplementedError(f'Unsupported _PlanT {type(plan)}')
  return plan
