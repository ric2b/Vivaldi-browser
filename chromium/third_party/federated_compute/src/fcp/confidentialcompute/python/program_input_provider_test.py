# Copyright 2025 Google LLC
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
import shutil

from absl import flags
from absl.testing import absltest
import tensorflow as tf
import tensorflow_federated as tff

from fcp.confidentialcompute.python import program_input_provider

FLAGS = flags.FLAGS

_CLIENT_IDS = ['a', 'b', 'c']
_TEST_CLIENT_DATA_DIRECTORY = 'test_dir'


def build_linear_regression_keras_functional_model(feature_dims=2):
  """Build a linear regression `tf.keras.Model` using the functional API."""
  a = tf.keras.layers.Input(shape=(feature_dims,), dtype=tf.float32)
  b = tf.keras.layers.Dense(
      units=1,
      use_bias=True,
      kernel_initializer='zeros',
      bias_initializer='zeros',
      activation=None,
  )(a)
  return tf.keras.Model(inputs=a, outputs=b)


class ProgramInputProviderTest(absltest.TestCase):

  def test_init_succeeds(self):
    input_provider = program_input_provider.ProgramInputProvider(
        _CLIENT_IDS,
        _TEST_CLIENT_DATA_DIRECTORY,
        {},
    )
    self.assertEqual(input_provider.client_ids, _CLIENT_IDS)
    self.assertEqual(
        input_provider.client_data_directory, _TEST_CLIENT_DATA_DIRECTORY
    )

  def test_get_model(self):
    keras_model = build_linear_regression_keras_functional_model()
    dataset = tf.data.Dataset.from_tensor_slices((
        [[1.0, 2.0], [3.0, 4.0]],
        [[5.0], [6.0]],
    )).batch(2)
    functional_model = tff.learning.models.functional_model_from_keras(
        keras_model,
        loss_fn=tf.keras.losses.MeanSquaredError(),
        input_spec=dataset.element_spec,
    )
    temp_dir = FLAGS.test_tmpdir
    saved_model_path = os.path.join(temp_dir, 'saved_model')
    tff.learning.models.save_functional_model(
        functional_model, saved_model_path
    )
    zip_file_path = os.path.join(temp_dir, 'model1')
    shutil.make_archive(zip_file_path, 'zip', saved_model_path)
    input_provider = program_input_provider.ProgramInputProvider(
        _CLIENT_IDS,
        _TEST_CLIENT_DATA_DIRECTORY,
        {'model1': zip_file_path + '.zip'},
    )
    model = input_provider.get_model('model1')
    self.assertIsInstance(model, tff.learning.models.FunctionalModel)


if __name__ == '__main__':
  absltest.main()
