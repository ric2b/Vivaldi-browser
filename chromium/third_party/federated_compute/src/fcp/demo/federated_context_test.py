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

import http
import http.client
import socket
import threading
import unittest
from unittest import mock

from absl.testing import absltest
import attr
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from fcp.artifact_building import artifact_constants
from fcp.artifact_building import federated_compute_plan_builder
from fcp.artifact_building import plan_utils
from fcp.artifact_building import variable_helpers
from fcp.demo import federated_computation
from fcp.demo import federated_context
from fcp.demo import federated_data_source
from fcp.demo import server
from fcp.demo import test_utils
from fcp.protos import plan_pb2

ADDRESS_FAMILY = socket.AddressFamily.AF_INET
POPULATION_NAME = 'test/population'
DATA_SOURCE = federated_data_source.FederatedDataSource(
    POPULATION_NAME, plan_pb2.ExampleSelector(collection_uri='app:/test'))


@tff.tensorflow.computation(tf.int32)
def add_one(x):
  return x + 1


@tff.tensorflow.computation(np.int32, np.int32)
def _add(x: int, y: int) -> int:
  return x + y


@federated_language.federated_computation(
    federated_language.FederatedType(np.int32, federated_language.SERVER),
    federated_language.FederatedType(
        federated_language.SequenceType(np.str_), federated_language.CLIENTS
    ),
)
def count_clients(state, client_data):
  """Example TFF computation that counts clients."""
  del client_data
  num_clients = federated_language.federated_sum(
      federated_language.federated_value(1, federated_language.CLIENTS)
  )
  updated_state = federated_language.federated_map(_add, (state, num_clients))
  non_state = federated_language.federated_value((), federated_language.SERVER)
  return updated_state, non_state


@federated_language.federated_computation(
    federated_language.FederatedType(
        federated_language.StructType([('foo', np.int32), ('bar', np.int32)]),
        federated_language.SERVER,
    ),
    federated_language.FederatedType(
        federated_language.SequenceType(np.str_), federated_language.CLIENTS
    ),
)
def irregular_arrays(state, client_data):
  """Example TFF computation that returns irregular data."""
  del client_data
  num_clients = federated_language.federated_sum(
      federated_language.federated_value(1, federated_language.CLIENTS)
  )
  non_state = federated_language.federated_value(1, federated_language.SERVER)
  updated_non_state = federated_language.federated_map(
      _add, (non_state, num_clients)
  )
  return state, updated_non_state


@attr.s(eq=False, frozen=True, slots=True)
class TestClass:
  """An attrs class."""

  field_one = attr.ib()
  field_two = attr.ib()


@tff.tensorflow.computation
def init():
  return TestClass(field_one=1, field_two=2)


attrs_type = init.type_signature.result


@federated_language.federated_computation(
    federated_language.FederatedType(attrs_type, federated_language.SERVER),
    federated_language.FederatedType(
        federated_language.SequenceType(np.str_), federated_language.CLIENTS
    ),
)
def attrs_computation(state, client_data):
  """Example TFF computation that returns an attrs class."""
  del client_data
  num_clients = federated_language.federated_sum(
      federated_language.federated_value(1, federated_language.CLIENTS)
  )
  non_state = federated_language.federated_value(1, federated_language.SERVER)

  metrics = federated_language.federated_map(_add, (non_state, num_clients))
  return state, metrics


def build_result_checkpoint(state: int) -> bytes:
  """Helper function to build a result checkpoint for `count_clients`."""
  var_names = variable_helpers.variable_names_from_type(
      count_clients.type_signature.result[0],
      name=artifact_constants.SERVER_STATE_VAR_PREFIX)
  return test_utils.create_checkpoint({var_names[0]: state})


