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
"""Tests for plan_utils."""

import tempfile
from typing import Optional

from absl.testing import absltest
import tensorflow as tf

from fcp.demo import plan_utils
from fcp.demo import test_utils
from fcp.protos import plan_pb2

DEFAULT_INITIAL_CHECKPOINT = b'initial'
CHECKPOINT_TENSOR_NAME = 'checkpoint'
AGGREGATED_RESULT_TENSOR_NAME = 'aggregated_result_value'
FINAL_TENSOR_NAME = 'final_value'


def create_plan(log_file: Optional[str] = None) -> plan_pb2.Plan:

  def log_op(name: str) -> tf.Operation:
    """Helper function to log op invocations to a file."""
    if log_file:
      return tf.print(name, output_stream=f'file://{log_file}')
    return tf.raw_ops.NoOp()

  with tf.Graph().as_default() as client_graph:
    tf.constant(0)

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

    client_checkpoint_data = tf.Variable(
        DEFAULT_INITIAL_CHECKPOINT, dtype=tf.string
    )
    restore_op = client_checkpoint_data.assign(
        tf.raw_ops.Restore(
            file_pattern=server_prepare_server_state_input_filepath_placeholder,
            tensor_name=CHECKPOINT_TENSOR_NAME,
            dt=tf.string,
        )
    )
    with tf.control_dependencies([log_op('server_prepare'), restore_op]):
      save_op = tf.io.write_file(
          server_prepare_output_filepath_placeholder,
          client_checkpoint_data.value(),
      )
    server_prepare_target_nodes.append(save_op.name)

  server_result_input_tensors = []
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

    aggregated_result_data = tf.Variable(0, dtype=tf.int32)
    restore_op = aggregated_result_data.assign(
        tf.raw_ops.Restore(
            file_pattern=server_result_aggregate_result_input_filepath_placeholder,
            tensor_name=AGGREGATED_RESULT_TENSOR_NAME,
            dt=tf.int32,
        )
    )
    with tf.control_dependencies([log_op('server_result'), restore_op]):
      save_op = tf.raw_ops.SaveSlices(
          filename=server_result_server_state_output_filepath_placeholder,
          tensor_names=[FINAL_TENSOR_NAME],
          shapes_and_slices=[''],
          data=[aggregated_result_data],
      )
    server_result_target_nodes.append(save_op.name)

  config_proto = tf.compat.v1.ConfigProto(operation_timeout_in_ms=1234)

  tensorflow_spec_prepare = plan_pb2.TensorflowSpec(
      input_tensor_specs=[
          tf.TensorSpec.from_tensor(t).experimental_as_proto()
          for t in server_prepare_input_tensors
      ],
      target_node_names=server_prepare_target_nodes,
  )
  server_prepare_io_router = plan_pb2.ServerPrepareIORouter(
      prepare_server_state_input_filepath_tensor_name=server_prepare_server_state_input_filepath_placeholder.name,
      prepare_output_filepath_tensor_name=server_prepare_output_filepath_placeholder.name,
      prepare_intermediate_state_output_filepath_tensor_name=server_prepare_intermediate_state_output_filepath_placeholder.name,
  )
  tensorflow_spec_result = plan_pb2.TensorflowSpec(
      input_tensor_specs=[
          tf.TensorSpec.from_tensor(t).experimental_as_proto()
          for t in server_result_input_tensors
      ],
      target_node_names=server_result_target_nodes,
  )
  server_result_io_router = plan_pb2.ServerResultIORouter(
      result_intermediate_state_input_filepath_tensor_name=server_result_intermediate_state_input_filepath_placeholder.name,
      result_aggregate_result_input_filepath_tensor_name=server_result_aggregate_result_input_filepath_placeholder.name,
      result_server_state_output_filepath_tensor_name=server_result_server_state_output_filepath_placeholder.name,
  )

  plan = plan_pb2.Plan(
      phase=[
          plan_pb2.Plan.Phase(
              client_phase=plan_pb2.ClientPhase(),
              server_phase_v2=plan_pb2.ServerPhaseV2(
                  tensorflow_spec_prepare=tensorflow_spec_prepare,
                  prepare_router=server_prepare_io_router,
                  aggregations=[],
                  tensorflow_spec_result=tensorflow_spec_result,
                  result_router=server_result_io_router,
              ),
          )
      ],
      client_tflite_graph_bytes=b'tflite-graph',
      version=1,
  )
  plan.client_graph_bytes.Pack(client_graph.as_graph_def())
  plan.server_graph_prepare_bytes.Pack(server_prepare_graph.as_graph_def())
  plan.server_graph_result_bytes.Pack(server_result_graph.as_graph_def())
  plan.tensorflow_config_proto.Pack(config_proto)
  return plan


def create_checkpoint(tensor_value=b'test'):
  """Creates a test initial checkpoint."""
  return test_utils.create_checkpoint({CHECKPOINT_TENSOR_NAME: tensor_value})


