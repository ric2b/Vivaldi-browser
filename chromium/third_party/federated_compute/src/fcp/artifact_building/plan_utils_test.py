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
"""Test class for plan_utils."""

import os

import tensorflow as tf

from fcp.artifact_building import checkpoint_utils
from fcp.artifact_building import plan_utils
from fcp.artifact_building import test_utils
from fcp.protos import plan_pb2


class PlanUtilsTest(tf.test.TestCase):

  def test_write_checkpoint(self):
    checkpoint_op = plan_pb2.CheckpointOp()
    graph = tf.Graph()
    with graph.as_default():
      v = tf.compat.v1.get_variable('v', initializer=tf.constant(1))
      saver = checkpoint_utils.create_deterministic_saver([v])
      test_utils.set_checkpoint_op(checkpoint_op, saver)
      init_op = v.assign(tf.constant(2))
      change_op = v.assign(tf.constant(3))

    with tf.compat.v1.Session(graph=graph) as sess:
      sess.run(init_op)
      temp_file = self.create_tempfile().full_path
      plan_utils.write_checkpoint(sess, checkpoint_op, temp_file)
      # Change the variable in this session.
      sess.run(change_op)

    with tf.compat.v1.Session(graph=graph) as sess:
      saver.restore(sess, temp_file)
      # Should not see update to 3.
      self.assertEqual(2, sess.run(v))

  def test_write_checkpoint_not_checkpoint_op(self):
    with self.assertRaises(ValueError):
      plan_utils.write_checkpoint(None, 'not_checkpoint_op', None)

  def test_write_checkpoint_skips_when_no_saver_def(self):
    checkpoint_op = plan_pb2.CheckpointOp()
    with tf.compat.v1.Session() as sess:
      temp_file = self.create_tempfile().full_path
      # Close deletes the file, we just want a good name.
      os.remove(temp_file)
      plan_utils.write_checkpoint(sess, checkpoint_op, temp_file)
      self.assertFalse(os.path.isfile(temp_file))

  def test_read_checkpoint(self):
    checkpoint_op = plan_pb2.CheckpointOp()
    graph = tf.Graph()
    with graph.as_default():
      v = tf.compat.v1.get_variable('v', initializer=tf.constant(1))
      saver = checkpoint_utils.create_deterministic_saver([v])
      test_utils.set_checkpoint_op(checkpoint_op, saver)
      init_op = v.assign(tf.constant(2))
      change_op = v.assign(tf.constant(3))

    with tf.compat.v1.Session(graph=graph) as sess:
      sess.run(init_op)
      temp_file = self.create_tempfile().full_path
      saver.save(sess, temp_file)
      sess.run(change_op)

      plan_utils.read_checkpoint(sess, checkpoint_op, temp_file)
      # Should not see update to 3.
      self.assertEqual(2, sess.run(v))

  def test_generate_and_add_tflite_model_to_plan(self):
    # Create a graph for y = x ^ 2.
    graph = tf.Graph()
    with graph.as_default():
      x = tf.compat.v1.placeholder(tf.int32, shape=[], name='x')
      _ = tf.math.pow(x, 2, name='y')
    input_tensor_spec = tf.TensorSpec(
        shape=tf.TensorShape([]), dtype=tf.int32, name='x:0'
    ).experimental_as_proto()
    output_tensor_spec = tf.TensorSpec(
        shape=tf.TensorShape([]), dtype=tf.int32, name='y:0'
    ).experimental_as_proto()

    tensorflow_spec = plan_pb2.TensorflowSpec()
    tensorflow_spec.input_tensor_specs.append(input_tensor_spec)
    tensorflow_spec.output_tensor_specs.append(output_tensor_spec)

    flatbuffer = plan_utils.convert_graphdef_to_flatbuffer(
        graph.as_graph_def(), tensorflow_spec
    )

    interpreter = tf.lite.Interpreter(model_content=flatbuffer)
    interpreter.allocate_tensors()
    input_data = tf.constant(3, shape=[])
    # Model has single output.
    model_output = interpreter.get_output_details()[0]
    # Model has single input.
    model_input = interpreter.get_input_details()[0]
    interpreter.set_tensor(model_input['index'], input_data)
    interpreter.invoke()
    self.assertEqual(interpreter.get_tensor(model_output['index']), 9)


