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
"""End-to-end test running a simple Federated Program."""

import asyncio
import os
import tempfile
import unittest

from absl import flags
from absl.testing import absltest
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from fcp import demo
from fcp.client import client_runner_example_data_pb2
from fcp.protos import plan_pb2
from fcp.protos import population_eligibility_spec_pb2

POPULATION_NAME = 'test/population'
COLLECTION_URI = 'app:/example'

_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)


@federated_language.federated_computation()
def initialize() -> federated_language.Value:
  """Returns the initial state."""
  return federated_language.federated_value(0, federated_language.SERVER)


@tff.tensorflow.computation(np.int32, np.int32)
def _add(x: int, y: int) -> int:
  return x + y


@federated_language.federated_computation(
    federated_language.FederatedType(np.int32, federated_language.SERVER),
    federated_language.FederatedType(
        federated_language.SequenceType(np.str_), federated_language.CLIENTS
    ),
)
def sum_counts(state, client_data):
  """Sums the value of all 'count' features across all clients."""

  @tf.function
  def reduce_counts(s: tf.int32, example: tf.string) -> tf.int32:
    features = {'count': tf.io.FixedLenFeature((), tf.int64)}
    count = tf.io.parse_example(example, features=features)['count']
    return s + tf.cast(count, tf.int32)

  @tff.tensorflow.computation
  def client_work(client_data):
    return client_data.reduce(0, reduce_counts)

  client_counts = federated_language.federated_map(client_work, client_data)
  aggregated_count = federated_language.federated_sum(client_counts)

  updated_state = federated_language.federated_map(
      _add, (state, aggregated_count)
  )
  num_clients = federated_language.federated_sum(
      federated_language.federated_value(1, federated_language.CLIENTS)
  )
  metrics = federated_language.federated_zip((num_clients,))
  return updated_state, metrics


@federated_language.federated_computation(
    federated_language.FederatedType(np.int32, federated_language.SERVER),
    federated_language.FederatedType(
        federated_language.SequenceType(np.str_), federated_language.CLIENTS
    ),
)
def count_clients(state, client_data):
  """Counts the number of clients."""
  del client_data
  num_clients = federated_language.federated_sum(
      federated_language.federated_value(1, federated_language.CLIENTS)
  )
  updated_state = federated_language.federated_map(_add, (state, num_clients))
  metrics = federated_language.federated_zip((
      federated_language.federated_value(0, federated_language.SERVER),
  ))
  return updated_state, metrics


async def program_logic(
    init_fns: list[federated_language.Computation],
    comp_fns: list[federated_language.Computation],
    data_source: federated_language.program.FederatedDataSource,
    total_rounds: int,
    number_of_clients: int,
    release_manager: federated_language.program.ReleaseManager,
) -> None:
  """Initializes and runs a computation, releasing metrics and final state."""
  federated_language.program.check_in_federated_context()
  assert len(init_fns) == len(comp_fns)
  data_iterator = data_source.iterator()
  states = [init() for init in init_fns]
  for rnd in range(total_rounds):
    cohort_config = data_iterator.select(number_of_clients)
    round_awaitables = []
    for i, (state, comp) in enumerate(zip(states, comp_fns)):
      states[i], metrics = comp(state, cohort_config)
      round_awaitables.append(
          release_manager.release(metrics, key=f'{i}/metrics/{rnd}')
      )
    await asyncio.gather(*round_awaitables)
  for i, (state, _) in enumerate(zip(states, comp_fns)):
    # The last round should already complete, so there's no need to await the
    # results in parallel.
    await release_manager.release(state, key=f'{i}/result')


async def run_client(population_name: str, server_url: str, num_rounds: int,
                     collection_uri: str,
                     examples: list[tf.train.Example]) -> int:
  """Runs a client and returns its return code."""
  client_runner = os.path.join(
      flags.FLAGS.test_srcdir,
      'com_google_fcp',
      'fcp',
      'client',
      'client_runner_main')

  example_data = client_runner_example_data_pb2.ClientRunnerExampleData(
      examples_by_collection_uri={
          collection_uri:
              client_runner_example_data_pb2.ClientRunnerExampleData
              .ExampleList(examples=[e.SerializeToString() for e in examples])
      })

  # Unfortunately, since there's no convenient way to tell when the server has
  # actually started serving the computation, we cannot delay starting the
  # client until the server's ready to assign it a task. This isn't an issue in
  # a production setting, where there's a steady stream of clients connecting,
  # but it is a problem in this unit test, where each client only connects to
  # the server a fixed number of times. To work around this, we give the server
  # a little extra time to become ready; this delay doesn't significantly slow
  # down the test since there are many other time-consuming steps.
  await asyncio.sleep(1)

  with tempfile.NamedTemporaryFile() as tmpfile:
    tmpfile.write(example_data.SerializeToString())
    tmpfile.flush()
    subprocess = await asyncio.create_subprocess_exec(
        client_runner,
        f'--population={population_name}',
        f'--server={server_url}',
        f'--example_data_path={tmpfile.name}',
        f'--num_rounds={num_rounds}',
        '--sleep_after_round_secs=1',
    )
    return await subprocess.wait()


