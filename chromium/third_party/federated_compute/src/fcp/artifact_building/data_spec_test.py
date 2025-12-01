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

import collections

from absl.testing import absltest
import federated_language
import numpy as np
import tensorflow as tf

from fcp.artifact_building import data_spec
from fcp.protos import plan_pb2

_TEST_EXAMPLE_SELECTOR = plan_pb2.ExampleSelector(
    collection_uri='app://fake_uri'
)


class DataSpecTest(absltest.TestCase):

  def test_construction_with_valid_arguments(self):
    preprocessing_fn = lambda ds: ds.batch(10)
    ds = data_spec.DataSpec(_TEST_EXAMPLE_SELECTOR, preprocessing_fn)
    self.assertIs(ds.example_selector_proto, _TEST_EXAMPLE_SELECTOR)
    self.assertIs(ds.preprocessing_fn, preprocessing_fn)

  def test_is_data_spec_or_structure(self):
    preprocessing_fn = lambda ds: ds.batch(10)
    ds = data_spec.DataSpec(_TEST_EXAMPLE_SELECTOR, preprocessing_fn)
    self.assertTrue(data_spec.is_data_spec_or_structure(ds))
    self.assertTrue(data_spec.is_data_spec_or_structure([ds, ds]))
    self.assertTrue(data_spec.is_data_spec_or_structure({'a': ds}))
    self.assertFalse(data_spec.is_data_spec_or_structure(10))
    self.assertFalse(data_spec.is_data_spec_or_structure({'a': 10}))

  def test_type_signature(self):
    def parsing_fn(serialized_example):
      parsing_dict = {
          'key': tf.io.FixedLenFeature(shape=[1], dtype=tf.int64),
      }
      parsed_example = tf.io.parse_example(serialized_example, parsing_dict)
      return collections.OrderedDict([('key', parsed_example['key'])])

    preprocessing_fn = lambda ds: ds.map(parsing_fn)
    ds = data_spec.DataSpec(_TEST_EXAMPLE_SELECTOR, preprocessing_fn)

    expected_type = federated_language.SequenceType(
        federated_language.StructWithPythonType(
            [('key', federated_language.TensorType(np.int64, (1,)))],
            collections.OrderedDict,
        )
    )
    self.assertEqual(ds.type_signature, expected_type)


if __name__ == '__main__':
  tf.compat.v1.enable_v2_behavior()
  absltest.main()