class TfLiteTest(tf.test.TestCase):
  """Tests common methods related to TFLite support."""

  def test_caught_exception_in_tflite_conversion_failure_for_plan(self):
    plan = plan_pb2.Plan()
    plan.client_graph_bytes.Pack(tf.compat.v1.GraphDef())
    plan.phase.add()
    with self.assertRaisesRegex(
        RuntimeError, 'Failure during TFLite conversion'
    ):
      plan_utils.generate_and_add_flat_buffer_to_plan(plan)

  def test_caught_exception_in_tflite_conversion_failure_for_client_only_plan(
      self,
  ):
    client_only_plan = plan_pb2.ClientOnlyPlan()
    client_only_plan.graph = tf.compat.v1.GraphDef().SerializeToString()
    with self.assertRaisesRegex(
        RuntimeError, 'Failure during TFLite conversion'
    ):
      plan_utils.generate_and_add_flat_buffer_to_plan(client_only_plan)

  def _create_test_graph_with_associated_tensor_specs(self):
    # Create a graph for y = x ^ 2.
    graph = tf.Graph()
    with graph.as_default():
      x = tf.compat.v1.placeholder(tf.int32, shape=[], name='x')
      _ = tf.math.pow(x, 2, name='y')
    input_tensor_spec = tf.TensorSpec(
        shape=tf.TensorShape([]), dtype=tf.int32, name='x:0'
    ).experimental_as_proto()
    output_tensor_spec = tf.TensorSpec(
        shape=tf.TensorShape([]), dtype=tf.int32, name='y:0'
    ).experimental_as_proto()
    return graph, input_tensor_spec, output_tensor_spec

  def _assert_tflite_flatbuffer_is_equivalent_to_test_graph(self, tflite_graph):
    # Check that the generated TFLite model also is y = x ^ 2.
    self.assertNotEmpty(tflite_graph)
    interpreter = tf.lite.Interpreter(model_content=tflite_graph)
    interpreter.allocate_tensors()
    input_data = tf.constant(3, shape=[])
    # Model has single output.
    model_output = interpreter.get_output_details()[0]
    # Model has single input.
    model_input = interpreter.get_input_details()[0]
    interpreter.set_tensor(model_input['index'], input_data)
    interpreter.invoke()
    self.assertEqual(interpreter.get_tensor(model_output['index']), 9)

  def test_add_equivalent_tflite_model_to_plan(self):
    """Tests that the generated tflite model is identical to the tf.Graph."""

    graph, input_tensor_spec, output_tensor_spec = (
        self._create_test_graph_with_associated_tensor_specs()
    )

    # Create a fairly empty Plan with just the graph and the
    # TensorSpecProtos populated (since that is all that is needed for
    # conversion.)
    plan_proto = plan_pb2.Plan()
    plan_proto.client_graph_bytes.Pack(graph.as_graph_def())
    plan_proto.phase.add()
    plan_proto.phase[0].client_phase.tensorflow_spec.input_tensor_specs.append(
        input_tensor_spec
    )
    plan_proto.phase[0].client_phase.tensorflow_spec.output_tensor_specs.append(
        output_tensor_spec
    )

    # Generate the TFLite model.
    plan_after_conversion = plan_utils.generate_and_add_flat_buffer_to_plan(
        plan_proto
    )

    self.assertIsInstance(plan_after_conversion, plan_pb2.Plan)
    self.assertEqual(plan_after_conversion, plan_proto)
    self._assert_tflite_flatbuffer_is_equivalent_to_test_graph(
        plan_after_conversion.client_tflite_graph_bytes
    )

  def test_add_equivalent_tflite_model_to_client_only_plan(self):
    """Tests that the generated tflite model is identical to the tf.Graph."""

    graph, input_tensor_spec, output_tensor_spec = (
        self._create_test_graph_with_associated_tensor_specs()
    )

    # Create a fairly empty ClientOnlyPlan with just the graph and the
    # TensorSpecProtos populated (since that is all that is needed for
    # conversion.)
    client_only_plan_proto = plan_pb2.ClientOnlyPlan()
    client_only_plan_proto.graph = graph.as_graph_def().SerializeToString()
    client_only_plan_proto.phase.tensorflow_spec.input_tensor_specs.append(
        input_tensor_spec
    )
    client_only_plan_proto.phase.tensorflow_spec.output_tensor_specs.append(
        output_tensor_spec
    )

    # Generate the TFLite model.
    plan_after_conversion = plan_utils.generate_and_add_flat_buffer_to_plan(
        client_only_plan_proto
    )

    self.assertIsInstance(plan_after_conversion, plan_pb2.ClientOnlyPlan)
    self.assertEqual(plan_after_conversion, client_only_plan_proto)
    self._assert_tflite_flatbuffer_is_equivalent_to_test_graph(
        plan_after_conversion.tflite_graph
    )


if __name__ == '__main__':
  tf.test.main()
