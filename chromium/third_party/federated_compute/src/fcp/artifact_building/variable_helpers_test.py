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

from absl.testing import absltest
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff

from fcp.artifact_building import artifact_constants
from fcp.artifact_building import variable_helpers


@federated_language.federated_computation(
    federated_language.FederatedType(np.int32, federated_language.SERVER),
    federated_language.FederatedType(np.float32, federated_language.CLIENTS),
)
def sample_comp(x, y):
  a = federated_language.federated_broadcast(x)
  output1 = federated_language.federated_sum(a)
  output2 = federated_language.federated_mean([y, y], y)
  return output1, output2


class VariableHelpersTest(absltest.TestCase):

  def test_create_vars_for_tff_type(self):
    with tf.Graph().as_default():
      vl = variable_helpers.create_vars_for_tff_type(
          federated_language.StructType(
              [('a', np.int32), ('b', [('c', np.bool_), ('d', np.float32)])]
          ),  # pytype: disable=wrong-arg-types
          'x',
      )
    self.assertLen(vl, 3)
    for v in vl:
      self.assertTrue(type(v).__name__.endswith('Variable'))
      self.assertEqual(v.shape.ndims, 0)
    self.assertEqual([v.dtype for v in vl], [tf.int32, tf.bool, tf.float32])
    self.assertEqual([v.name for v in vl], ['x/a:0', 'x/b/c:0', 'x/b/d:0'])

  def test_create_vars_for_tff_type_with_none_and_zero_shape(self):
    with tf.Graph().as_default():
      vl = variable_helpers.create_vars_for_tff_type(
          federated_language.TensorType(dtype=np.int32, shape=[5, None, 0])
      )
      self.assertLen(vl, 1)
      test_variable = vl[0]
      self.assertEqual(test_variable.initial_value.shape.as_list(), [5, 0, 0])
      self.assertEqual(test_variable.shape.as_list(), [5, None, None])

  def test_create_vars_for_tff_federated_type(self):
    tff_type = federated_language.FederatedType(
        federated_language.TensorType(np.int32), federated_language.SERVER
    )
    with tf.Graph().as_default():
      vl = variable_helpers.create_vars_for_tff_type(tff_type)

    self.assertLen(vl, 1)
    v = vl[0]
    self.assertTrue(type(v).__name__.endswith('Variable'))
    self.assertEqual(v.shape.ndims, 0)
    self.assertEqual(v.dtype, tf.int32)
    self.assertEqual(v.name, 'v:0')

  def test_create_vars_for_struct_of_tff_federated_types(self):
    tff_type = federated_language.StructType([
        (
            'num_examples_secagg',
            federated_language.FederatedType(
                federated_language.TensorType(np.int32),
                federated_language.SERVER,
            ),
        ),
        (
            'num_examples_simpleagg',
            federated_language.FederatedType(
                federated_language.TensorType(np.int32),
                federated_language.SERVER,
            ),
        ),
    ])
    with tf.Graph().as_default():
      vl = variable_helpers.create_vars_for_tff_type(tff_type)

    self.assertLen(vl, 2)
    for v in vl:
      self.assertTrue(type(v).__name__.endswith('Variable'))
      self.assertEqual(v.shape.ndims, 0)
      self.assertEqual([v.dtype for v in vl], [np.int32, np.int32])
      self.assertEqual(
          [v.name for v in vl],
          ['v/num_examples_secagg:0', 'v/num_examples_simpleagg:0'],
      )

  def test_variable_names_from_type_with_tensor_type_and_no_name(self):
    names = variable_helpers.variable_names_from_type(
        federated_language.TensorType(dtype=np.int32)
    )
    self.assertEqual(names, ['v'])

  def test_variable_names_from_type_with_tensor_type(self):
    names = variable_helpers.variable_names_from_type(
        federated_language.TensorType(dtype=np.int32),
        'test_name',
    )
    self.assertEqual(names, ['test_name'])

  def test_variable_names_from_type_with_federated_type(self):
    names = variable_helpers.variable_names_from_type(
        federated_language.FederatedType(
            federated_language.TensorType(dtype=np.int32),
            federated_language.SERVER,
        ),
        'test_name',
    )
    self.assertEqual(names, ['test_name'])

  def test_variable_names_from_type_with_named_tuple_type_and_no_name(self):
    names = variable_helpers.variable_names_from_type(
        federated_language.to_type(
            [('a', np.int32), ('b', [('c', np.bool_), ('d', np.float32)])]
        )  # pytype: disable=wrong-arg-types
    )
    self.assertEqual(names, ['v/a', 'v/b/c', 'v/b/d'])

  def test_variable_names_from_type_with_named_tuple_type(self):
    names = variable_helpers.variable_names_from_type(
        federated_language.to_type(
            [('a', np.int32), ('b', [('c', np.bool_), ('d', np.float32)])]
        ),  # pytype: disable=wrong-arg-types
        'test_name',
    )
    self.assertEqual(names, ['test_name/a', 'test_name/b/c', 'test_name/b/d'])

  def test_variable_names_from_type_with_named_tuple_type_no_name_field(self):
    names = variable_helpers.variable_names_from_type(
        federated_language.to_type([(np.int32), ('b', [(np.bool_), ('d', np.float32)])]),  # pytype: disable=wrong-arg-types
        'test_name',
    )
    self.assertEqual(names, ['test_name/0', 'test_name/b/0', 'test_name/b/d'])

  def test_get_flattened_tensor_specs_with_tensor_type(self):
    specs = variable_helpers.get_flattened_tensor_specs(
        federated_language.TensorType(dtype=np.int32, shape=[3, 5]),
        'test_name',
    )
    self.assertEqual(
        specs,
        [
            tf.TensorSpec(
                name='test_name',
                shape=tf.TensorShape([3, 5]),
                dtype=tf.int32,
            )
        ],
    )

  def test_get_flattened_tensor_specs_with_federated_type(self):
    specs = variable_helpers.get_flattened_tensor_specs(
        federated_language.FederatedType(
            federated_language.TensorType(dtype=np.int32, shape=[3, 5]),
            federated_language.SERVER,
        ),
        'test_name',
    )
    self.assertEqual(
        specs,
        [
            tf.TensorSpec(
                name='test_name',
                shape=tf.TensorShape([3, 5]),
                dtype=tf.int32,
            )
        ],
    )

  def test_get_flattened_tensor_specs_with_tuple_type(self):
    specs = variable_helpers.get_flattened_tensor_specs(
        federated_language.StructType([
            (
                'a',
                federated_language.TensorType(dtype=np.int32, shape=[3, 5]),
            ),
            (
                'b',
                federated_language.StructType([
                    (federated_language.TensorType(dtype=np.bool_, shape=[4])),
                    (
                        'd',
                        federated_language.TensorType(
                            dtype=np.float32,
                            shape=[1, 3, 5],
                        ),
                    ),
                ]),
            ),
        ]),
        'test_name',
    )
    self.assertEqual(
        specs,
        [
            tf.TensorSpec(
                name='test_name/a',
                shape=tf.TensorShape([3, 5]),
                dtype=tf.int32,
            ),
            tf.TensorSpec(
                name='test_name/b/0',
                shape=tf.TensorShape([4]),
                dtype=tf.bool,
            ),
            tf.TensorSpec(
                name='test_name/b/d',
                shape=tf.TensorShape([1, 3, 5]),
                dtype=tf.float32,
            ),
        ],
    )

  def test_get_grouped_input_tensor_specs_for_aggregations(self):
    daf = tff.backends.mapreduce.get_distribute_aggregate_form_for_computation(
        sample_comp
    )
    grouped_input_tensor_specs = variable_helpers.get_grouped_input_tensor_specs_for_aggregations(
        daf.client_to_server_aggregation.to_building_block(),
        artifact_constants.AGGREGATION_INTRINSIC_ARG_SELECTION_INDEX_TO_NAME_DICT,
    )
    self.assertEqual(
        grouped_input_tensor_specs,
        [
            [  # federated_weighted_mean intrinsic args
                [  # federated_weighted_mean value arg
                    tf.TensorSpec(
                        name='update/0/0',
                        shape=tf.TensorShape([]),
                        dtype=tf.float32,
                    ),
                    tf.TensorSpec(
                        name='update/0/1',
                        shape=tf.TensorShape([]),
                        dtype=tf.float32,
                    ),
                ],
                [  # federated_weighted_mean weight arg
                    tf.TensorSpec(
                        name='update/1',
                        shape=tf.TensorShape([]),
                        dtype=tf.float32,
                    )
                ],
            ],
            [  # federated_sum intrinsic args
                [  # federated_sum value arg
                    tf.TensorSpec(
                        name='update/2',
                        shape=tf.TensorShape([]),
                        dtype=tf.int32,
                    )
                ],
            ],
        ],
    )

  def test_get_grouped_input_tensor_specs_for_aggregations_raises_value_error(
      self,
  ):

    @federated_language.federated_computation(
        federated_language.FederatedType(np.int32, federated_language.SERVER),
        federated_language.FederatedType(
            np.float32, federated_language.CLIENTS
        ),
    )
    def _comp(x, y):
      a = federated_language.federated_broadcast(x)
      output1 = federated_language.federated_secure_sum_bitwidth(a, 5)
      output2 = federated_language.federated_mean([y, y], y)
      return output1, output2

    daf = tff.backends.mapreduce.get_distribute_aggregate_form_for_computation(
        _comp
    )

    with self.assertRaises(ValueError):
      variable_helpers.get_grouped_input_tensor_specs_for_aggregations(
          daf.client_to_server_aggregation.to_building_block(),
          artifact_constants.AGGREGATION_INTRINSIC_ARG_SELECTION_INDEX_TO_NAME_DICT,
      )

  def test_get_grouped_output_tensor_specs_for_aggregations(self):
    daf = tff.backends.mapreduce.get_distribute_aggregate_form_for_computation(
        sample_comp
    )
    grouped_output_tensor_specs = (
        variable_helpers.get_grouped_output_tensor_specs_for_aggregations(
            daf.client_to_server_aggregation.to_building_block()
        )
    )
    self.assertEqual(
        grouped_output_tensor_specs,
        [
            [  # federated_weighted_mean intrinsic output
                tf.TensorSpec(
                    name='intermediate_update/0/0/0',
                    shape=tf.TensorShape([]),
                    dtype=tf.float32,
                ),
                tf.TensorSpec(
                    name='intermediate_update/0/0/1',
                    shape=tf.TensorShape([]),
                    dtype=tf.float32,
                ),
            ],
            [  # federated_secure_sum_bitwidth intrinsic output
                tf.TensorSpec(
                    name='intermediate_update/0/1',
                    shape=tf.TensorShape([]),
                    dtype=tf.int32,
                )
            ],
        ],
    )


if __name__ == '__main__':
  absltest.main()
