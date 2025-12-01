# Copyright 2021 Google LLC
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

import tensorflow as tf

from fcp.protos import plan_pb2
from fcp.tensorflow import test_selector_pb2

import fcp.tensorflow.example_selector_fuser as fuser


class ExampleSelectorFuserTest(tf.test.TestCase):

  def test_example_selector_fuser(self):
    selector = plan_pb2.ExampleSelector(collection_uri='app:/test_collection')
    criteria = test_selector_pb2.TestCriteria(max_examples=10)
    selector.criteria.Pack(criteria)
    resumption_token = test_selector_pb2.ResumptionToken(last_index=25)
    fused_selector_tensor = fuser.example_selector_fuser(
        tf.convert_to_tensor(selector.SerializeToString(), dtype=tf.string),
        tf.convert_to_tensor(
            'type.googleapis.com/fcp.ResumptionToken', dtype=tf.string),
        tf.convert_to_tensor(
            resumption_token.SerializeToString(), dtype=tf.string))

    fused_selector = plan_pb2.ExampleSelector()
    fused_selector.ParseFromString(fused_selector_tensor.numpy())
    assert fused_selector.collection_uri == 'app:/test_collection'
    unpacked_criteria = test_selector_pb2.TestCriteria()
    assert fused_selector.criteria.Unpack(unpacked_criteria)
    assert unpacked_criteria.max_examples == 10

    unpacked_token = test_selector_pb2.ResumptionToken()
    assert fused_selector.resumption_token.Unpack(unpacked_token)
    assert unpacked_token.last_index == 25

  def test_example_selector_fuser_error(self):
    resumption_token = test_selector_pb2.ResumptionToken(last_index=25)
    with self.assertRaises(tf.errors.InvalidArgumentError):
      fuser.example_selector_fuser(
          tf.convert_to_tensor(b'1234', dtype=tf.string),
          tf.convert_to_tensor(
              'type.googleapis.com/fcp.ResumptionToken', dtype=tf.string),
          tf.convert_to_tensor(
              resumption_token.SerializeToString(), dtype=tf.string))


if __name__ == '__main__':
  tf.test.main()
