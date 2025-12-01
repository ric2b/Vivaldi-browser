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
"""Action handlers for the Aggregations service."""

import asyncio
from collections.abc import Callable, Sequence
import contextlib
import dataclasses
import enum
import functools
import http
import queue
import threading
from typing import Optional
import uuid

from absl import logging
import tensorflow as tf

from google.longrunning import operations_pb2
from google.rpc import code_pb2
from fcp.demo import http_actions
from fcp.demo import media
from fcp.protos import plan_pb2
from fcp.protos.federatedcompute import aggregations_pb2
from fcp.protos.federatedcompute import common_pb2
from pybind11_abseil import status as absl_status
from tensorflow_federated.cc.core.impl.aggregation.core import tensor_pb2 as aggregation_tensor_pb2
from tensorflow_federated.cc.core.impl.aggregation.protocol import aggregation_protocol_messages_pb2
from tensorflow_federated.cc.core.impl.aggregation.protocol import configuration_pb2
from tensorflow_federated.cc.core.impl.aggregation.protocol.python import aggregation_protocol
from tensorflow_federated.cc.core.impl.aggregation.tensorflow.python import aggregation_protocols


class AggregationStatus(enum.Enum):
  COMPLETED = 1
  PENDING = 2
  FAILED = 3
  ABORTED = 4


@dataclasses.dataclass
class SessionStatus:
  """The status of an aggregation session."""
  # The current state of the aggregation session.
  status: AggregationStatus = AggregationStatus.PENDING
  # Number of clients that successfully started and completed the aggregation
  # upload protocol.
  num_clients_completed: int = 0
  # Number of clients that started the aggregation upload protocol but failed
  # to complete (e.g dropped out in the middle of the protocol).
  num_clients_failed: int = 0
  # Number of clients that started the aggregation upload protocol but have not
  # yet finished (either successfully or not).
  num_clients_pending: int = 0
  # Number of clients that started the aggregation protocol but were aborted by
  # the server before they could complete (e.g., if progress on the session was
  # no longer needed).
  num_clients_aborted: int = 0
  # Number of inputs that were successfully aggregated and included in the
  # final aggregate. Note that even if a client successfully completes the
  # protocol (i.e., it is included in num_clients_completed), it is not
  # guaranteed that the uploaded report is included in the final aggregate yet.
  num_inputs_aggregated_and_included: int = 0
  # Number of inputs that were received by the server and are pending (i.e.,
  # the inputs have not been included in the final aggregate yet).
  num_inputs_aggregated_and_pending: int = 0
  # Number of inputs that were received by the server but discarded.
  num_inputs_discarded: int = 0


@dataclasses.dataclass(frozen=True)
class AggregationRequirements:
  # The minimum number of clients required before a result can be released
  # outside this service. Note that aggregation does not automatically stop if
  # minimum_clients_in_server_published_aggregate is met. It is up to callers
  # to stop aggregation when they want.
  minimum_clients_in_server_published_aggregate: int
  # The Plan to execute.
  plan: plan_pb2.Plan


@dataclasses.dataclass
class _ActiveClientData:
  """Information about an active client."""
  # The client's identifier in the aggregation protocol.
  client_id: int
  # The name of the resource to which the client should write its update.
  resource_name: str


@dataclasses.dataclass(eq=False)
class _WaitData:
  """Information about a pending wait operation."""
  # The condition under which the wait should complete.
  num_inputs_aggregated_and_included: Optional[int]
  # The loop the caller is waiting on.
  loop: asyncio.AbstractEventLoop = dataclasses.field(
      default_factory=asyncio.get_running_loop)
  # The future to which the SessionStatus will be written once the wait is over.
  status_future: asyncio.Future[SessionStatus] = dataclasses.field(
      default_factory=asyncio.Future)