class PlanUtilsTest(absltest.TestCase):

  def test_session_without_phase(self):
    plan = create_plan()
    plan.ClearField('phase')
    with self.assertRaises(ValueError):
      plan_utils.Session(plan, create_checkpoint())

  def test_session_without_server_phase_v2(self):
    plan = create_plan()
    plan.phase[0].ClearField('server_phase_v2')
    with self.assertRaises(ValueError):
      plan_utils.Session(plan, create_checkpoint())

  def test_session_with_multiple_phases(self):
    plan = create_plan()
    plan.phase.append(plan.phase[0])
    with self.assertRaises(ValueError):
      plan_utils.Session(plan, create_checkpoint())

  def test_session_client_plan(self):
    plan = create_plan()
    with plan_utils.Session(plan, create_checkpoint()) as session:
      self.assertEqual(
          plan_pb2.ClientOnlyPlan.FromString(session.client_plan),
          plan_pb2.ClientOnlyPlan(
              phase=plan.phase[0].client_phase,
              graph=plan.client_graph_bytes.value,
              tflite_graph=plan.client_tflite_graph_bytes,
              tensorflow_config_proto=plan.tensorflow_config_proto))

  def test_session_client_plan_without_tensorflow_config(self):
    plan = create_plan()
    plan.ClearField('tensorflow_config_proto')
    with plan_utils.Session(plan, create_checkpoint()) as session:
      self.assertEqual(
          plan_pb2.ClientOnlyPlan.FromString(session.client_plan),
          plan_pb2.ClientOnlyPlan(
              phase=plan.phase[0].client_phase,
              graph=plan.client_graph_bytes.value,
              tflite_graph=plan.client_tflite_graph_bytes))

  def test_session_client_plan_without_tflite_graph(self):
    plan = create_plan()
    plan.ClearField('client_tflite_graph_bytes')
    with plan_utils.Session(plan, create_checkpoint()) as session:
      self.assertEqual(
          plan_pb2.ClientOnlyPlan.FromString(session.client_plan),
          plan_pb2.ClientOnlyPlan(
              phase=plan.phase[0].client_phase,
              graph=plan.client_graph_bytes.value,
              tensorflow_config_proto=plan.tensorflow_config_proto))

  def test_session_client_checkpoint(self):
    expected = b'test-client-checkpoint'
    with plan_utils.Session(
        create_plan(),
        test_utils.create_checkpoint({CHECKPOINT_TENSOR_NAME: expected
                                     })) as session:
      self.assertEqual(
          session.client_checkpoint,
          expected,
      )

  def test_session_finalize(self):
    with tempfile.NamedTemporaryFile('r') as tmpfile:
      with plan_utils.Session(create_plan(tmpfile.name),
                              create_checkpoint()) as session:
        checkpoint = session.finalize(
            test_utils.create_checkpoint({AGGREGATED_RESULT_TENSOR_NAME: 3})
        )
      self.assertSequenceEqual(
          tmpfile.read().splitlines(), ['server_prepare', 'server_result']
      )

    result = test_utils.read_tensor_from_checkpoint(checkpoint,
                                                    FINAL_TENSOR_NAME, tf.int32)
    # The value should be propagated from the aggregated result.
    self.assertEqual(result, 3)

  def test_session_client_checkpoint_without_server_prepare(self):
    plan = create_plan()
    plan.ClearField('server_graph_prepare_bytes')
    with plan_utils.Session(
        plan,
        test_utils.create_checkpoint(
            {CHECKPOINT_TENSOR_NAME: b'test-client-checkpoint'}
        ),
    ) as session:
      # Expect an empty client checkpoint.
      self.assertEqual(
          session.client_checkpoint,
          b'',
      )

  def test_session_finalize_without_server_result(self):
    with tempfile.NamedTemporaryFile('r') as tmpfile:
      plan = create_plan(tmpfile.name)
      plan.ClearField('server_graph_result_bytes')
      with plan_utils.Session(plan, create_checkpoint()) as session:
        checkpoint = session.finalize(
            test_utils.create_checkpoint({AGGREGATED_RESULT_TENSOR_NAME: 3})
        )
      self.assertSequenceEqual(tmpfile.read().splitlines(), ['server_prepare'])

    result = test_utils.read_tensor_from_checkpoint(
        checkpoint, AGGREGATED_RESULT_TENSOR_NAME, tf.int32
    )
    # The value should be propagated from the aggregated result.
    self.assertEqual(result, 3)

  def test_session_finalize_without_server_prepare_or_server_result(self):
    with tempfile.NamedTemporaryFile('r') as tmpfile:
      plan = create_plan(tmpfile.name)
      plan.ClearField('server_graph_prepare_bytes')
      plan.ClearField('server_graph_result_bytes')
      with plan_utils.Session(plan, create_checkpoint()) as session:
        checkpoint = session.finalize(
            test_utils.create_checkpoint({AGGREGATED_RESULT_TENSOR_NAME: 3})
        )
      self.assertEmpty(tmpfile.read().splitlines())

    result = test_utils.read_tensor_from_checkpoint(
        checkpoint, AGGREGATED_RESULT_TENSOR_NAME, tf.int32
    )
    # The value should be propagated from the aggregated result.
    self.assertEqual(result, 3)

  def test_session_with_tensorflow_error(self):
    plan = create_plan()
    plan.phase[
        0
    ].server_phase_v2.tensorflow_spec_prepare.target_node_names.append(
        'nonexistent-node'
    )
    with self.assertRaises(ValueError):
      plan_utils.Session(plan, create_checkpoint())


if __name__ == '__main__':
  absltest.main()
