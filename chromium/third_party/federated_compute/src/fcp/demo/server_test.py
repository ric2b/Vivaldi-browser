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
"""Tests for server."""

import asyncio
import gzip
import http
import http.client
import os
import threading
import unittest
from unittest import mock
import urllib.parse
import urllib.request

from absl import flags
from absl import logging
from absl.testing import absltest
import tensorflow as tf

from google.longrunning import operations_pb2
from fcp.demo import plan_utils
from fcp.demo import server
from fcp.demo import test_utils
from fcp.protos import plan_pb2
from fcp.protos import population_eligibility_spec_pb2
from fcp.protos.federatedcompute import task_assignments_pb2
from fcp.tensorflow import external_dataset

_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)

POPULATION_NAME = 'test/population'
CAP_TENSOR_NAME = 'cap'
COUNT_TENSOR_NAME = 'count'
TEST_SLICES = {
    'id1': [b'1-1', b'1-2', b'1-3'],
    'id2': [b'2-1', b'2-2'],
}


def create_plan() -> plan_pb2.Plan:
  """Creates a test plan that counts examples, with a per-client cap."""

  with tf.compat.v1.Graph().as_default() as client_graph:
    dataset_token = tf.compat.v1.placeholder(tf.string, shape=())
    input_filepath = tf.compat.v1.placeholder(tf.string, shape=())
    output_filepath = tf.compat.v1.placeholder(tf.string, shape=())
    ds = external_dataset.ExternalDataset(token=dataset_token, selector=b'')
    cap = tf.raw_ops.Restore(
        file_pattern=input_filepath, tensor_name=CAP_TENSOR_NAME, dt=tf.int32)
    count = ds.take(tf.cast(cap, dtype=tf.int64)).reduce(0, lambda x, _: x + 1)
    target_node = tf.raw_ops.Save(
        filename=output_filepath,
        tensor_names=[COUNT_TENSOR_NAME],
        data=[count])

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

    count = tf.Variable(0, dtype=tf.int32)
    load_initial_count = count.assign(
        tf.raw_ops.Restore(
            file_pattern=server_prepare_server_state_input_filepath_placeholder,
            tensor_name=COUNT_TENSOR_NAME,
            dt=tf.int32,
        ),
        read_value=False,
    )
    with tf.control_dependencies([load_initial_count]):
      save_intermediate_state_op = tf.raw_ops.SaveSlices(
          filename=server_prepare_intermediate_state_output_filepath_placeholder,
          tensor_names=[COUNT_TENSOR_NAME],
          shapes_and_slices=[''],
          data=[count],
      )
    server_prepare_target_nodes.append(save_intermediate_state_op.name)

    contribution_cap = tf.Variable(0, dtype=tf.int32)
    load_contribution_cap = contribution_cap.assign(
        tf.raw_ops.Restore(
            file_pattern=server_prepare_server_state_input_filepath_placeholder,
            tensor_name=CAP_TENSOR_NAME,
            dt=tf.int32,
        ),
        read_value=False,
    )
    with tf.control_dependencies([load_contribution_cap]):
      save_client_checkpoint_op = tf.raw_ops.SaveSlices(
          filename=server_prepare_output_filepath_placeholder,
          tensor_names=[CAP_TENSOR_NAME],
          shapes_and_slices=[''],
          data=[contribution_cap],
      )
    server_prepare_target_nodes.append(save_client_checkpoint_op.name)

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

    updated_count = tf.Variable(0, dtype=tf.int32)
    load_initial_count = updated_count.assign(
        tf.raw_ops.Restore(
            file_pattern=server_result_intermediate_state_input_filepath_placeholder,
            tensor_name=COUNT_TENSOR_NAME,
            dt=tf.int32,
        ),
        read_value=False,
    )
    with tf.control_dependencies([load_initial_count]):
      combine_counts = updated_count.assign_add(
          tf.raw_ops.Restore(
              file_pattern=server_result_aggregate_result_input_filepath_placeholder,
              tensor_name=COUNT_TENSOR_NAME,
              dt=tf.int32,
          )
      )
    with tf.control_dependencies([combine_counts]):
      save_op = tf.raw_ops.SaveSlices(
          filename=server_result_server_state_output_filepath_placeholder,
          tensor_names=[COUNT_TENSOR_NAME],
          shapes_and_slices=[''],
          data=[updated_count],
      )
    server_result_target_nodes.append(save_op.name)

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
  aggregations = [
      plan_pb2.ServerAggregationConfig(
          intrinsic_uri='federated_sum',
          intrinsic_args=[
              plan_pb2.ServerAggregationConfig.IntrinsicArg(
                  input_tensor=tf.TensorSpec(
                      (), tf.int32, COUNT_TENSOR_NAME
                  ).experimental_as_proto()
              )
          ],
          output_tensors=[
              tf.TensorSpec(
                  (), tf.int32, COUNT_TENSOR_NAME
              ).experimental_as_proto()
          ],
      )
  ]

  plan = plan_pb2.Plan(
      phase=[
          plan_pb2.Plan.Phase(
              client_phase=plan_pb2.ClientPhase(
                  tensorflow_spec=plan_pb2.TensorflowSpec(
                      dataset_token_tensor_name=dataset_token.op.name,
                      input_tensor_specs=[
                          tf.TensorSpec.from_tensor(
                              input_filepath
                          ).experimental_as_proto(),
                          tf.TensorSpec.from_tensor(
                              output_filepath
                          ).experimental_as_proto(),
                      ],
                      target_node_names=[target_node.name],
                  ),
                  federated_compute=plan_pb2.FederatedComputeIORouter(
                      input_filepath_tensor_name=input_filepath.op.name,
                      output_filepath_tensor_name=output_filepath.op.name,
                  ),
              ),
              server_phase_v2=plan_pb2.ServerPhaseV2(
                  tensorflow_spec_prepare=tensorflow_spec_prepare,
                  prepare_router=server_prepare_io_router,
                  aggregations=aggregations,
                  tensorflow_spec_result=tensorflow_spec_result,
                  result_router=server_result_io_router,
              ),
          )
      ],
      version=1,
  )
  plan.client_graph_bytes.Pack(client_graph.as_graph_def())
  plan.server_graph_prepare_bytes.Pack(server_prepare_graph.as_graph_def())
  plan.server_graph_result_bytes.Pack(server_result_graph.as_graph_def())
  return plan


