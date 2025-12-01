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

import collections

from absl.testing import absltest
import federated_language
from federated_language.proto import computation_pb2
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from fcp.artifact_building import data_spec
from fcp.artifact_building import graph_helpers
from fcp.artifact_building import variable_helpers
from fcp.protos import plan_pb2

TRAIN_URI = 'boo'
TEST_URI = 'foo'
NUM_PIXELS = 784
FAKE_INPUT_DIRECTORY_TENSOR = tf.constant('/path/to/input_dir')


class EmbedDataLogicTest(absltest.TestCase):

  def assertTensorSpec(self, tensor, name, shape, dtype):
    self.assertIsInstance(tensor, tf.Tensor)
    self.assertEqual(tensor.name, name)
    self.assertEqual(tensor.shape, shape)
    self.assertEqual(tensor.dtype, dtype)

  def test_one_dataset_of_integers_w_dataspec(self):
    with tf.Graph().as_default():
      token_placeholder, data_values, placeholders = (
          graph_helpers.embed_data_logic(
              federated_language.SequenceType((np.str_)),
              data_spec.DataSpec(
                  plan_pb2.ExampleSelector(collection_uri='app://fake_uri')
              ),
          )
      )

    self.assertTensorSpec(token_placeholder, 'data_token:0', [], tf.string)
    self.assertLen(data_values, 1)
    self.assertTensorSpec(data_values[0], 'data_0/Identity:0', [], tf.variant)
    self.assertEmpty(placeholders)

  def test_two_datasets_of_integers_w_dataspec(self):
    with tf.Graph().as_default():
      token_placeholder, data_values, placeholders = (
          graph_helpers.embed_data_logic(
              collections.OrderedDict(
                  A=federated_language.SequenceType((np.str_)),
                  B=federated_language.SequenceType((np.str_)),
              ),  # pytype: disable=wrong-arg-types
              collections.OrderedDict(
                  A=data_spec.DataSpec(
                      plan_pb2.ExampleSelector(collection_uri='app://foo')
                  ),
                  B=data_spec.DataSpec(
                      plan_pb2.ExampleSelector(collection_uri='app://bar')
                  ),
              ),
          )
      )

    self.assertTensorSpec(token_placeholder, 'data_token:0', [], tf.string)

    self.assertLen(data_values, 2)
    self.assertTensorSpec(data_values[0], 'data_0/Identity:0', [], tf.variant)
    self.assertTensorSpec(data_values[1], 'data_1/Identity:0', [], tf.variant)
    self.assertEmpty(placeholders)

  def test_nested_dataspec(self):
    with tf.Graph().as_default():
      token_placeholder, data_values, placeholders = (
          graph_helpers.embed_data_logic(
              collections.OrderedDict(
                  A=collections.OrderedDict(
                      B=federated_language.SequenceType(np.str_)
                  )
              ),  # pytype: disable=wrong-arg-types
              collections.OrderedDict(
                  A=collections.OrderedDict(
                      B=data_spec.DataSpec(
                          plan_pb2.ExampleSelector(collection_uri='app://foo')
                      )
                  )
              ),
          )
      )

    self.assertTensorSpec(token_placeholder, 'data_token:0', [], tf.string)
    self.assertLen(data_values, 1)
    self.assertTensorSpec(data_values[0], 'data_0/Identity:0', [], tf.variant)
    self.assertEmpty(placeholders)

  def test_one_dataset_of_integers_without_dataspec(self):
    with tf.Graph().as_default():
      token_placeholder, data_values, placeholders = (
          graph_helpers.embed_data_logic(
              federated_language.SequenceType(np.str_)
          )
      )

    self.assertTensorSpec(token_placeholder, 'data_token:0', [], tf.string)
    self.assertLen(data_values, 1)
    self.assertTensorSpec(data_values[0], 'data_0/Identity:0', [], tf.variant)
    self.assertLen(placeholders, 1)
    self.assertEqual(placeholders[0].name, 'example_selector:0')

  def test_two_datasets_of_integers_without_dataspec(self):
    with tf.Graph().as_default():
      token_placeholder, data_values, placeholders = (
          graph_helpers.embed_data_logic(
              collections.OrderedDict(
                  A=federated_language.SequenceType(np.str_),
                  B=federated_language.SequenceType(np.str_),
              )  # pytype: disable=wrong-arg-types
          )
      )

    self.assertTensorSpec(token_placeholder, 'data_token:0', [], tf.string)

    self.assertLen(data_values, 2)
    self.assertTensorSpec(data_values[0], 'data_0/Identity:0', [], tf.variant)
    self.assertTensorSpec(data_values[1], 'data_1/Identity:0', [], tf.variant)
    self.assertLen(placeholders, 2)
    self.assertEqual(placeholders[0].name, 'example_selector_0:0')
    self.assertEqual(placeholders[1].name, 'example_selector_1:0')

  def test_nested_input_without_dataspec(self):
    with tf.Graph().as_default():
      token_placeholder, data_values, placeholders = (
          graph_helpers.embed_data_logic(
              collections.OrderedDict(
                  A=collections.OrderedDict(
                      B=federated_language.SequenceType(np.str_)
                  )
              )  # pytype: disable=wrong-arg-types
          )
      )

    self.assertTensorSpec(token_placeholder, 'data_token:0', [], tf.string)
    self.assertLen(data_values, 1)
    self.assertTensorSpec(data_values[0], 'data_0/Identity:0', [], tf.variant)
    self.assertLen(placeholders, 1)
    self.assertEqual(placeholders[0].name, 'example_selector_0_0:0')


