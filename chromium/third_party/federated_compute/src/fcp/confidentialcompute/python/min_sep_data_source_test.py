# Copyright 2025, The TensorFlow Federated Authors.
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

import os

from absl.testing import absltest
from absl.testing import parameterized
from federated_language.proto import computation_pb2
from federated_language.proto import data_type_pb2

from fcp.confidentialcompute.python import min_sep_data_source
from fcp.protos.confidentialcompute import file_info_pb2


_MIN_SEP = 10
_CLIENT_IDS = ['a', 'b', 'c']
_TEST_CLIENT_DATA_DIRECTORY = 'test_dir'
_KEY_NAME = 'key_name'
_COMPUTATION_TYPE = computation_pb2.Type(
    tensor=computation_pb2.TensorType(dtype=data_type_pb2.DataType.DT_INT32)
)


class MinSepDataSourceIteratorTest(parameterized.TestCase):

  def test_init_raises_value_error_with_client_ids_empty(self):
    client_ids = []

    with self.assertRaisesRegex(
        ValueError, 'Expected `client_ids` to not be empty.'
    ):
      min_sep_data_source.MinSepDataSourceIterator(
          _MIN_SEP,
          client_ids,
          _TEST_CLIENT_DATA_DIRECTORY,
          _COMPUTATION_TYPE,
          _KEY_NAME,
      )

  def test_init_raises_value_error_with_invalid_min_sep(self):
    min_sep = 0

    with self.assertRaisesRegex(
        ValueError,
        'Expected `min_sep` to be a positive integer, found `min_sep` of 0.',
    ):
      min_sep_data_source.MinSepDataSourceIterator(
          min_sep,
          _CLIENT_IDS,
          _TEST_CLIENT_DATA_DIRECTORY,
          _COMPUTATION_TYPE,
          _KEY_NAME,
      )

  @parameterized.named_parameters(
      (
          'none',
          None,
          'Expected `k` to be a positive integer, found `k` of None.',
      ),
      (
          'negative',
          -1,
          'Expected `k` to be a positive integer, found `k` of -1.',
      ),
      (
          'too_small',
          20,
          'Requested more than the number of eligible clients.',
      ),
  )
  def test_select_raises_value_error_with_k(self, k, expected_error_message):
    num_clients = 100
    client_ids = [str(i) for i in range(num_clients)]
    iterator = min_sep_data_source.MinSepDataSourceIterator(
        _MIN_SEP,
        client_ids,
        _TEST_CLIENT_DATA_DIRECTORY,
        _COMPUTATION_TYPE,
        _KEY_NAME,
    )

    with self.assertRaisesRegex(ValueError, expected_error_message):
      iterator.select(k)

  def test_select_returns_client_ids(self):
    num_clients = 1000
    client_ids = [str(i) for i in range(num_clients)]

    # Create an iterator that will have 100 eligible clients per round.
    # (1000 clients that are eligible to participate every 10th round)
    iterator = min_sep_data_source.MinSepDataSourceIterator(
        _MIN_SEP,
        client_ids,
        _TEST_CLIENT_DATA_DIRECTORY,
        _COMPUTATION_TYPE,
        _KEY_NAME,
    )

    # Call `select` 1000 times with `k=10`. Each client should be selected for
    # an average of 10 rounds (although each client will have been eligible for
    # 100 rounds).
    # (1000 rounds * 10 clients per round / 1000 clients = 10)
    client_id_to_round_indices = {}
    num_rounds = 1000
    k = 10
    for round_index in range(num_rounds):
      data_for_round = iterator.select(k)

      # Check that the number of client ids returned is equal to `k`.
      self.assertLen(data_for_round, k)

      # Track which rounds clients are chosen to participate in.
      for data_computation in data_for_round:
        self.assertIsInstance(data_computation, computation_pb2.Computation)
        self.assertEqual(data_computation.type, _COMPUTATION_TYPE)
        self.assertEqual(data_computation.WhichOneof('computation'), 'data')
        unpacked_content = file_info_pb2.FileInfo()
        data_computation.data.content.Unpack(unpacked_content)

        self.assertEqual(
            os.path.dirname(unpacked_content.uri), _TEST_CLIENT_DATA_DIRECTORY
        )
        client_id_to_round_indices.setdefault(
            os.path.basename(unpacked_content.uri), []
        ).append(round_index)
        self.assertEqual(unpacked_content.key, _KEY_NAME)

    # Verify that the rounds that clients are chosen to participate in are
    # multiples of `_MIN_SEP` apart.
    for client_id, round_indices in client_id_to_round_indices.items():
      self.assertIn(client_id, client_ids)

      if len(round_indices) < 2:
        continue

      # Calculate the expected modulus from the first round index, and check
      # that the modulus is the same across all rounds for which this client was
      # chosen to participate.
      expected_modulus = round_indices[0] % _MIN_SEP
      for round_index in round_indices[1:]:
        self.assertEqual(round_index % _MIN_SEP, expected_modulus)


class MinSepDataSourceTest(absltest.TestCase):

  def test_init_raises_value_error_with_client_ids_empty(self):
    client_ids = []

    with self.assertRaisesRegex(
        ValueError, 'Expected `client_ids` to not be empty.'
    ):
      min_sep_data_source.MinSepDataSource(
          _MIN_SEP,
          client_ids,
          _TEST_CLIENT_DATA_DIRECTORY,
          _COMPUTATION_TYPE,
          _KEY_NAME,
      )

  def test_init_raises_value_error_with_invalid_min_sep(self):
    min_sep = 0

    with self.assertRaisesRegex(
        ValueError,
        'Expected `min_sep` to be a positive integer, found `min_sep` of 0.',
    ):
      min_sep_data_source.MinSepDataSource(
          min_sep,
          _CLIENT_IDS,
          _TEST_CLIENT_DATA_DIRECTORY,
          _COMPUTATION_TYPE,
          _KEY_NAME,
      )


if __name__ == '__main__':
  absltest.main()
