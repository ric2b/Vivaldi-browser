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
"""Tests for `tensor_name` custom op."""

import tensorflow as tf

from fcp.tensorflow import tensor_name


class TensorNameTest(tf.test.TestCase):

  def test_returns_simple_name(self):
    test_name = b'placeholder_test_name'
    with tf.Graph().as_default() as graph:
      placeholder = tf.compat.v1.placeholder_with_default(
          input='default_value', shape=(), name=test_name)
      tensor_name_out = tensor_name.tensor_name(placeholder)
    with tf.compat.v1.Session(graph=graph) as sess:
      result = sess.run(tensor_name_out)
    self.assertEqual(test_name, result)

  def test_returns_modified_name_after_reimport(self):
    test_name = b'placeholder_test_name'
    with tf.Graph().as_default() as inner_graph:
      placeholder = tf.compat.v1.placeholder_with_default(
          input='default_value', shape=(), name=test_name)
      inner_tensor_name_out = tensor_name.tensor_name(placeholder)
    import_prefix = b'import_prefix_'
    with tf.Graph().as_default() as outer_graph:
      tensor_name_out = tf.graph_util.import_graph_def(
          graph_def=inner_graph.as_graph_def(),
          input_map={},
          return_elements=[inner_tensor_name_out.name],
          name=import_prefix)[0]
    with tf.compat.v1.Session(graph=outer_graph) as sess:
      result = sess.run(tensor_name_out)
    self.assertEqual(b'/'.join([import_prefix, test_name]), result)


if __name__ == '__main__':
  tf.test.main()