class GraphHelperTest(absltest.TestCase):

  def test_import_tensorflow(self):
    # NOTE: Minimal test for now, since this is exercised by other components,
    # just a single example with a combo of all flavors of params and results.
    @tff.tensorflow.computation(
        federated_language.SequenceType(np.int64), tf.int64
    )
    def work(ds, x):
      return x + 1, ds.map(lambda a: a + x)

    with tf.Graph().as_default():
      ds = tf.data.experimental.to_variant(tf.data.Dataset.range(3))
      v = tf.constant(10, dtype=tf.int64)
      y, ds2_variant = graph_helpers.import_tensorflow(
          'work', work, ([ds], [v]), split_outputs=True
      )
      ds2 = tf.data.experimental.from_variant(
          ds2_variant[0], tf.TensorSpec([], tf.int64)
      )
      z = ds2.reduce(np.int64(0), lambda x, y: x + y)
      with tf.compat.v1.Session() as sess:
        self.assertEqual(sess.run(y[0]), 11)
        self.assertEqual(sess.run(z), 33)

  def test_import_tensorflow_with_session_token(self):

    @tff.tensorflow.computation
    def return_value():
      return federated_language.framework.get_context_stack().current.session_token  # pytype: disable=attribute-error

    with tf.Graph().as_default():
      x = tf.compat.v1.placeholder(dtype=tf.string)
      output = graph_helpers.import_tensorflow(
          'return_value', comp=return_value, session_token_tensor=x
      )
      with tf.compat.v1.Session() as sess:
        self.assertEqual(sess.run(output[0], feed_dict={x: 'value'}), b'value')

  def test_import_tensorflow_with_control_dep_remap(self):
    # Assert that importing graphdef remaps both regular and control dep inputs.
    @tff.tensorflow.computation(tf.int64, tf.int64)
    def work(x, y):
      # Insert a control dependency to ensure it is remapped during import.
      with tf.compat.v1.control_dependencies([y]):
        return tf.identity(x)

    with tf.Graph().as_default():
      x = tf.compat.v1.placeholder(dtype=tf.int64)
      y = tf.compat.v1.placeholder(dtype=tf.int64)
      output = graph_helpers.import_tensorflow(
          'control_dep_graph', comp=work, args=[x, y]
      )
      with tf.compat.v1.Session() as sess:
        self.assertEqual(sess.run(output, feed_dict={x: 10, y: 20})[0], 10)

  def test_add_control_deps_for_init_op(self):
    # Creates a graph (double edges are regular dependencies, single edges are
    # control dependencies) like this:
    #
    #  ghi
    #   |
    #  def
    #   ||
    #  def:0         foo
    #   ||        //     ||
    #  abc      bar      ||
    #     \   //   \\    ||
    #      bak        baz
    #
    graph_def = tf.compat.v1.GraphDef(
        node=[
            tf.compat.v1.NodeDef(name='foo', input=[]),
            tf.compat.v1.NodeDef(name='bar', input=['foo']),
            tf.compat.v1.NodeDef(name='baz', input=['foo', 'bar']),
            tf.compat.v1.NodeDef(name='bak', input=['bar', '^abc']),
            tf.compat.v1.NodeDef(name='abc', input=['def:0']),
            tf.compat.v1.NodeDef(name='def', input=['^ghi']),
            tf.compat.v1.NodeDef(name='ghi', input=[]),
        ]
    )
    new_graph_def = graph_helpers.add_control_deps_for_init_op(graph_def, 'abc')
    self.assertEqual(
        ','.join(
            '{}({})'.format(node.name, ','.join(node.input))
            for node in new_graph_def.node
        ),
        (
            'foo(^abc),bar(foo,^abc),baz(foo,bar,^abc),'
            'bak(bar,^abc),abc(def:0),def(^ghi),ghi()'
        ),
    )

  def test_create_tensor_map_with_sequence_binding_and_variant(self):
    with tf.Graph().as_default():
      variant_tensor = tf.data.experimental.to_variant(tf.data.Dataset.range(3))
      input_map = graph_helpers.create_tensor_map(
          computation_pb2.TensorFlow.Binding(
              sequence=computation_pb2.TensorFlow.SequenceBinding(
                  variant_tensor_name='foo'
              )
          ),
          [variant_tensor],
      )
      self.assertLen(input_map, 1)
      self.assertCountEqual(list(input_map.keys()), ['foo'])
      self.assertIs(input_map['foo'], variant_tensor)

  def test_create_tensor_map_with_sequence_binding_and_multiple_variants(self):
    with tf.Graph().as_default():
      variant_tensor = tf.data.experimental.to_variant(tf.data.Dataset.range(3))
      with self.assertRaises(ValueError):
        graph_helpers.create_tensor_map(
            computation_pb2.TensorFlow.Binding(
                sequence=computation_pb2.TensorFlow.SequenceBinding(
                    variant_tensor_name='foo'
                )
            ),
            [variant_tensor, variant_tensor],
        )

  def test_create_tensor_map_with_sequence_binding_and_non_variant(self):
    with tf.Graph().as_default():
      non_variant_tensor = tf.constant(1)
      with self.assertRaises(TypeError):
        graph_helpers.create_tensor_map(
            computation_pb2.TensorFlow.Binding(
                sequence=computation_pb2.TensorFlow.SequenceBinding(
                    variant_tensor_name='foo'
                )
            ),
            [non_variant_tensor],
        )

  def test_create_tensor_map_with_non_sequence_binding_and_vars(self):
    with tf.Graph().as_default():
      vars_list = variable_helpers.create_vars_for_tff_type(
          federated_language.StructType([('a', np.int32), ('b', np.int32)])
      )
      init_op = tf.compat.v1.global_variables_initializer()
      assign_op = tf.group(
          *(v.assign(tf.constant(k + 1)) for k, v in enumerate(vars_list))
      )
      input_map = graph_helpers.create_tensor_map(
          computation_pb2.TensorFlow.Binding(
              struct=computation_pb2.TensorFlow.StructBinding(
                  element=[
                      computation_pb2.TensorFlow.Binding(
                          tensor=computation_pb2.TensorFlow.TensorBinding(
                              tensor_name='foo'
                          )
                      ),
                      computation_pb2.TensorFlow.Binding(
                          tensor=computation_pb2.TensorFlow.TensorBinding(
                              tensor_name='bar'
                          )
                      ),
                  ]
              )
          ),
          vars_list,
      )
      with tf.compat.v1.Session() as sess:
        sess.run(init_op)
        sess.run(assign_op)
        self.assertDictEqual(sess.run(input_map), {'foo': 1, 'bar': 2})

  def test_get_deps_for_graph_node(self):
    # Creates a graph (double edges are regular dependencies, single edges are
    # control dependencies) like this:
    #                      foo
    #                   //      \\
    #               foo:0        foo:1
    #                  ||       //
    #       abc       bar      //
    #     //    \   //   \\   //
    #  abc:0     bak       baz
    #    ||
    #   def
    #    |
    #   ghi
    #
    graph_def = tf.compat.v1.GraphDef(
        node=[
            tf.compat.v1.NodeDef(name='foo', input=[]),
            tf.compat.v1.NodeDef(name='bar', input=['foo:0']),
            tf.compat.v1.NodeDef(name='baz', input=['foo:1', 'bar']),
            tf.compat.v1.NodeDef(name='bak', input=['bar', '^abc']),
            tf.compat.v1.NodeDef(name='abc', input=[]),
            tf.compat.v1.NodeDef(name='def', input=['abc:0']),
            tf.compat.v1.NodeDef(name='ghi', input=['^def']),
        ]
    )

    def _get_deps(x):
      return ','.join(
          sorted(list(graph_helpers._get_deps_for_graph_node(graph_def, x)))
      )

    self.assertEqual(_get_deps('foo'), '')
    self.assertEqual(_get_deps('bar'), 'foo')
    self.assertEqual(_get_deps('baz'), 'bar,foo')
    self.assertEqual(_get_deps('bak'), 'abc,bar,foo')
    self.assertEqual(_get_deps('abc'), '')
    self.assertEqual(_get_deps('def'), 'abc')
    self.assertEqual(_get_deps('ghi'), 'abc,def')

  def test_list_tensor_names_in_binding(self):
    binding = computation_pb2.TensorFlow.Binding(
        struct=computation_pb2.TensorFlow.StructBinding(
            element=[
                computation_pb2.TensorFlow.Binding(
                    tensor=computation_pb2.TensorFlow.TensorBinding(
                        tensor_name='a'
                    )
                ),
                computation_pb2.TensorFlow.Binding(
                    struct=computation_pb2.TensorFlow.StructBinding(
                        element=[
                            computation_pb2.TensorFlow.Binding(
                                tensor=computation_pb2.TensorFlow.TensorBinding(
                                    tensor_name='b'
                                )
                            ),
                            computation_pb2.TensorFlow.Binding(
                                tensor=computation_pb2.TensorFlow.TensorBinding(
                                    tensor_name='c'
                                )
                            ),
                        ]
                    )
                ),
                computation_pb2.TensorFlow.Binding(
                    tensor=computation_pb2.TensorFlow.TensorBinding(
                        tensor_name='d'
                    )
                ),
                computation_pb2.TensorFlow.Binding(
                    sequence=computation_pb2.TensorFlow.SequenceBinding(
                        variant_tensor_name='e'
                    )
                ),
            ]
        )
    )
    self.assertEqual(
        graph_helpers._list_tensor_names_in_binding(binding),
        ['a', 'b', 'c', 'd', 'e'],
    )


if __name__ == '__main__':
  with federated_language.framework.get_context_stack().install(
      federated_language.framework.RuntimeErrorContext()
  ):
    absltest.main()