class ServerTest(absltest.TestCase, unittest.IsolatedAsyncioTestCase):

  def setUp(self):
    super().setUp()
    self.server = server.InProcessServer(  # pytype: disable=wrong-arg-types
        population_name=POPULATION_NAME,
        host='localhost',
        port=0)
    self._server_thread = threading.Thread(target=self.server.serve_forever)
    self._server_thread.start()
    self.conn = http.client.HTTPConnection(
        self.server.server_name, port=self.server.server_port)

  def tearDown(self):
    self.server.shutdown()
    self._server_thread.join()
    self.server.server_close()
    super().tearDown()

  async def wait_for_task(self) -> task_assignments_pb2.TaskAssignment:
    """Polls the server until a task is being served."""
    pop = urllib.parse.quote(POPULATION_NAME, safe='')
    url = f'/v1/populations/{pop}/taskassignments/test:start?%24alt=proto'
    request = task_assignments_pb2.StartTaskAssignmentRequest()
    while True:
      self.conn.request('POST', url, request.SerializeToString())
      http_response = self.conn.getresponse()
      if http_response.status == http.HTTPStatus.OK:
        op = operations_pb2.Operation.FromString(http_response.read())
        response = task_assignments_pb2.StartTaskAssignmentResponse()
        op.response.Unpack(response)
        if response.HasField('task_assignment'):
          logging.info('wait_for_task received assignment to %s',
                       response.task_assignment.task_name)
          return response.task_assignment
      await asyncio.sleep(0.5)

  async def test_run_computation(self):
    initial_count = 100
    cap = 10
    examples_per_client = [1, 5, 15]
    checkpoint = test_utils.create_checkpoint({
        CAP_TENSOR_NAME: cap,
        COUNT_TENSOR_NAME: initial_count,
    })
    run_computation_task = asyncio.create_task(
        self.server.run_computation(
            'task/name',
            create_plan(),
            checkpoint,
            _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE,
            len(examples_per_client),
        )
    )

    # Wait for task assignment to return a task.
    wait_task = asyncio.create_task(self.wait_for_task())
    await asyncio.wait([run_computation_task, wait_task],
                       timeout=10,
                       return_when=asyncio.FIRST_COMPLETED)
    self.assertTrue(wait_task.done())
    # `run_computation` should not be done since no clients have reported.
    self.assertFalse(run_computation_task.done())

    client_runner = os.path.join(
        flags.FLAGS.test_srcdir,
        'com_google_fcp',
        'fcp',
        'client',
        'client_runner_main')
    server_url = f'http://{self.server.server_name}:{self.server.server_port}/'
    clients = []
    for num_examples in examples_per_client:
      subprocess = asyncio.create_subprocess_exec(
          client_runner,
          f'--server={server_url}',
          f'--population={POPULATION_NAME}',
          f'--num_empty_examples={num_examples}',
          '--sleep_after_round_secs=0',
      )
      clients.append(asyncio.create_task((await subprocess).wait()))

    # Wait for the computation to complete.
    await asyncio.wait([run_computation_task] + clients, timeout=10)
    self.assertTrue(run_computation_task.done())
    for client in clients:
      self.assertTrue(client.done())
      self.assertEqual(client.result(), 0)

    # Verify the sum in the checkpoint.
    result = test_utils.read_tensor_from_checkpoint(
        run_computation_task.result(), COUNT_TENSOR_NAME, tf.int32)
    self.assertEqual(
        result, initial_count + sum([min(n, cap) for n in examples_per_client]))

  @mock.patch.object(
      plan_utils.Session,
      'slices',
      new=property(lambda unused_self: TEST_SLICES),
  )
  async def test_federated_select(self):
    checkpoint = test_utils.create_checkpoint({
        CAP_TENSOR_NAME: 100,
        COUNT_TENSOR_NAME: 0,
    })
    run_computation_task = asyncio.create_task(
        self.server.run_computation(
            'task/name',
            create_plan(),
            checkpoint,
            _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE,
            1,
        )
    )

    # Wait for task assignment to return a task.
    wait_task = asyncio.create_task(self.wait_for_task())
    await asyncio.wait(
        [run_computation_task, wait_task],
        timeout=10,
        return_when=asyncio.FIRST_COMPLETED,
    )
    self.assertTrue(wait_task.done())
    uri_template = wait_task.result().federated_select_uri_info.uri_template
    self.assertNotEmpty(uri_template)

    # Check the contents of the slices.
    for served_at_id, slices in TEST_SLICES.items():
      for i, slice_data in enumerate(slices):
        with urllib.request.urlopen(
            uri_template.format(served_at_id=served_at_id, key_base10=str(i))
        ) as response:
          self.assertEqual(
              response.getheader('Content-Type'),
              'application/octet-stream+gzip',
          )
          self.assertEqual(gzip.decompress(response.read()), slice_data)


if __name__ == '__main__':
  absltest.main()
