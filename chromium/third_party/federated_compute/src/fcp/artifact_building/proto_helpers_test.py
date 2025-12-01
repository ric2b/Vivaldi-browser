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
"""Tests for proto_helpers.py."""

import collections

import federated_language
import numpy as np
import tensorflow as tf

import tensorflow_federated as tff

from fcp.artifact_building import proto_helpers
from fcp.artifact_building import variable_helpers


class MakeMetricTest(tf.test.TestCase):

  def test_make_metric(self):
    with tf.Graph().as_default():
      v = variable_helpers.create_vars_for_tff_type(
          federated_language.StructType(
              collections.OrderedDict([("bar", np.int32)])
          ),
          name="foo",
      )
      self.assertProtoEquals(
          "variable_name: 'Identity:0' stat_name: 'client/bar'",
          proto_helpers.make_metric(v[0], "client"),
      )


class MakeTensorSpecTest(tf.test.TestCase):

  def test_fully_defined_shape(self):
    with tf.Graph().as_default():
      test_tensor = tf.constant([[1], [2]])  # Shape [1, 2]
      with self.subTest("no_hint"):
        tensor_spec = proto_helpers.make_tensor_spec_from_tensor(test_tensor)
        self.assertProtoEquals(
            (
                "name: 'Const:0' "
                "shape { "
                "  dim { size: 2 } "
                "  dim { size: 1 } "
                "} "
                "dtype: DT_INT32"
            ),
            tensor_spec.experimental_as_proto(),
        )
      with self.subTest("ignored_hint"):
        # Supplied shape hint is incompatible, but ignored because tensor is
        # fully defined.
        tensor_spec = proto_helpers.make_tensor_spec_from_tensor(
            test_tensor, shape_hint=tf.TensorShape([1, 4])
        )
        self.assertProtoEquals(
            (
                "name: 'Const:0' "
                "shape { "
                "  dim { size: 2 } "
                "  dim { size: 1 } "
                "} "
                "dtype: DT_INT32"
            ),
            tensor_spec.experimental_as_proto(),
        )

  def test_undefined_shape(self):
    with tf.Graph().as_default():
      # Create a undefined shape tensor via a placeholder and an op that doesn't
      # alter shape.
      test_tensor = tf.clip_by_value(
          tf.compat.v1.placeholder(dtype=tf.int32), 0, 1
      )
      with self.subTest("no_hint"):
        tensor_spec = proto_helpers.make_tensor_spec_from_tensor(test_tensor)
        self.assertProtoEquals(
            (
                "name: 'clip_by_value:0' "
                "shape { "
                " unknown_rank: true "
                "} "
                "dtype: DT_INT32"
            ),
            tensor_spec.experimental_as_proto(),
        )
      with self.subTest("hint"):
        tensor_spec = proto_helpers.make_tensor_spec_from_tensor(
            test_tensor, shape_hint=tf.TensorShape([1, 4])
        )
        self.assertProtoEquals(
            (
                "name: 'clip_by_value:0' "
                "shape { "
                "  dim { size: 1 } "
                "  dim { size: 4 } "
                "} "
                "dtype: DT_INT32"
            ),
            tensor_spec.experimental_as_proto(),
        )

  def test_partially_defined_shape(self):
    with tf.Graph().as_default():
      # Create a partially defined shape tensor via a placeholder and a reshape
      # to specify some dimensions.
      test_tensor = tf.reshape(
          tf.compat.v1.placeholder(dtype=tf.int32), [2, -1]
      )
      with self.subTest("no_hint"):
        tensor_spec = proto_helpers.make_tensor_spec_from_tensor(test_tensor)
        self.assertProtoEquals(
            (
                "name: 'Reshape:0' "
                "shape { "
                "  dim { size: 2 } "
                "  dim { size: -1 } "
                "} "
                "dtype: DT_INT32"
            ),
            tensor_spec.experimental_as_proto(),
        )
      with self.subTest("hint"):
        tensor_spec = proto_helpers.make_tensor_spec_from_tensor(
            test_tensor, shape_hint=tf.TensorShape([2, 4])
        )
        self.assertProtoEquals(
            (
                "name: 'Reshape:0' "
                "shape { "
                "  dim { size: 2 } "
                "  dim { size: 4} "
                "} "
                "dtype: DT_INT32"
            ),
            tensor_spec.experimental_as_proto(),
        )
      with self.subTest("invalid_hint"):
        with self.assertRaises(TypeError):
          _ = proto_helpers.make_tensor_spec_from_tensor(
              test_tensor, shape_hint=tf.TensorShape([1, 4])
          )


class MakeMeasurementTest(tf.test.TestCase):

  def test_succeeds(self):
    with tf.Graph().as_default():
      tensor = tf.constant(1)
      tff_type = federated_language.TensorType(np.int32)
      m = proto_helpers.make_measurement(
          t=tensor, name="test", tff_type=tff_type
      )

      self.assertEqual(m.name, "test")
      self.assertProtoEquals(
          m.tff_type,
          tff.types.serialize_type(tff_type).SerializeToString()
      )

  def test_fails_for_non_matching_dtype(self):
    with tf.Graph().as_default():
      tensor = tf.constant(1.0)
      tff_type = federated_language.TensorType(np.int32)

      with self.assertRaisesRegex(ValueError, ".* does not match.*"):
        proto_helpers.make_measurement(t=tensor, name="test", tff_type=tff_type)

  def test_fails_for_non_matching_shape(self):
    with tf.Graph().as_default():
      tensor = tf.constant(1.0)
      tff_type = federated_language.TensorType(np.float32, shape=[5])

      with self.assertRaisesRegex(ValueError, ".* does not match.*"):
        proto_helpers.make_measurement(t=tensor, name="test", tff_type=tff_type)


if __name__ == "__main__":
  tf.test.main()
