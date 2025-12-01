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
"""Tests for `make_slices_selector_example_selector` custom op."""

import tensorflow as tf

from fcp.protos import plan_pb2
from fcp.tensorflow import make_slices_selector_example_selector


class MakeSlicesSelectorExampleSelectorTest(tf.test.TestCase):

  def test_returns_serialized_proto(self):
    served_at_id = 'test_served_at_id'
    keys = [1, 3, 5, 20]
    serialized_proto_tensor = make_slices_selector_example_selector.make_slices_selector_example_selector(
        served_at_id, keys)
    self.assertIsInstance(serialized_proto_tensor, tf.Tensor)
    self.assertEqual(serialized_proto_tensor.dtype, tf.string)
    serialized_proto = serialized_proto_tensor.numpy()
    example_selector = plan_pb2.ExampleSelector.FromString(serialized_proto)
    self.assertEqual(example_selector.collection_uri,
                     'internal:/federated_select')
    slices_selector = plan_pb2.SlicesSelector()
    self.assertTrue(example_selector.criteria.Unpack(slices_selector))
    self.assertEqual(slices_selector.served_at_id, served_at_id)
    self.assertEqual(slices_selector.keys, keys)


if __name__ == '__main__':
  tf.test.main()