class FederatedContextTest(absltest.TestCase, unittest.IsolatedAsyncioTestCase):

  def test_invalid_population_name(self):
    with self.assertRaisesRegex(ValueError, 'population_name must match ".+"'):
      federated_context.FederatedContext(
          '^^invalid^^', address_family=ADDRESS_FAMILY)

  @mock.patch.object(server.InProcessServer, 'shutdown', autospec=True)
  @mock.patch.object(server.InProcessServer, 'serve_forever', autospec=True)
  def test_context_management(self, serve_forever, shutdown):
    started = threading.Event()
    serve_forever.side_effect = lambda *args, **kwargs: started.set()

    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    self.assertFalse(started.is_set())
    shutdown.assert_not_called()
    with ctx:
      self.assertTrue(started.wait(0.5))
      shutdown.assert_not_called()
    shutdown.assert_called_once()

  def test_http(self):
    with federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY) as ctx:
      conn = http.client.HTTPConnection('localhost', port=ctx.server_port)
      conn.request('GET', '/does-not-exist')
      self.assertEqual(conn.getresponse().status, http.HTTPStatus.NOT_FOUND)

  def test_invoke_non_federated_with_base_context(self):
    base_context = tff.backends.native.create_sync_local_cpp_execution_context()
    ctx = federated_context.FederatedContext(
        POPULATION_NAME,
        address_family=ADDRESS_FAMILY,
        base_context=base_context)
    with federated_language.framework.get_context_stack().install(ctx):
      self.assertEqual(add_one(3), 4)

  def test_invoke_non_federated_without_base_context(self):
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    with federated_language.framework.get_context_stack().install(ctx):
      with self.assertRaisesRegex(TypeError,
                                  'computation must be a FederatedComputation'):
        add_one(3)

  def test_invoke_with_invalid_state_type(self):
    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    with federated_language.framework.get_context_stack().install(ctx):
      with self.assertRaisesRegex(
          TypeError, r'arg\[0\] must be a value or structure of values'
      ):
        comp(plan_pb2.Plan(), DATA_SOURCE.iterator().select(1))

  def test_invoke_with_invalid_data_source_type(self):
    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    with federated_language.framework.get_context_stack().install(ctx):
      with self.assertRaisesRegex(
          TypeError, r'arg\[1\] must be the result of '
          r'FederatedDataSource.iterator\(\).select\(\)'):
        comp(0, plan_pb2.Plan())

  async def test_invoke_succeeds_with_structure_state_type(self):
    comp = federated_computation.FederatedComputation(
        irregular_arrays, name='x'
    )
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY
    )
    with federated_language.framework.get_context_stack().install(ctx):
      state = {'foo': (3, 1), 'bar': (4, 5, 6)}
      comp(state, DATA_SOURCE.iterator().select(1))

  async def test_invoke_succeeds_with_attrs_state_type(self):
    comp = federated_computation.FederatedComputation(
        attrs_computation, name='x'
    )
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY
    )
    with federated_language.framework.get_context_stack().install(ctx):
      state = TestClass(field_one=1, field_two=2)
      comp(state, DATA_SOURCE.iterator().select(1))

  def test_invoke_with_mismatched_population_names(self):
    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ds = federated_data_source.FederatedDataSource('other/name',
                                                   DATA_SOURCE.example_selector)
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    with federated_language.framework.get_context_stack().install(ctx):
      with self.assertRaisesRegex(
          ValueError, 'FederatedDataSource and FederatedContext '
          'population_names must match'):
        comp(0, ds.iterator().select(1))

  @mock.patch.object(server.InProcessServer, 'run_computation', autospec=True)
  async def test_invoke_success(self, run_computation):
    run_computation.return_value = build_result_checkpoint(7)

    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    release_manager = federated_language.program.MemoryReleaseManager()
    with federated_language.framework.get_context_stack().install(ctx):
      state, _ = comp(3, DATA_SOURCE.iterator().select(10))
      await release_manager.release(state, key='result')

    self.assertEqual(release_manager.values()['result'], 7)

    run_computation.assert_called_once_with(
        mock.ANY,
        comp.name,
        mock.ANY,
        mock.ANY,
        DATA_SOURCE.task_assignment_mode,
        10,
    )
    plan = run_computation.call_args.args[2]
    self.assertIsInstance(plan, plan_pb2.Plan)
    self.assertNotEmpty(plan.client_tflite_graph_bytes)
    input_var_names = variable_helpers.variable_names_from_type(
        count_clients.type_signature.parameter[0],
        name=artifact_constants.SERVER_STATE_VAR_PREFIX)
    self.assertLen(input_var_names, 1)
    self.assertEqual(
        test_utils.read_tensor_from_checkpoint(
            run_computation.call_args.args[3], input_var_names[0], tf.int32), 3)

  @mock.patch.object(server.InProcessServer, 'run_computation', autospec=True)
  async def test_invoke_with_value_reference(self, run_computation):
    run_computation.side_effect = [
        build_result_checkpoint(1234),
        build_result_checkpoint(5678)
    ]

    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    release_manager = federated_language.program.MemoryReleaseManager()
    with federated_language.framework.get_context_stack().install(ctx):
      state, _ = comp(3, DATA_SOURCE.iterator().select(10))
      state, _ = comp(state, DATA_SOURCE.iterator().select(10))
      await release_manager.release(state, key='result')

    self.assertEqual(release_manager.values()['result'], 5678)

    input_var_names = variable_helpers.variable_names_from_type(
        count_clients.type_signature.parameter[0],
        name=artifact_constants.SERVER_STATE_VAR_PREFIX)
    self.assertLen(input_var_names, 1)
    # The second invocation should be passed the value returned by the first
    # invocation.
    self.assertEqual(run_computation.call_count, 2)
    self.assertEqual(
        test_utils.read_tensor_from_checkpoint(
            run_computation.call_args.args[3], input_var_names[0], tf.int32),
        1234)

  async def test_invoke_without_input_state(self):
    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    with federated_language.framework.get_context_stack().install(ctx):
      with self.assertRaisesRegex(
          TypeError, r'arg\[0\] must be a value or structure of values'
      ):
        comp(None, DATA_SOURCE.iterator().select(1))

  @mock.patch.object(server.InProcessServer, 'run_computation', autospec=True)
  async def test_invoke_with_run_computation_error(self, run_computation):
    run_computation.side_effect = ValueError('message')

    comp = federated_computation.FederatedComputation(count_clients, name='x')
    ctx = federated_context.FederatedContext(
        POPULATION_NAME, address_family=ADDRESS_FAMILY)
    release_manager = federated_language.program.MemoryReleaseManager()
    with federated_language.framework.get_context_stack().install(ctx):
      state, _ = comp(0, DATA_SOURCE.iterator().select(10))
      with self.assertRaisesRegex(ValueError, 'message'):
        await release_manager.release(state, key='result')


