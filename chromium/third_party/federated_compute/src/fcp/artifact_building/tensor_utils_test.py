# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for tensor_utils."""

from absl.testing import absltest
from absl.testing import parameterized

import tensorflow as tf

from google.protobuf import any_pb2
from fcp.artifact_building import tensor_utils


class TensorUtilsTest(parameterized.TestCase, tf.test.TestCase):

  def test_bare_name(self):
    self.assertEqual(tensor_utils.bare_name('foo'), 'foo')
    self.assertEqual(tensor_utils.bare_name('foo:0'), 'foo')
    self.assertEqual(tensor_utils.bare_name('foo:1'), 'foo')
    self.assertEqual(tensor_utils.bare_name('^foo:1'), 'foo')
    self.assertEqual(tensor_utils.bare_name('^foo:output:2'), 'foo')
    with tf.Graph().as_default() as g:
      v = tf.Variable(0.0, name='foo')
      self.assertEqual(tensor_utils.bare_name(v), 'foo')

      @tf.function
      def foo(x):
        return tf.add(x, v.read_value(), 'add_op')

      foo(tf.constant(1.0))

    # Exchange the input tensor names (the outputs of other nodes) in the graph
    # to ensure we can recover the original user-specified bare names.
    graph_def = g.as_graph_def()
    # Test that the graph def contains
    graph_def_str = str(graph_def)
    self.assertIn('add_op:z:0', graph_def_str)
    self.assertIn('Read/ReadVariableOp:value:0', graph_def_str)
    # Ensure that we can locate
    required_names = ['add_op', 'Read/ReadVariableOp']
    for node in graph_def.library.function[0].node_def:
      for i in node.input:
        if tensor_utils.bare_name(i) in required_names:
          required_names.remove(tensor_utils.bare_name(i))
    self.assertEmpty(required_names)

  def test_bare_name_with_scope(self):
    self.assertEqual(tensor_utils.bare_name('bar/foo:1'), 'bar/foo')

    with tf.Graph().as_default():
      with tf.compat.v1.variable_scope('bar'):
        v = tf.Variable(0.0, name='foo')
      self.assertEqual(tensor_utils.bare_name(v), 'bar/foo')

  def test_name_or_str_with_named_variable(self):
    with tf.Graph().as_default():
      v = tf.Variable(0.0, name='foo')
      self.assertEqual('foo:0', tensor_utils.name_or_str(v))

  def test_name_or_str_with_unnamed_variable(self):
    with tf.Graph().as_default():
      v = tf.Variable(0.0)
      self.assertEqual('Variable:0', tensor_utils.name_or_str(v))

  def test_import_graph_def_from_any(self):
    with tf.Graph().as_default() as g:
      tf.constant(0.0)
      graph_def = g.as_graph_def()
    graph_def_any = any_pb2.Any()
    graph_def_any.Pack(graph_def)
    # Graph object doesn't have equality, so we check that the graph defs match.
    self.assertEqual(
        tensor_utils.import_graph_def_from_any(graph_def_any), g.as_graph_def()
    )

  def test_save_and_restore_in_eager_mode(self):
    filename = tf.constant(self.create_tempfile().full_path)
    tensor_name = 'a'
    tensor = tf.constant(1.0)
    tensor_utils.save(filename, [tensor_name], [tensor])
    restored_tensor = tensor_utils.restore(filename, tensor_name, tensor.dtype)
    self.assertAllEqual(tensor, restored_tensor)

  @parameterized.named_parameters(
      ('scalar_tensor', tf.constant(1.0)),
      ('non_scalar_tensor', tf.constant([1.0, 2.0])),
  )
  def test_save_and_restore_with_shape_info_in_eager_mode(self, tensor):
    filename = tf.constant(self.create_tempfile().full_path)
    tensor_name = 'a'
    tensor_utils.save(filename, [tensor_name], [tensor])
    restored_tensor = tensor_utils.restore(
        filename, tensor_name, tensor.dtype, tensor.shape
    )
    self.assertAllEqual(tensor, restored_tensor)

  def _assert_op_in_graph(self, expected_op, graph):
    graph_def = graph.as_graph_def()
    node_ops = [node.op for node in graph_def.node]
    self.assertIn(expected_op, node_ops)

  def _get_shape_and_slices_value(self, graph):
    graph_def = graph.as_graph_def()
    node_name_to_value_dict = {node.name: node for node in graph_def.node}
    self.assertIn('restore/shape_and_slices', node_name_to_value_dict)
    return (
        node_name_to_value_dict['restore/shape_and_slices']
        .attr['value']
        .tensor.string_val[0]
    )

  def test_save_and_restore_in_graph_mode(self):
    temp_file = self.create_tempfile().full_path
    graph = tf.Graph()
    with graph.as_default():
      filename = tf.constant(temp_file)
      tensor_name = 'a'
      tensor = tf.constant(1.0)
      save_op = tensor_utils.save(filename, [tensor_name], [tensor])
      restored = tensor_utils.restore(filename, tensor_name, tensor.dtype)
    with tf.compat.v1.Session(graph=graph) as sess:
      sess.run(save_op)
      expected_tensor, restored_tensor = sess.run([tensor, restored])
      self.assertAllEqual(expected_tensor, restored_tensor)
      self._assert_op_in_graph(expected_op='SaveSlices', graph=graph)
      self._assert_op_in_graph(expected_op='RestoreV2', graph=graph)
      self.assertEqual(b'', self._get_shape_and_slices_value(graph))

  @parameterized.named_parameters(
      ('scalar_tensor', lambda: tf.constant(1.0), b''),
      ('non_scalar_tensor', lambda: tf.constant([1.0, 2.0]), b'2 :-'),
  )
  def test_save_and_restore_with_shape_info_in_graph_mode(
      self, tensor_builder, expected_shape_and_slices_value
  ):
    temp_file = self.create_tempfile().full_path
    graph = tf.Graph()
    with graph.as_default():
      filename = tf.constant(temp_file)
      tensor_name = 'a'
      tensor = tensor_builder()
      save_op = tensor_utils.save(filename, [tensor_name], [tensor])
      restored = tensor_utils.restore(
          filename, tensor_name, tensor.dtype, tensor.shape
      )
    with tf.compat.v1.Session(graph=graph) as sess:
      sess.run(save_op)
      expected_tensor, restored_tensor = sess.run([tensor, restored])
      self.assertAllEqual(expected_tensor, restored_tensor)
      self._assert_op_in_graph(expected_op='SaveSlices', graph=graph)
      self._assert_op_in_graph(expected_op='RestoreV2', graph=graph)
      self.assertEqual(
          expected_shape_and_slices_value,
          self._get_shape_and_slices_value(graph),
      )


if __name__ == '__main__':
  absltest.main()