def create_examples(counts: list[int]) -> list[tf.train.Example]:
  """Creates a list of tf.train.Example with the provided 'count' features."""
  examples = []
  for count in counts:
    example = tf.train.Example()
    example.features.feature['count'].int64_list.value.append(count)
    examples.append(example)
  return examples


class FederatedProgramTest(absltest.TestCase, unittest.IsolatedAsyncioTestCase):

  async def test_multiple_rounds(self):
    data_source = demo.FederatedDataSource(
        POPULATION_NAME,
        plan_pb2.ExampleSelector(collection_uri=COLLECTION_URI))
    comp = demo.FederatedComputation(sum_counts, name='sum_counts')
    release_manager = federated_language.program.MemoryReleaseManager()
    num_rounds = 2
    client_counts = [
        [0, 3, 5, 1],
        [2, 4],
    ]

    base_context = tff.backends.native.create_sync_local_cpp_execution_context()

    with demo.FederatedContext(
        POPULATION_NAME,
        base_context=base_context) as ctx:
      clients = [
          run_client(POPULATION_NAME, f'http://localhost:{ctx.server_port}',
                     num_rounds, COLLECTION_URI, create_examples(counts))
          for counts in client_counts
      ]
      with federated_language.framework.get_context_stack().install(ctx):
        program = program_logic(
            [initialize],
            [comp],
            data_source,
            num_rounds,
            len(client_counts),
            release_manager,
        )
        return_codes = (await asyncio.gather(program, *clients))[1:]
        # All clients should complete successfully.
        self.assertListEqual(return_codes, [0] * len(client_counts))  # pytype: disable=wrong-arg-types

    self.assertEqual(
        release_manager.values()['0/result'],
        num_rounds * sum([sum(l) for l in client_counts]),
    )
    for i in range(num_rounds):
      self.assertEqual(
          release_manager.values()[f'0/metrics/{i}'],
          (len(client_counts),),
      )

  async def test_multiple_assignment(self):
    data_source = demo.FederatedDataSource(
        POPULATION_NAME,
        plan_pb2.ExampleSelector(collection_uri=COLLECTION_URI),
        task_assignment_mode=_TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE,
    )
    comp1 = demo.FederatedComputation(sum_counts, name='sum_counts')
    comp2 = demo.FederatedComputation(count_clients, name='count_clients')
    release_manager = federated_language.program.MemoryReleaseManager()
    client_counts = [
        [0, 3, 5, 1],
        [2, 4],
    ]

    base_context = tff.backends.native.create_sync_local_cpp_execution_context()

    with demo.FederatedContext(
        POPULATION_NAME,
        base_context=base_context,
    ) as ctx:
      clients = []
      for counts in client_counts:
        clients.append(
            run_client(
                POPULATION_NAME,
                f'http://localhost:{ctx.server_port}',
                1,
                COLLECTION_URI,
                create_examples(counts),
            )
        )
      with federated_language.framework.get_context_stack().install(ctx):
        num_rounds = 1
        program = program_logic(
            [initialize, initialize],
            [comp1, comp2],
            data_source,
            num_rounds,
            len(client_counts),
            release_manager,
        )
        return_codes = (await asyncio.gather(program, *clients))[1:]
        # All clients should complete successfully.
        self.assertListEqual(return_codes, [0] * len(client_counts))  # pytype: disable=wrong-arg-types

    # With multiple assignment, clients should have contributed to both
    # computations.
    self.assertEqual(
        release_manager.values()['0/result'],
        sum([sum(l) for l in client_counts]),
    )
    expected_result = len(client_counts)
    self.assertEqual(
        release_manager.values()['1/result'],
        expected_result,
    )


if __name__ == '__main__':
  absltest.main()
