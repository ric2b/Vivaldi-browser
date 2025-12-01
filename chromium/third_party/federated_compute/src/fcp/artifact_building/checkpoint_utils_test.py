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
import os
import typing
from typing import Any

from absl.testing import absltest
from absl.testing import parameterized
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from google.protobuf import any_pb2
from fcp.artifact_building import checkpoint_utils
from fcp.protos import plan_pb2


class CheckpointUtilsTest(tf.test.TestCase, parameterized.TestCase):

  def _assert_variable_functionality(
      self, test_vars: list[tf.Variable], test_value_to_save: Any = 10
  ):
    self.assertIsInstance(test_vars, list)
    initializer = tf.compat.v1.global_variables_initializer()
    for test_variable in test_vars:
      with self.test_session() as session:
        session.run(initializer)
        session.run(test_variable.assign(test_value_to_save))
        self.assertEqual(session.run(test_variable), test_value_to_save)

  def test_create_server_checkpoint_vars_and_savepoint_succeeds_state_vars(
      self,
  ):
    with tf.Graph().as_default():
      state_vars, _, _, savepoint = (
          checkpoint_utils.create_server_checkpoint_vars_and_savepoint(
              server_state_type=federated_language.StructType(
                  [('foo1', np.int32)]
              ),
              server_metrics_type=federated_language.StructType(
                  [('bar2', np.int32)]
              ),
              write_metrics_to_checkpoint=True,
          )
      )
      self.assertIsInstance(savepoint, plan_pb2.CheckpointOp)
      self._assert_variable_functionality(state_vars)

  def test_create_server_checkpoint_vars_and_savepoint_succeeds_metadata_vars(
      self,
  ):
    def additional_checkpoint_metadata_var_fn(
        state_vars, metrics_vars, write_metrics_to_checkpoint
    ):
      del state_vars, metrics_vars, write_metrics_to_checkpoint
      return [tf.Variable(initial_value=b'dog', name='metadata')]

    with tf.Graph().as_default():
      _, _, metadata_vars, savepoint = (
          checkpoint_utils.create_server_checkpoint_vars_and_savepoint(
              server_state_type=federated_language.StructType(
                  [('foo3', np.int32)]
              ),
              server_metrics_type=federated_language.StructType(
                  [('bar1', np.int32)]
              ),
              additional_checkpoint_metadata_var_fn=(
                  additional_checkpoint_metadata_var_fn
              ),
              write_metrics_to_checkpoint=True,
          )
      )
      self.assertIsInstance(savepoint, plan_pb2.CheckpointOp)
      self._assert_variable_functionality(
          metadata_vars, test_value_to_save=b'cat'
      )

  def test_create_server_checkpoint_vars_and_savepoint_succeeds_metrics_vars(
      self,
  ):
    with tf.Graph().as_default():
      _, metrics_vars, _, savepoint = (
          checkpoint_utils.create_server_checkpoint_vars_and_savepoint(
              server_state_type=federated_language.StructType(
                  [('foo2', np.int32)]
              ),
              server_metrics_type=federated_language.StructType(
                  [('bar3', np.int32)]
              ),
              write_metrics_to_checkpoint=True,
          )
      )
      self.assertIsInstance(savepoint, plan_pb2.CheckpointOp)
      self._assert_variable_functionality(metrics_vars)

  def test_tff_type_to_dtype_list_as_expected(self):
    tff_type = federated_language.FederatedType(
        federated_language.StructType([('foo', np.int32), ('bar', np.str_)]),
        federated_language.SERVER,
    )
    expected_dtype_list = [tf.int32, tf.string]
    self.assertEqual(
        checkpoint_utils.tff_type_to_dtype_list(tff_type), expected_dtype_list
    )

  def test_tff_type_to_dtype_list_type_error(self):
    list_type = [np.int32, np.str_]
    with self.assertRaisesRegex(TypeError, 'to be an instance of type'):
      checkpoint_utils.tff_type_to_dtype_list(list_type)  # pytype: disable=wrong-arg-types

  def test_tff_type_to_tensor_spec_list_as_expected(self):
    tff_type = federated_language.FederatedType(
        federated_language.StructType([
            ('foo', np.int32),
            ('bar', federated_language.TensorType(np.str_, shape=[1])),
        ]),
        federated_language.SERVER,
    )
    expected_tensor_spec_list = [
        tf.TensorSpec([], tf.int32),
        tf.TensorSpec([1], tf.string),
    ]
    self.assertEqual(
        checkpoint_utils.tff_type_to_tensor_spec_list(tff_type),
        expected_tensor_spec_list,
    )

  def test_tff_type_to_tensor_spec_list_type_error(self):
    list_type = [np.int32, np.str_]
    with self.assertRaisesRegex(TypeError, 'to be an instance of type'):
      checkpoint_utils.tff_type_to_tensor_spec_list(list_type)  # pytype: disable=wrong-arg-types

  def test_pack_tff_value_with_tensors_as_expected(self):
    tff_type = federated_language.StructType(
        [('foo', np.int32), ('bar', np.str_)]
    )
    value_list = [
        tf.constant(1, dtype=np.int32),
        tf.constant('bla', dtype=np.str_),
    ]
    expected_packed_structure = tff.structure.Struct([
        ('foo', tf.constant(1, dtype=np.int32)),
        ('bar', tf.constant('bla', dtype=np.str_)),
    ])
    self.assertEqual(
        checkpoint_utils.pack_tff_value(tff_type, value_list),
        expected_packed_structure,
    )

  def test_pack_tff_value_with_federated_server_tensors_as_expected(self):
    # This test must create a type that has `StructType`s nested under the
    # `FederatedType` to cover testing that the packing logic correctly descends
    # through the entire type tree.
    tff_type = federated_language.to_type(
        collections.OrderedDict(
            foo=federated_language.FederatedType(
                np.int32, federated_language.SERVER
            ),
            # Some arbitrarily deep nesting to ensure full traversals.
            bar=federated_language.FederatedType(
                [(), ([np.int32], np.int32)], federated_language.SERVER
            ),
        )
    )
    value_list = [tf.constant(1), tf.constant(2), tf.constant(3)]
    expected_packed_structure = tff.structure.Struct(
        [
            ('foo', tf.constant(1)),
            (
                'bar',
                tff.structure.Struct([
                    (None, tff.structure.Struct([])),
                    (
                        None,
                        tff.structure.Struct([
                            (
                                None,
                                tff.structure.Struct([
                                    (None, tf.constant(2)),
                                ]),
                            ),
                            (None, tf.constant(3)),
                        ]),
                    ),
                ]),
            ),
        ],
    )
    self.assertEqual(
        checkpoint_utils.pack_tff_value(
            tff_type,  # pytype: disable=wrong-arg-types
            value_list,
        ),
        expected_packed_structure,
    )

  def test_pack_tff_value_with_unmatched_input_sizes(self):
    tff_type = federated_language.StructType(
        [('foo', np.int32), ('bar', np.str_)]
    )
    value_list = [tf.constant(1, dtype=np.int32)]
    with self.assertRaises(ValueError):
      checkpoint_utils.pack_tff_value(tff_type, value_list)

  def test_pack_tff_value_with_tff_type_error(self):

    @federated_language.federated_computation
    def fed_comp():
      return federated_language.federated_value(0, federated_language.SERVER)

    tff_function_type = fed_comp.type_signature
    value_list = [tf.constant(1, dtype=np.int32)]
    with self.assertRaisesRegex(TypeError, 'to be an instance of type'):
      checkpoint_utils.pack_tff_value(tff_function_type, value_list)

  def test_variable_names_from_structure_with_tensor_and_no_name(self):
    names = checkpoint_utils.variable_names_from_structure(tf.constant(1.0))
    self.assertEqual(names, ['v'])

  def test_variable_names_from_structure_with_tensor(self):
    names = checkpoint_utils.variable_names_from_structure(
        tf.constant(1.0), 'test_name'
    )
    self.assertEqual(names, ['test_name'])

  def test_variable_names_from_structure_with_named_tuple_type_and_no_name(
      self,
  ):
    names = checkpoint_utils.variable_names_from_structure(
        tff.structure.Struct([
            ('a', tf.constant(1.0)),
            (
                'b',
                tff.structure.Struct(
                    [('c', tf.constant(True)), ('d', tf.constant(0.0))]
                ),
            ),
        ])
    )
    self.assertEqual(names, ['v/a', 'v/b/c', 'v/b/d'])

  def test_variable_names_from_structure_with_named_struct(self):
    names = checkpoint_utils.variable_names_from_structure(
        tff.structure.Struct([
            ('a', tf.constant(1.0)),
            (
                'b',
                tff.structure.Struct(
                    [('c', tf.constant(True)), ('d', tf.constant(0.0))]
                ),
            ),
        ]),
        'test_name',
    )
    self.assertEqual(names, ['test_name/a', 'test_name/b/c', 'test_name/b/d'])

  def test_variable_names_from_structure_with_named_tuple_type_no_name_field(
      self,
  ):
    names = checkpoint_utils.variable_names_from_structure(
        tff.structure.Struct([
            (None, tf.constant(1.0)),
            (
                None,
                tff.structure.Struct(
                    [(None, tf.constant(False)), (None, tf.constant(0.0))]
                ),
            ),
        ]),
        'test_name',
    )
    self.assertEqual(names, ['test_name/0', 'test_name/1/0', 'test_name/1/1'])

  def test_save_tf_tensor_to_checkpoint_as_expected(self):
    temp_dir = self.create_tempdir()
    output_checkpoint_path = os.path.join(temp_dir, 'output_checkpoint.ckpt')

    tensor = tf.constant([[1.0, 2.0], [3.0, 4.0]])

    checkpoint_utils.save_tff_structure_to_checkpoint(
        tensor, ['v'], output_checkpoint_path=output_checkpoint_path
    )

    reader = tf.compat.v1.train.NewCheckpointReader(output_checkpoint_path)
    var_to_shape_map = reader.get_variable_to_shape_map()
    self.assertLen(var_to_shape_map, 1)
    self.assertIn('v', var_to_shape_map)
    np.testing.assert_almost_equal(
        [[1.0, 2.0], [3.0, 4.0]], reader.get_tensor('v')
    )

  def test_save_tff_struct_to_checkpoint_as_expected(self):
    temp_dir = self.create_tempdir()
    output_checkpoint_path = os.path.join(temp_dir, 'output_checkpoint.ckpt')

    struct = tff.structure.Struct([
        ('foo', tf.constant(1, dtype=tf.int32)),
        ('bar', tf.constant('bla', dtype=tf.string)),
    ])

    checkpoint_utils.save_tff_structure_to_checkpoint(
        struct,
        ordered_var_names=['v/foo', 'v/bar'],
        output_checkpoint_path=output_checkpoint_path,
    )

    reader = tf.compat.v1.train.NewCheckpointReader(output_checkpoint_path)
    var_to_shape_map = reader.get_variable_to_shape_map()
    self.assertLen(var_to_shape_map, 2)
    self.assertIn('v/foo', var_to_shape_map)
    self.assertIn('v/bar', var_to_shape_map)
    self.assertEqual(1, reader.get_tensor('v/foo'))
    self.assertEqual(b'bla', reader.get_tensor('v/bar'))

  def test_save_tff_struct_to_checkpoint_fails_if_wrong_num_var_names(self):
    temp_dir = self.create_tempdir()
    output_checkpoint_path = os.path.join(temp_dir, 'output_checkpoint.ckpt')

    struct = tff.structure.Struct([
        ('foo', tf.constant(1, dtype=tf.int32)),
        ('bar', tf.constant('bla', dtype=tf.string)),
    ])

    with self.assertRaisesRegex(ValueError, 'does not match the number'):
      checkpoint_utils.save_tff_structure_to_checkpoint(
          struct,
          ordered_var_names=['v/foo'],
          output_checkpoint_path=output_checkpoint_path,
      )

  @parameterized.named_parameters(
      ('tf.tensor', tf.constant(1.0)),
      ('ndarray', np.asarray([1.0, 2.0, 3.0])),
      ('npnumber', np.float64(1.0)),
      ('int', 1),
      ('float', 1.0),
      ('str', 'test'),
      ('bytes', b'test'),
  )
  def test_is_allowed(self, structure):
    self.assertTrue(checkpoint_utils.is_structure_of_allowed_types(structure))

  @parameterized.named_parameters(
      ('function', lambda x: x),
      ('any_proto', any_pb2.Any()),
  )
  def test_is_not_allowed(self, structure):
    self.assertFalse(checkpoint_utils.is_structure_of_allowed_types(structure))


