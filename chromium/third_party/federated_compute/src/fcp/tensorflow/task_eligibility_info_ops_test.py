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

from fcp.protos import federated_api_pb2
from fcp.tensorflow.task_eligibility_info_ops import create_task_eligibility_info


class TaskEligibilityInfoOpsTest(tf.test.TestCase):

  def test_create_task_eligibility_info_succeeds(self):
    # Run the op and parse its result into the expected proto type.
    actual_serialized_value = create_task_eligibility_info(
        version=555,
        task_names=['foo_task', 'bar_task'],
        task_weights=[123.456, 789.012])
    tf.debugging.assert_scalar(actual_serialized_value)
    tf.debugging.assert_type(actual_serialized_value, tf.string)

    actual_value = federated_api_pb2.TaskEligibilityInfo()
    # Note: the .numpy() call converts the string tensor to a Python string we
    # can parse the proto from.
    actual_value.ParseFromString(actual_serialized_value.numpy())

    # Ensure the resulting proto contains the expected data.
    expected_value = federated_api_pb2.TaskEligibilityInfo(
        version=555,
        task_weights=[
            federated_api_pb2.TaskWeight(task_name='foo_task', weight=123.456),
            federated_api_pb2.TaskWeight(task_name='bar_task', weight=789.012)
        ])
    assert actual_value == expected_value

  def test_create_task_eligibility_info_empty_task_list_succeeds(self):
    """Tests that an empty `task_names` input is allowed & handled correctly."""
    actual_serialized_value = create_task_eligibility_info(
        version=555, task_names=[], task_weights=[])
    actual_value = federated_api_pb2.TaskEligibilityInfo()
    actual_value.ParseFromString(actual_serialized_value.numpy())

    # Ensure the resulting proto contains the expected data.
    expected_value = federated_api_pb2.TaskEligibilityInfo(version=555)
    assert actual_value == expected_value

  def test_create_task_eligibility_info_non_scalar_version_raises_error(self):
    with self.assertRaises(tf.errors.InvalidArgumentError):
      create_task_eligibility_info(
          version=[555], task_names=['foo_task'], task_weights=[123.456])

  def test_create_task_eligibility_info_non_vector_task_names_list_raises_error(
      self):
    with self.assertRaises(tf.errors.InvalidArgumentError):
      create_task_eligibility_info(
          version=555, task_names=[['foo_task']], task_weights=[123.456])

  def test_create_task_eligibility_info_non_vector_task_weights_list_raises_error(
      self):
    with self.assertRaises(tf.errors.InvalidArgumentError):
      create_task_eligibility_info(
          version=555, task_names=['foo_task'], task_weights=[[123.456]])

  def test_create_task_eligibility_info_differing_names_weights_length_raises_error(
      self):
    with self.assertRaises(tf.errors.InvalidArgumentError):
      create_task_eligibility_info(
          version=555, task_names=['foo_task', 'bar_task'], task_weights=[123])

  def test_create_task_eligibility_info_invalid_task_names_type_raises_error(
      self):
    with self.assertRaises(TypeError):
      create_task_eligibility_info(
          version=555, task_names=[111, 222], task_weights=[123.456, 789.012])

  def test_create_task_eligibility_info_invalid_task_weights_type_raises_error(
      self):
    with self.assertRaises(TypeError):
      create_task_eligibility_info(
          version=555,
          task_names=['foo_task', 'bar_task'],
          task_weights=['hello', 'world'])


if __name__ == '__main__':
  tf.test.main()
