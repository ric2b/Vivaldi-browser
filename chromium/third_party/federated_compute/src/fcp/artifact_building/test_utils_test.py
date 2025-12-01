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
"""Tests for test_utils.py."""

from absl.testing import absltest

import tensorflow as tf

from fcp.artifact_building import checkpoint_utils
from fcp.artifact_building import test_utils
from fcp.protos import plan_pb2


class TestUtilsTest(absltest.TestCase):

  def test_set_savepoint(self):
    checkpoint_op = plan_pb2.CheckpointOp()
    graph = tf.Graph()
    with graph.as_default():
      v = tf.compat.v1.get_variable('v', initializer=tf.constant(1))
      saver = checkpoint_utils.create_deterministic_saver([v])
      test_utils.set_checkpoint_op(checkpoint_op, saver)
      self.assertTrue(checkpoint_op.HasField('saver_def'))
      self.assertNotIn(':', checkpoint_op.saver_def.save_tensor_name)

  def test_set_savepoint_no_saver(self):
    checkpoint_op = plan_pb2.CheckpointOp()
    test_utils.set_checkpoint_op(checkpoint_op, None)
    self.assertEqual(plan_pb2.CheckpointOp(), checkpoint_op)


if __name__ == '__main__':
  absltest.main()
