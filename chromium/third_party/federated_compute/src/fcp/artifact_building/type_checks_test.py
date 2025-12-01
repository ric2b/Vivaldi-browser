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
"""Tests for type_checks.py."""

from absl.testing import absltest

import tensorflow as tf

from fcp.artifact_building import type_checks


class TestObject:
  pass


class TestObject2:
  pass


class TypeChecksTest(absltest.TestCase):

  def test_check_callable_succeeds(self):
    type_checks.check_callable(lambda: None)

    def foo():
      pass

    type_checks.check_callable(foo)

    class Bar:

      def __call__(self):
        pass

    type_checks.check_callable(Bar())

  def test_check_callable_failure(self):
    with self.assertRaisesRegex(TypeError, 'Expected argument to be callable'):
      type_checks.check_callable(None)
    with self.assertRaisesRegex(TypeError, 'Expected argument to be callable'):
      type_checks.check_callable(0)
    with self.assertRaisesRegex(TypeError, 'Expected argument to be callable'):
      type_checks.check_callable([])

  def test_check_callable_failure_message_with_name(self):
    with self.assertRaisesRegex(TypeError, r'\bfoo\b'):
      type_checks.check_callable(3, name='foo')

  def test_check_type_succeeds(self):
    with self.subTest('int'):
      type_checks.check_type(3, int)
      type_checks.check_type(3, (int, float))
      type_checks.check_type(3, (int, float, TestObject))

    with self.subTest('custom_class'):
      test_obj = TestObject()
      type_checks.check_type(test_obj, object)  # Also true for parent classes.
      type_checks.check_type(test_obj, TestObject)
      type_checks.check_type(test_obj, (object, TestObject))
      type_checks.check_type(test_obj, (int, TestObject))

  def test_check_type_fails(self):
    with self.subTest('int'):
      with self.assertRaises(TypeError):
        type_checks.check_type(3, float)
      with self.assertRaises(TypeError):
        type_checks.check_type(3, (float, TestObject))

    with self.subTest('custom_class'):
      test_obj = TestObject()
      with self.assertRaises(TypeError):
        type_checks.check_type(test_obj, TestObject2)
      with self.assertRaises(TypeError):
        type_checks.check_type(test_obj, int)

  def test_check_type_failure_message_with_name(self):
    with self.assertRaisesRegex(TypeError, r'\bfoo\b'):
      type_checks.check_type(3, float, name='foo')

  def test_check_dataset(self):
    # Should not raise
    type_checks.check_dataset(tf.data.Dataset.from_tensors([42]))
    type_checks.check_dataset(tf.compat.v1.data.Dataset.from_tensors([42]))
    type_checks.check_dataset(tf.compat.v2.data.Dataset.from_tensors([42]))

    with self.assertRaisesWithLiteralMatch(
        TypeError,
        'Expected argument to be a Dataset; but found an instance of int.',
    ):
      type_checks.check_dataset(1234)

  def test_check_dataset_failure_message_with_name(self):
    with self.assertRaisesRegex(TypeError, r'\bfoo\b'):
      type_checks.check_dataset(3, name='foo')


if __name__ == '__main__':
  absltest.main()
