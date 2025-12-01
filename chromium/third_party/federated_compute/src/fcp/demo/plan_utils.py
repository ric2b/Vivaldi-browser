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
"""Utilities for working with Plan protos and TensorFlow.

See the field comments in plan.proto for more information about each operation
and when it should be run.
"""

import functools
import os
import tempfile

import tensorflow as tf

from google.protobuf import message
from fcp.protos import plan_pb2


class Session:
  """A helper class for executing ServerPhaseV2."""

  def __init__(self, plan: plan_pb2.Plan, server_checkpoint: bytes):
    if len(plan.phase) != 1:
      raise ValueError('plan must contain exactly 1 phase.')
    if not plan.phase[0].HasField('server_phase_v2'):
      raise ValueError('plan.phase[0] is missing ServerPhaseV2.')
    self._plan = plan
    self._server_phase_v2 = plan.phase[0].server_phase_v2

    self._server_prepare_graph_def = None
    if plan.HasField('server_graph_prepare_bytes'):
      self._server_prepare_graph_def = tf.compat.v1.GraphDef()
      try:
        plan.server_graph_prepare_bytes.Unpack(self._server_prepare_graph_def)
      except message.DecodeError as e:
        raise ValueError('Unable to parse server_prepare graph.') from e

    self._server_result_graph_def = None
    if plan.HasField('server_graph_result_bytes'):
      self._server_result_graph_def = tf.compat.v1.GraphDef()
      try:
        plan.server_graph_result_bytes.Unpack(self._server_result_graph_def)
      except message.DecodeError as e:
        raise ValueError('Unable to parse server_result graph.') from e

    self._temp_dir = tempfile.TemporaryDirectory()
    server_state_checkpoint_filename = os.path.join(
        self._temp_dir.name, 'server_state.ckpt'
    )
    self._client_checkpoint_filename = os.path.join(
        self._temp_dir.name, 'client_init.ckpt'
    )
    self._intermediate_state_checkpoint_filename = os.path.join(
        self._temp_dir.name, 'intermediate_state.ckpt'
    )

    # Run server prepare logic (if it exists) to generate the intermediate state
    # checkpoint and client checkpoint.
    if self._server_prepare_graph_def:
      with open(server_state_checkpoint_filename, 'wb') as f:
        f.write(server_checkpoint)
      graph = tf.Graph()
      with graph.as_default():
        tf.import_graph_def(self._server_prepare_graph_def, name='')
      with tf.compat.v1.Session(graph=graph) as sess:
        prepare_router = self._server_phase_v2.prepare_router
        feed_dict = {
            prepare_router.prepare_server_state_input_filepath_tensor_name: (
                server_state_checkpoint_filename
            ),
            prepare_router.prepare_output_filepath_tensor_name: (
                self._client_checkpoint_filename
            ),
            prepare_router.prepare_intermediate_state_output_filepath_tensor_name: (
                self._intermediate_state_checkpoint_filename
            ),
        }
        sess.run(
            fetches=list(
                self._server_phase_v2.tensorflow_spec_prepare.target_node_names
            ),
            feed_dict=feed_dict,
        )

  def __enter__(self) -> 'Session':
    return self

  def __exit__(self, exc_type, exc_value, tb) -> None:
    self._temp_dir.cleanup()

  @functools.cached_property
  def client_plan(self) -> bytes:
    """The serialized ClientOnlyPlan corresponding to the Plan proto."""
    client_only_plan = plan_pb2.ClientOnlyPlan(
        phase=self._plan.phase[0].client_phase,
        graph=self._plan.client_graph_bytes.value,
        tflite_graph=self._plan.client_tflite_graph_bytes)
    if self._plan.HasField('tensorflow_config_proto'):
      client_only_plan.tensorflow_config_proto.CopyFrom(
          self._plan.tensorflow_config_proto)
    return client_only_plan.SerializeToString()

  @functools.cached_property
  def client_checkpoint(self) -> bytes:
    """The initial checkpoint for use by clients."""
    try:
      with open(self._client_checkpoint_filename, 'rb') as f:
        return f.read()
    except FileNotFoundError:
      return b''

  def finalize(self, aggregation_result: bytes) -> bytes:
    """Run server_result logic (if it exists)."""
    if self._server_result_graph_def is None:
      # When there is no server_result logic, the updated server state is just
      # the aggregated client results.
      return aggregation_result

    aggregation_result_checkpoint_filename = os.path.join(
        self._temp_dir.name, 'aggregation_result.ckpt'
    )
    updated_server_state_checkpoint_filename = os.path.join(
        self._temp_dir.name, 'updated_server_state.ckpt'
    )
    with open(aggregation_result_checkpoint_filename, 'wb') as f:
      f.write(aggregation_result)
    graph = tf.Graph()
    with graph.as_default():
      tf.import_graph_def(self._server_result_graph_def, name='')
    with tf.compat.v1.Session(graph=graph) as sess:
      result_router = self._server_phase_v2.result_router
      feed_dict = {
          result_router.result_intermediate_state_input_filepath_tensor_name: (
              self._intermediate_state_checkpoint_filename
          ),
          result_router.result_aggregate_result_input_filepath_tensor_name: (
              aggregation_result_checkpoint_filename
          ),
          result_router.result_server_state_output_filepath_tensor_name: (
              updated_server_state_checkpoint_filename
          ),
      }
      sess.run(
          fetches=list(
              self._server_phase_v2.tensorflow_spec_result.target_node_names
          ),
          feed_dict=feed_dict,
      )
    try:
      with open(updated_server_state_checkpoint_filename, 'rb') as f:
        return f.read()
    except FileNotFoundError:
      return b''

  @property
  def slices(self) -> dict[str, list[bytes]]:
    """The Federated Select slices, keyed by served_at_id."""
    # DistributeAggregateForm does not currently support slices.
    return {}