class CreateDeterministicSaverTest(tf.test.TestCase):

  def test_failure_unknown_type(self):
    with self.assertRaisesRegex(ValueError, 'Do not know how to make'):
      # Using a cast in case the test is being run with static type checking.
      checkpoint_utils.create_deterministic_saver(
          typing.cast(list[tf.Variable], 0)
      )

  def test_creates_saver_for_list(self):
    with tf.Graph().as_default() as g:
      saver = checkpoint_utils.create_deterministic_saver([
          tf.Variable(initial_value=1.0, name='z'),
          tf.Variable(initial_value=2.0, name='x'),
          tf.Variable(initial_value=3.0, name='y'),
      ])
    self.assertIsInstance(saver, tf.compat.v1.train.Saver)
    test_filepath = self.create_tempfile().full_path
    with tf.compat.v1.Session(graph=g) as sess:
      sess.run(tf.compat.v1.global_variables_initializer())
      saver.save(sess, save_path=test_filepath)
    variable_specs = tf.train.list_variables(test_filepath)
    self.assertEqual([('x', []), ('y', []), ('z', [])], variable_specs)

  def test_creates_saver_for_dict(self):
    with tf.Graph().as_default() as g:
      saver = checkpoint_utils.create_deterministic_saver({
          'foo': tf.Variable(initial_value=1.0, name='z'),
          'baz': tf.Variable(initial_value=2.0, name='x'),
          'bar': tf.Variable(initial_value=3.0, name='y'),
      })
    self.assertIsInstance(saver, tf.compat.v1.train.Saver)
    test_filepath = self.create_tempfile().full_path
    with tf.compat.v1.Session(graph=g) as sess:
      sess.run(tf.compat.v1.global_variables_initializer())
      saver.save(sess, save_path=test_filepath)
    variable_specs = tf.train.list_variables(test_filepath)
    self.assertEqual([('bar', []), ('baz', []), ('foo', [])], variable_specs)


if __name__ == '__main__':
  absltest.main()
