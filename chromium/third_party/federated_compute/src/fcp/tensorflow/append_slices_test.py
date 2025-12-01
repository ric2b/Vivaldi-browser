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
"""Tests for the `append_slices` and `merge_appended_slices` custom ops."""

import os
import tensorflow as tf

from fcp.tensorflow import append_slices
from fcp.tensorflow import delete_file


class AppendSlicesTest(tf.test.TestCase):

  def new_tempfile_path(self):
    """Returns a path that can be used to store a new tempfile."""
    return os.path.join(self.create_tempdir(), 'checkpoint.ckp')

  def test_converts_single_element_once_appended_file_to_checkpoint(self):
    checkpoint_path = self.new_tempfile_path()
    tensor_name = 'a'
    tensor = tf.constant(42, dtype=tf.int32)
    append_slices.append_slices(
        filename=checkpoint_path,
        tensor_names=[tensor_name],
        data=[tensor],
        shapes_and_slices=[''])
    append_slices.merge_appended_slices(checkpoint_path)
    restored = tf.raw_ops.RestoreV2(
        prefix=checkpoint_path,
        tensor_names=[tensor_name],
        shape_and_slices=[''],
        dtypes=[tf.int32])
    self.assertEqual(restored[0], 42)

  def test_converts_single_element_twice_appended_file_to_checkpoint(self):
    checkpoint_path = self.new_tempfile_path()
    tensor_names = ['a', 'b']
    tensor_values = [tf.constant(x, dtype=tf.int32) for x in (7, 11)]
    for (tensor_name, tensor_value) in zip(tensor_names, tensor_values):
      append_slices.append_slices(
          filename=checkpoint_path,
          tensor_names=[tensor_name],
          data=[tensor_value],
          shapes_and_slices=[''])
    append_slices.merge_appended_slices(checkpoint_path)
    restored = tf.raw_ops.RestoreV2(
        prefix=checkpoint_path,
        tensor_names=tensor_names,
        shape_and_slices=[''] * 2,
        dtypes=[tf.int32] * 2)
    self.assertEqual(restored[0], 7)
    self.assertEqual(restored[1], 11)

  def test_converts_two_element_once_appended_file_to_checkpoint(self):
    checkpoint_path = self.new_tempfile_path()
    tensors = [('a', 16), ('b', 17)]
    append_slices.append_slices(
        filename=checkpoint_path,
        tensor_names=[name for (name, value) in tensors],
        data=[tf.constant(value, tf.int32) for (name, value) in tensors],
        shapes_and_slices=['' for _ in tensors])
    append_slices.merge_appended_slices(checkpoint_path)
    restored = tf.raw_ops.RestoreV2(
        prefix=checkpoint_path,
        tensor_names=['a', 'b'],
        shape_and_slices=[''] * 2,
        dtypes=[tf.int32] * 2)
    self.assertEqual(restored[0], 16)
    self.assertEqual(restored[1], 17)

  def test_converts_two_element_multi_twice_appended_file_to_checkpoint(self):
    # Note: the interleaved ordering ensures that the resulting merged
    # checkpoint is able to mix together the two input checkpoints properly.
    checkpoint_path = self.new_tempfile_path()
    tensors = [
        [('a', 12), ('c', 55)],
        [('b', 40), ('d', 88)],
    ]
    for tensors_for_checkpoint in tensors:
      append_slices.append_slices(
          filename=checkpoint_path,
          tensor_names=[name for (name, value) in tensors_for_checkpoint],
          data=[
              tf.constant(value, tf.int32)
              for (name, value) in tensors_for_checkpoint
          ],
          shapes_and_slices=['' for _ in tensors_for_checkpoint])
    append_slices.merge_appended_slices(checkpoint_path)
    restored = tf.raw_ops.RestoreV2(
        prefix=checkpoint_path,
        tensor_names=['a', 'b', 'c', 'd'],
        shape_and_slices=[''] * 4,
        dtypes=[tf.int32] * 4)
    self.assertEqual(restored[0], 12)
    self.assertEqual(restored[1], 40)
    self.assertEqual(restored[2], 55)
    self.assertEqual(restored[3], 88)

  def test_converts_nonalphabetical_two_element_multi_twice_appended_file_to_checkpoint(
      self):
    # Note: the interleaved ordering ensures that the resulting merged
    # checkpoint is able to mix together the two input checkpoints properly.
    checkpoint_path = self.new_tempfile_path()
    tensors = [
        [('b', 12), ('a', 55)],
        [('d', 40), ('c', 88)],
    ]
    for tensors_for_checkpoint in tensors:
      append_slices.append_slices(
          filename=checkpoint_path,
          tensor_names=[name for (name, value) in tensors_for_checkpoint],
          data=[
              tf.constant(value, tf.int32)
              for (name, value) in tensors_for_checkpoint
          ],
          shapes_and_slices=['' for _ in tensors_for_checkpoint])
    append_slices.merge_appended_slices(checkpoint_path)
    restored = tf.raw_ops.RestoreV2(
        prefix=checkpoint_path,
        tensor_names=['d', 'c', 'b', 'a'],
        shape_and_slices=[''] * 4,
        dtypes=[tf.int32] * 4)
    self.assertEqual(restored[0], 40)
    self.assertEqual(restored[1], 88)
    self.assertEqual(restored[2], 12)
    self.assertEqual(restored[3], 55)

  def test_merge_missing_checkpoint_file_raises(self):
    checkpoint_path = self.new_tempfile_path()
    with self.assertRaises(tf.errors.NotFoundError):
      append_slices.merge_appended_slices(checkpoint_path)

  def test_duplicate_named_tensor_raises(self):
    checkpoint_path = self.new_tempfile_path()
    tensor_values = [tf.constant(x, dtype=tf.int32) for x in (7, 11)]
    for tensor_value in tensor_values:
      append_slices.append_slices(
          filename=checkpoint_path,
          tensor_names=['a'],
          data=[tensor_value],
          shapes_and_slices=[''])
    with self.assertRaisesRegex(
        tf.errors.InvalidArgumentError,
        'Attempted to merge two checkpoint entries for slice name: `a`'):
      append_slices.merge_appended_slices(checkpoint_path)

  def test_append_and_merge_using_same_filename(self):
    checkpoint_path = self.new_tempfile_path()
    for _ in range(2):
      # Without calling this we might append to a previously used file.
      delete_file.delete_file(checkpoint_path)

      tensor_names = ['a', 'b']
      tensor_values = [tf.constant(x, dtype=tf.int32) for x in (7, 11)]
      for (tensor_name, tensor_value) in zip(tensor_names, tensor_values):
        append_slices.append_slices(
            filename=checkpoint_path,
            tensor_names=[tensor_name],
            data=[tensor_value],
            shapes_and_slices=[''])
      append_slices.merge_appended_slices(checkpoint_path)
      restored = tf.raw_ops.RestoreV2(
          prefix=checkpoint_path,
          tensor_names=tensor_names,
          shape_and_slices=[''] * 2,
          dtypes=[tf.int32] * 2)
      self.assertEqual(restored[0], 7)
      self.assertEqual(restored[1], 11)


if __name__ == '__main__':
  tf.test.main()