class FederatedContextPlanCachingTest(absltest.TestCase,
                                      unittest.IsolatedAsyncioTestCase):

  async def asyncSetUp(self):
    await super().asyncSetUp()

    @federated_language.federated_computation(
        federated_language.FederatedType(np.int32, federated_language.SERVER),
        federated_language.FederatedType(
            federated_language.SequenceType(np.str_), federated_language.CLIENTS
        ),
    )
    def identity(state, client_data):
      del client_data
      return state, federated_language.federated_value(
          (), federated_language.SERVER
      )

    self.count_clients_comp1 = federated_computation.FederatedComputation(
        count_clients, name='count_clients1')
    self.count_clients_comp2 = federated_computation.FederatedComputation(
        count_clients, name='count_clients2')
    self.identity_comp = federated_computation.FederatedComputation(
        identity, name='identity')

    self.data_source1 = federated_data_source.FederatedDataSource(
        POPULATION_NAME, plan_pb2.ExampleSelector(collection_uri='app:/1'))
    self.data_source2 = federated_data_source.FederatedDataSource(
        POPULATION_NAME, plan_pb2.ExampleSelector(collection_uri='app:/2'))

    self.run_computation = self.enter_context(
        mock.patch.object(
            server.InProcessServer, 'run_computation', autospec=True))
    self.run_computation.return_value = build_result_checkpoint(0)
    self.build_plan = self.enter_context(
        mock.patch.object(
            federated_compute_plan_builder, 'build_plan', autospec=True))
    self.build_plan.return_value = plan_pb2.Plan()
    self.generate_and_add_flat_buffer_to_plan = self.enter_context(
        mock.patch.object(
            plan_utils, 'generate_and_add_flat_buffer_to_plan', autospec=True))
    self.generate_and_add_flat_buffer_to_plan.side_effect = lambda plan: plan
    self.enter_context(
        federated_language.framework.get_context_stack().install(
            federated_context.FederatedContext(
                POPULATION_NAME, address_family=ADDRESS_FAMILY
            )
        )
    )
    self.release_manager = federated_language.program.MemoryReleaseManager()

    # Run (and therefore cache) count_clients_comp1 with data_source1.
    await self.release_manager.release(
        self.count_clients_comp1(0, self.data_source1.iterator().select(1)),
        key='result',
    )
    self.build_plan.assert_called_once()
    self.assertIsNone(self.build_plan.call_args.args[0])
    self.assertEqual(
        self.build_plan.call_args.args[1],
        self.count_clients_comp1.distribute_aggregate_form,
    )
    self.assertEqual(
        self.build_plan.call_args.args[2].example_selector_proto,
        self.data_source1.example_selector,
    )
    self.run_computation.assert_called_once()
    self.build_plan.reset_mock()
    self.run_computation.reset_mock()

  async def test_reuse_with_repeat_computation(self):
    await self.release_manager.release(
        self.count_clients_comp1(0, self.data_source1.iterator().select(1)),
        key='result',
    )
    self.build_plan.assert_not_called()
    self.run_computation.assert_called_once()

  async def test_reuse_with_changed_num_clients(self):
    await self.release_manager.release(
        self.count_clients_comp1(0, self.data_source1.iterator().select(10)),
        key='result',
    )
    self.build_plan.assert_not_called()
    self.run_computation.assert_called_once()

  async def test_reuse_with_changed_initial_state(self):
    await self.release_manager.release(
        self.count_clients_comp1(3, self.data_source1.iterator().select(1)),
        key='result',
    )
    self.build_plan.assert_not_called()
    self.run_computation.assert_called_once()

  async def test_reuse_with_equivalent_map_reduce_form(self):
    await self.release_manager.release(
        self.count_clients_comp2(0, self.data_source1.iterator().select(1)),
        key='result',
    )
    self.build_plan.assert_not_called()
    self.run_computation.assert_called_once()

  async def test_rebuild_with_different_computation(self):
    await self.release_manager.release(
        self.identity_comp(0, self.data_source1.iterator().select(1)),
        key='result',
    )
    self.build_plan.assert_called_once()
    self.assertIsNone(self.build_plan.call_args.args[0])
    self.assertEqual(
        self.build_plan.call_args.args[1],
        self.identity_comp.distribute_aggregate_form,
    )
    self.assertEqual(
        self.build_plan.call_args.args[2].example_selector_proto,
        self.data_source1.example_selector,
    )
    self.run_computation.assert_called_once()

  async def test_rebuild_with_different_data_source(self):
    await self.release_manager.release(
        self.count_clients_comp1(0, self.data_source2.iterator().select(1)),
        key='result',
    )
    self.build_plan.assert_called_once()
    self.assertIsNone(self.build_plan.call_args.args[0])
    self.assertEqual(
        self.build_plan.call_args.args[1],
        self.count_clients_comp1.distribute_aggregate_form,
    )
    self.assertEqual(
        self.build_plan.call_args.args[2].example_selector_proto,
        self.data_source2.example_selector,
    )
    self.run_computation.assert_called_once()


if __name__ == '__main__':
  absltest.main()