@dataclasses.dataclass(eq=False)
class _AggregationSessionState:
  """Internal state for an aggregation session."""
  # The session's aggregation requirements.
  requirements: AggregationRequirements
  # The protocol performing the aggregation. Service._sessions_lock should not
  # be held while AggregationProtocol methods are invoked because
  # methods may be slow.
  agg_protocol: aggregation_protocol.AggregationProtocol
  # The current status of the session.
  status: AggregationStatus = AggregationStatus.PENDING
  # Unredeemed client authorization tokens.
  authorization_tokens: set[str] = dataclasses.field(default_factory=set)
  # Information about active clients, keyed by authorization token
  active_clients: dict[str, _ActiveClientData] = dataclasses.field(
      default_factory=dict)
  # Information for in-progress wait calls on this session.
  pending_waits: set[_WaitData] = dataclasses.field(default_factory=set)


class Service:
  """Implements the Aggregations service."""

  def __init__(self, forwarding_info: Callable[[], common_pb2.ForwardingInfo],
               media_service: media.Service):
    self._forwarding_info = forwarding_info
    self._media_service = media_service
    self._sessions: dict[str, _AggregationSessionState] = {}
    self._sessions_lock = threading.Lock()

  def create_session(
      self, aggregation_requirements: AggregationRequirements
  ) -> str:
    """Creates a new aggregation session and returns its id."""
    session_id = str(uuid.uuid4())
    if (len(aggregation_requirements.plan.phase) != 1 or
        not aggregation_requirements.plan.phase[0].HasField('server_phase_v2')):
      raise ValueError('Plan must contain exactly one server_phase_v2.')
    protocol_config = configuration_pb2.Configuration(
        intrinsic_configs=[
            self._translate_server_aggregation_config(aggregation_config)
            for aggregation_config in aggregation_requirements.plan.phase[
                0
            ].server_phase_v2.aggregations
        ]
    )

    # NOTE: For simplicity, this implementation only creates a single,
    # in-process aggregation shard. In a production implementation, there
    # should be multiple shards running on separate servers to enable high
    # rates of client contributions. Utilities for combining results from
    # separate shards are still in development as of Jan 2023.

    agg_protocol = aggregation_protocols.create_simple_aggregation_protocol(
        protocol_config
    )

    # Start the aggregation protocol.
    agg_protocol.Start(0)

    with self._sessions_lock:
      self._sessions[session_id] = _AggregationSessionState(
          requirements=aggregation_requirements,
          agg_protocol=agg_protocol)
    return session_id

  def complete_session(
      self, session_id: str) -> tuple[SessionStatus, Optional[bytes]]:
    """Completes the aggregation session and returns its results."""
    with self._sessions_lock:
      state = self._sessions.pop(session_id)

    try:
      # Only complete the AggregationProtocol if it's still pending. The most
      # likely alternative is that it's ABORTED due to an error generated by the
      # protocol itself.
      status = self._get_session_status(state)
      if status.status != AggregationStatus.PENDING:
        return self._get_session_status(state), None

      # Ensure privacy requirements have been met.
      if (state.agg_protocol.GetStatus().num_inputs_aggregated_and_included <
          state.requirements.minimum_clients_in_server_published_aggregate):
        state.agg_protocol.Abort()
        raise ValueError(
            'minimum_clients_in_server_published_aggregate has not been met.')

      state.agg_protocol.Complete()
      result = state.agg_protocol.GetResult()
      if isinstance(result, absl_status.Status):
        raise absl_status.StatusNotOk(result)
      state.status = AggregationStatus.COMPLETED
      return self._get_session_status(state), result
    except (ValueError, absl_status.StatusNotOk, queue.Empty) as e:
      logging.warning('Failed to complete aggregation session: %s', e)
      state.status = AggregationStatus.FAILED
      return self._get_session_status(state), None
    finally:
      self._cleanup_session(state)

  def abort_session(self, session_id: str) -> SessionStatus:
    """Aborts/cancels an aggregation session."""
    with self._sessions_lock:
      state = self._sessions.pop(session_id)

    # Only abort the AggregationProtocol if it's still pending. The most likely
    # alternative is that it's ABORTED due to an error generated by the protocol
    # itself.
    if state.status == AggregationStatus.PENDING:
      state.status = AggregationStatus.ABORTED
      state.agg_protocol.Abort()

    self._cleanup_session(state)
    return self._get_session_status(state)

  def get_session_status(self, session_id: str) -> SessionStatus:
    """Returns the status of an aggregation session."""
    with self._sessions_lock:
      return self._get_session_status(self._sessions[session_id])

  async def wait(
      self,
      session_id: str,
      num_inputs_aggregated_and_included: Optional[int] = None
  ) -> SessionStatus:
    """Blocks until all conditions are satisfied or the aggregation fails."""
    with self._sessions_lock:
      state = self._sessions[session_id]
      # Check if any of the conditions are already satisfied.
      status = self._get_session_status(state)
      if (num_inputs_aggregated_and_included is None or
          num_inputs_aggregated_and_included <=
          status.num_inputs_aggregated_and_included):
        return status

      wait_data = _WaitData(num_inputs_aggregated_and_included)
      state.pending_waits.add(wait_data)
    return await wait_data.status_future

  def pre_authorize_clients(self, session_id: str,
                            num_tokens: int) -> Sequence[str]:
    """Generates tokens authorizing clients to contribute to the session."""
    tokens = set(str(uuid.uuid4()) for _ in range(num_tokens))
    with self._sessions_lock:
      self._sessions[session_id].authorization_tokens |= tokens
    return list(tokens)

  def _translate_tensor_spec_proto(
      self, tensor_spec_proto
  ) -> aggregation_tensor_pb2.TensorSpecProto:
    """Transform TensorSpecProto for use by the aggregation service."""
    transformed_tensor = aggregation_tensor_pb2.TensorSpecProto(
        name=tensor_spec_proto.name
    )
    transformed_tensor.shape.dim_sizes.extend(
        [dim.size for dim in tensor_spec_proto.shape.dim]
    )
    dtype = tf.dtypes.as_dtype(tensor_spec_proto.dtype)
    if dtype == tf.float32:
      transformed_tensor.dtype = aggregation_tensor_pb2.DataType.DT_FLOAT
    elif dtype == tf.double:
      transformed_tensor.dtype = aggregation_tensor_pb2.DataType.DT_DOUBLE
    elif dtype == tf.string:
      transformed_tensor.dtype = aggregation_tensor_pb2.DataType.DT_STRING
    elif dtype == tf.int32:
      transformed_tensor.dtype = aggregation_tensor_pb2.DataType.DT_INT32
    elif dtype == tf.int64:
      transformed_tensor.dtype = aggregation_tensor_pb2.DataType.DT_INT64
    elif dtype == tf.uint64:
      transformed_tensor.dtype = aggregation_tensor_pb2.DataType.DT_UINT64
    else:
      raise AssertionError('Cases should have exhausted all possible dtypes.')
    return transformed_tensor

  def _translate_intrinsic_arg(
      self, intrinsic_arg: plan_pb2.ServerAggregationConfig.IntrinsicArg
  ) -> configuration_pb2.Configuration.IntrinsicConfig.IntrinsicArg:
    """Transform an aggregation intrinsic arg for the aggregation service."""
    if intrinsic_arg.HasField('input_tensor'):
      return configuration_pb2.Configuration.IntrinsicConfig.IntrinsicArg(
          input_tensor=self._translate_tensor_spec_proto(
              intrinsic_arg.input_tensor
          )
      )
    elif intrinsic_arg.HasField('state_tensor'):
      raise ValueError(
          'Non-client intrinsic args are not supported in this demo.'
      )
    else:
      raise AssertionError(
          'Cases should have exhausted all possible types of intrinsic args.'
      )

  def _translate_server_aggregation_config(
      self, plan_aggregation_config: plan_pb2.ServerAggregationConfig
  ) -> configuration_pb2.Configuration.IntrinsicConfig:
    """Transform the aggregation config for use by the aggregation service."""
    return configuration_pb2.Configuration.IntrinsicConfig(
        intrinsic_uri=plan_aggregation_config.intrinsic_uri,
        intrinsic_args=[
            self._translate_intrinsic_arg(intrinsic_arg)
            for intrinsic_arg in plan_aggregation_config.intrinsic_args
        ],
        output_tensors=[
            self._translate_tensor_spec_proto(output_tensor)
            for output_tensor in plan_aggregation_config.output_tensors
        ],
        inner_intrinsics=[
            self._translate_server_aggregation_config(inner_aggregation)
            for inner_aggregation in plan_aggregation_config.inner_aggregations
        ],
    )

  def _get_session_status(self,
                          state: _AggregationSessionState) -> SessionStatus:
    """Returns the SessionStatus for an _AggregationSessionState object."""
    status = state.agg_protocol.GetStatus()
    return SessionStatus(
        status=state.status,
        num_clients_completed=status.num_clients_completed,
        num_clients_failed=status.num_clients_failed,
        num_clients_pending=status.num_clients_pending,
        num_clients_aborted=status.num_clients_aborted,
        num_inputs_aggregated_and_included=(
            status.num_inputs_aggregated_and_included),
        num_inputs_aggregated_and_pending=(
            status.num_inputs_aggregated_and_pending),
        num_inputs_discarded=status.num_inputs_discarded)

  def _get_http_status(self, code: absl_status.StatusCode) -> http.HTTPStatus:
    """Returns the HTTPStatus code for an absl StatusCode."""
    if (code == absl_status.StatusCode.INVALID_ARGUMENT or
        code == absl_status.StatusCode.FAILED_PRECONDITION):
      return http.HTTPStatus.BAD_REQUEST
    elif code == absl_status.StatusCode.NOT_FOUND:
      return http.HTTPStatus.NOT_FOUND
    else:
      return http.HTTPStatus.INTERNAL_SERVER_ERROR

  def _cleanup_session(self, state: _AggregationSessionState) -> None:
    """Cleans up the session and releases any resources.

    Args:
      state: The session state to clean up.
    """
    state.authorization_tokens.clear()
    for client_data in state.active_clients.values():
      self._media_service.finalize_upload(client_data.resource_name)
    state.active_clients.clear()
    # Anyone waiting on the session should be notified that it's finished.
    if state.pending_waits:
      status = self._get_session_status(state)
      for data in state.pending_waits:
        data.loop.call_soon_threadsafe(
            functools.partial(data.status_future.set_result, status))
      state.pending_waits.clear()

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.Aggregations',
      method='StartAggregationDataUpload')
  def start_aggregation_data_upload(
      self, request: aggregations_pb2.StartAggregationDataUploadRequest
  ) -> operations_pb2.Operation:
    """Handles a StartAggregationDataUpload request."""
    with self._sessions_lock:
      try:
        state = self._sessions[request.aggregation_id]
      except KeyError as e:
        raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND) from e
      try:
        state.authorization_tokens.remove(request.authorization_token)
      except KeyError as e:
        raise http_actions.HttpError(http.HTTPStatus.UNAUTHORIZED) from e

    client_id = state.agg_protocol.AddClients(1)
    client_token = str(uuid.uuid4())
    upload_name = self._media_service.register_upload()

    with self._sessions_lock:
      state.active_clients[client_token] = _ActiveClientData(
          client_id, upload_name
      )

    forwarding_info = self._forwarding_info()
    response = aggregations_pb2.StartAggregationDataUploadResponse(
        aggregation_protocol_forwarding_info=forwarding_info,
        resource=common_pb2.ByteStreamResource(
            data_upload_forwarding_info=forwarding_info,
            resource_name=upload_name),
        client_token=client_token)

    op = operations_pb2.Operation(name=f'operations/{uuid.uuid4()}', done=True)
    op.metadata.Pack(aggregations_pb2.StartAggregationDataUploadMetadata())
    op.response.Pack(response)
    return op

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.Aggregations',
      method='SubmitAggregationResult')
  def submit_aggregation_result(
      self, request: aggregations_pb2.SubmitAggregationResultRequest
  ) -> aggregations_pb2.SubmitAggregationResultResponse:
    """Handles a SubmitAggregationResult request."""
    with self._sessions_lock:
      try:
        state = self._sessions[request.aggregation_id]
      except KeyError as e:
        raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND) from e
      try:
        client_data = state.active_clients.pop(request.client_token)
      except KeyError as e:
        raise http_actions.HttpError(http.HTTPStatus.UNAUTHORIZED) from e

    # Ensure the client is using the resource name provided when they called
    # StartAggregationDataUpload.
    if request.resource_name != client_data.resource_name:
      raise http_actions.HttpError(http.HTTPStatus.BAD_REQUEST)

    # The aggregation protocol may have already closed the connect (e.g., if
    # an error occurred). If so, clean up the upload and return the error.
    close_message = state.agg_protocol.PollServerMessage(client_data.client_id)
    if close_message:
      with contextlib.suppress(KeyError):
        self._media_service.finalize_upload(request.resource_name)
      raise http_actions.HttpError(
          self._get_http_status(
              close_message.simple_aggregation.close_message.code
          )
      )

    # Finalize the upload.
    try:
      update = self._media_service.finalize_upload(request.resource_name)
      if update is None:
        raise absl_status.StatusNotOk(
            absl_status.invalid_argument_error(
                'Aggregation result never uploaded'))
    except (KeyError, absl_status.StatusNotOk) as e:
      if isinstance(e, KeyError):
        e = absl_status.StatusNotOk(
            absl_status.internal_error('Failed to finalize upload'))
      state.agg_protocol.CloseClient(
          client_data.client_id, e.status.message(), e.status.raw_code()
      )
      raise http_actions.HttpError(self._get_http_status(
          e.status.code())) from e

    simple_aggregation = (
        aggregation_protocol_messages_pb2.ClientMessage.SimpleAggregation(
            input=aggregation_protocol_messages_pb2.ClientResource(
                inline_bytes=update
            )
        )
    )
    client_message = aggregation_protocol_messages_pb2.ClientMessage(
        simple_aggregation=simple_aggregation
    )
    try:
      state.agg_protocol.ReceiveClientMessage(client_data.client_id,
                                              client_message)
    except absl_status.StatusNotOk as e:
      # ReceiveClientInput should only fail if the AggregationProtocol is in a
      # bad state -- likely leading to it being aborted.
      logging.warning('Failed to receive client input: %s', e)
      raise http_actions.HttpError(http.HTTPStatus.INTERNAL_SERVER_ERROR) from e

    # Poll the close status of the client.
    close_status = state.agg_protocol.PollServerMessage(
        client_data.client_id
    ).simple_aggregation.close_message.code
    if close_status != absl_status.StatusCode.OK.value:
      raise http_actions.HttpError(self._get_http_status(close_status))

    # Check for any newly-satisfied pending wait operations.
    with self._sessions_lock:
      if state.pending_waits:
        completed_waits = set()
        status = self._get_session_status(state)
        for data in state.pending_waits:
          if (data.num_inputs_aggregated_and_included is not None and
              status.num_inputs_aggregated_and_included >=
              data.num_inputs_aggregated_and_included):
            data.loop.call_soon_threadsafe(
                functools.partial(data.status_future.set_result, status))
            completed_waits.add(data)
        state.pending_waits -= completed_waits
    return aggregations_pb2.SubmitAggregationResultResponse()

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.Aggregations',
      method='AbortAggregation')
  def abort_aggregation(
      self, request: aggregations_pb2.AbortAggregationRequest
  ) -> aggregations_pb2.AbortAggregationResponse:
    """Handles an AbortAggregation request."""
    with self._sessions_lock:
      try:
        state = self._sessions[request.aggregation_id]
      except KeyError as e:
        raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND) from e
      try:
        client_data = state.active_clients.pop(request.client_token)
      except KeyError as e:
        raise http_actions.HttpError(http.HTTPStatus.UNAUTHORIZED) from e

    # Attempt to finalize the in-progress upload to free up resources.
    with contextlib.suppress(KeyError):
      self._media_service.finalize_upload(client_data.resource_name)

    # Notify the aggregation protocol that the client has left.
    if request.status.code == code_pb2.Code.OK:
      status = absl_status.Status.OkStatus()
    else:
      status = absl_status.Status(
          absl_status.StatusCodeFromInt(request.status.code),
          request.status.message,
      )

    state.agg_protocol.CloseClient(
        client_data.client_id, status.message(), status.raw_code()
    )

    logging.debug('[%s] AbortAggregation: %s', request.aggregation_id,
                  request.status)
    return aggregations_pb2.AbortAggregationResponse()
