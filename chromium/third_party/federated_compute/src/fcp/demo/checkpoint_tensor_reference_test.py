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

import asyncio
import unittest

from absl.testing import absltest
import numpy
import tensorflow as tf
import tensorflow_federated as tff

from fcp.demo import checkpoint_tensor_reference as ctr
from fcp.demo import test_utils

TENSOR_NAME = 'test'
DTYPE = tf.int32
SHAPE = (2, 3)
TEST_VALUE = tf.zeros(SHAPE, DTYPE).numpy()


async def get_test_checkpoint():
  return test_utils.create_checkpoint({TENSOR_NAME: TEST_VALUE})


class CheckpointTensorReferenceTest(absltest.TestCase,
                                    unittest.IsolatedAsyncioTestCase):

  async def test_type_signature(self):
    coro = get_test_checkpoint()
    task = asyncio.create_task(coro)
    ref = ctr.CheckpointTensorReference(TENSOR_NAME, DTYPE, SHAPE, task)
    self.assertEqual(ref.type_signature, tff.tensorflow.to_type((DTYPE, SHAPE)))

  async def test_get_value(self):

    async def get_checkpoint():
      return test_utils.create_checkpoint({TENSOR_NAME: TEST_VALUE})

    coro = get_checkpoint()
    task = asyncio.create_task(coro)
    ref = ctr.CheckpointTensorReference(TENSOR_NAME, DTYPE, SHAPE, task)
    self.assertTrue(numpy.array_equiv(await ref.get_value(), TEST_VALUE))

  async def test_get_value_in_graph_mode(self):
    with tf.compat.v1.Graph().as_default():
      coro = get_test_checkpoint()
      task = asyncio.create_task(coro)
      ref = ctr.CheckpointTensorReference(TENSOR_NAME, DTYPE, SHAPE, task)
      with self.assertRaisesRegex(ValueError,
                                  'get_value is only supported in eager mode'):
        await ref.get_value()

  async def test_get_value_not_found(self):

    async def get_not_found_checkpoint():
      return test_utils.create_checkpoint({'other': TEST_VALUE})

    coro = get_not_found_checkpoint()
    task = asyncio.create_task(coro)
    ref = ctr.CheckpointTensorReference(TENSOR_NAME, DTYPE, SHAPE, task)
    with self.assertRaises(tf.errors.NotFoundError):
      await ref.get_value()

  async def test_get_value_with_invalid_checkpoint(self):

    async def get_invalid_checkpoint():
      return b'invalid'

    coro = get_invalid_checkpoint()
    task = asyncio.create_task(coro)
    ref = ctr.CheckpointTensorReference(TENSOR_NAME, DTYPE, SHAPE, task)
    with self.assertRaises(tf.errors.DataLossError):
      await ref.get_value()


if __name__ == '__main__':
  absltest.main()
