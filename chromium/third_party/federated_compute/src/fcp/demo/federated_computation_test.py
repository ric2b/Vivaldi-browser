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

from unittest import mock

from absl.testing import absltest
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from fcp.demo import federated_computation as fc


@tff.tensorflow.computation(tf.int32, tf.int32)
def add_values(x, y):
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
  client_value = federated_language.federated_value(
      1, federated_language.CLIENTS
  )
  aggregated_count = federated_language.federated_sum(client_value)
  metrics = federated_language.federated_value([], federated_language.SERVER)
  return (
      federated_language.federated_map(add_values, (state, aggregated_count)),
      metrics,
  )


@federated_language.federated_computation(
    federated_language.FederatedType(np.int32, federated_language.SERVER),
    federated_language.FederatedType(
        federated_language.SequenceType(np.str_), federated_language.CLIENTS
    ),
)
def count_examples(state, client_data):
  """Example TFF computation that counts client examples."""

  @tff.tensorflow.computation
  def client_work(client_data):
    return client_data.reduce(0, lambda x, _: x + 1)

  client_counts = federated_language.federated_map(client_work, client_data)
  aggregated_count = federated_language.federated_sum(client_counts)
  metrics = federated_language.federated_value([], federated_language.SERVER)
  return (
      federated_language.federated_map(add_values, (state, aggregated_count)),
      metrics,
  )


class FederatedComputationTest(absltest.TestCase):

  def test_invalid_name(self):
    with self.assertRaisesRegex(ValueError, r'name must match ".+"'):
      fc.FederatedComputation(count_clients, name='^invalid^')

  def test_incompatible_computation(self):
    # This function doesn't have the return value structure required for MRF.
    @federated_language.federated_computation(
        federated_language.FederatedType(np.int32, federated_language.SERVER)
    )
    def _identity(value):
      return value

    with self.assertRaises(TypeError):
      fc.FederatedComputation(_identity, name='comp')

  @federated_language.framework.with_context(
      tff.backends.native.create_sync_local_cpp_execution_context
  )
  def test_distribute_aggregate_form(self):
    comp1 = fc.FederatedComputation(count_clients, name='comp1')
    comp2 = fc.FederatedComputation(count_examples, name='comp2')
    self.assertNotEqual(
        comp1.distribute_aggregate_form, comp2.distribute_aggregate_form
    )

    # While we treat the DAF contents as an implementation detail, we can verify
    # the invocation results of the corresponding computation.
    # comp1 should return the number of clients.
    self.assertEqual(
        tff.backends.mapreduce.get_computation_for_distribute_aggregate_form(
            comp1.distribute_aggregate_form
        )(0, [['', '']] * 3),
        (3, ()),
    )
    # comp2 should return the number of examples across all clients.
    self.assertEqual(
        tff.backends.mapreduce.get_computation_for_distribute_aggregate_form(
            comp2.distribute_aggregate_form
        )(0, [['', '']] * 3),
        (6, ()),
    )

  def test_wrapped_computation(self):
    comp = fc.FederatedComputation(count_clients, name='comp')
    self.assertEqual(comp.wrapped_computation, count_clients)

  def test_name(self):
    comp = fc.FederatedComputation(count_clients, name='comp')
    self.assertEqual(comp.name, 'comp')

  def test_type_signature(self):
    comp = fc.FederatedComputation(count_clients, name='comp')
    self.assertEqual(comp.type_signature, count_clients.type_signature)

  def test_call(self):
    comp = fc.FederatedComputation(count_clients, name='comp')
    ctx = mock.create_autospec(
        federated_language.program.FederatedContext, instance=True
    )
    ctx.invoke.return_value = 1234
    with federated_language.framework.get_context_stack().install(ctx):
      self.assertEqual(comp(1, 2, 3, kw1='a', kw2='b'), 1234)
    ctx.invoke.assert_called_once_with(
        comp,
        tff.structure.Struct([(None, 1), (None, 2), (None, 3), ('kw1', 'a'),
                              ('kw2', 'b')]))

  def test_hash(self):
    comp = fc.FederatedComputation(count_clients, name='comp')
    # Equivalent objects should have equal hashes.
    self.assertEqual(
        hash(comp), hash(fc.FederatedComputation(count_clients, name='comp')))
    # Different computations or names should produce different hashes.
    self.assertNotEqual(
        hash(comp), hash(fc.FederatedComputation(count_clients, name='other')))
    self.assertNotEqual(
        hash(comp), hash(fc.FederatedComputation(count_examples, name='comp')))


if __name__ == '__main__':
  absltest.main()
