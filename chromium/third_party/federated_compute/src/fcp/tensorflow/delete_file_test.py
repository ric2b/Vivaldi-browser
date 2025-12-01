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
"""Tests for the `delete_file` custom op."""

import os

import tensorflow as tf

from fcp.tensorflow import delete_file


class DeleteOpTest(tf.test.TestCase):

  def setup_temp_dir(self) -> tuple[str, str]:
    """Sets up a temporary directory suitable for testing.

    The filesystem consist of directory with one file inside.

    Returns:
      Tuple of directory and checkpoint paths.
    """
    temp_dir = self.create_tempdir().full_path
    temp_file = os.path.join(temp_dir, 'checkpoint.ckp')

    expected_content = 'content'
    tf.io.write_file(temp_file, expected_content)
    read_content = tf.io.read_file(temp_file)
    self.assertEqual(expected_content, read_content)

    self.assertTrue(os.path.isdir(temp_dir))
    self.assertTrue(os.path.exists(temp_file))
    return temp_dir, temp_file

  def test_delete_file_op(self):
    _, temp_file = self.setup_temp_dir()

    delete_file.delete_file(temp_file)
    # Delete one more time to make sure no error when the file doesn't exist.
    delete_file.delete_file(temp_file)
    self.assertFalse(os.path.exists(temp_file))

  def test_delete_file_op_exceptions(self):
    with self.subTest(name='non_string_dtype'):
      with self.assertRaises(TypeError):
        delete_file.delete_file(1.0)
    with self.subTest(name='non_scalar'):
      with self.assertRaisesRegex(tf.errors.InvalidArgumentError,
                                  '.*must be a string scalar.*'):
        _, checkpoint_path = self.setup_temp_dir()
        delete_file.delete_file([checkpoint_path, checkpoint_path])

  def test_delete_file_and_dir_succeeds(self):
    temp_dir, temp_file = self.setup_temp_dir()
    delete_file.delete_file(temp_file)
    self.assertFalse(os.path.exists(temp_file))

    delete_file.delete_dir(temp_dir)
    # Delete dir more time to make sure no error when the dir doesn't exist.
    delete_file.delete_dir(temp_dir)
    self.assertFalse(os.path.isdir(temp_dir))

  def test_delete_non_empty_dir_fails(self):
    temp_dir, temp_file = self.setup_temp_dir()

    delete_file.delete_dir(temp_dir)
    self.assertTrue(os.path.isdir(temp_dir))
    self.assertTrue(os.path.exists(temp_file))

  def test_recursive_delete_non_empty_dir_succeeds(self):
    temp_dir, temp_file = self.setup_temp_dir()

    delete_file.delete_dir(temp_dir, recursively=True)
    self.assertFalse(os.path.isdir(temp_dir))
    self.assertFalse(os.path.exists(temp_file))

  def test_delete_dir_op_exceptions(self):
    with self.subTest(name='non_string_dtype'):
      with self.assertRaises(TypeError):
        delete_file.delete_dir(1.0)
    with self.subTest(name='non_scalar'):
      with self.assertRaisesRegex(
          tf.errors.InvalidArgumentError, '.*must be a string scalar.*'
      ):
        temp_dir, _ = self.setup_temp_dir()
        delete_file.delete_dir([temp_dir, temp_dir])


if __name__ == '__main__':
  tf.test.main()
