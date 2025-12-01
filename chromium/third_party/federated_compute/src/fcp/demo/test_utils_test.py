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
"""Tests for test_utils."""

from absl.testing import absltest

import tensorflow as tf

from fcp.demo import test_utils


class TestUtilsTest(absltest.TestCase):

  def test_create_checkpoint(self):
    checkpoint = test_utils.create_checkpoint({
        'int': 3,
        'str': 'test',
        'list': [1, 2, 3],
    })
    self.assertEqual(
        test_utils.read_tensor_from_checkpoint(checkpoint, 'int', tf.int32), 3)
    self.assertEqual(
        test_utils.read_tensor_from_checkpoint(checkpoint, 'str', tf.string),
        b'test')
    self.assertListEqual(
        test_utils.read_tensor_from_checkpoint(checkpoint, 'list',
                                               tf.int32).tolist(), [1, 2, 3])

  def test_read_from_checkpoint_not_found(self):
    checkpoint = test_utils.create_checkpoint({'int': 3})
    with self.assertRaises(Exception):
      test_utils.read_tensor_from_checkpoint(checkpoint, 'str', tf.string)

  def test_read_from_checkpoint_wrong_type(self):
    checkpoint = test_utils.create_checkpoint({'int': 3})
    with self.assertRaises(Exception):
      test_utils.read_tensor_from_checkpoint(checkpoint, 'int', tf.string)


if __name__ == '__main__':
  absltest.main()
