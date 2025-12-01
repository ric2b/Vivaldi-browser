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
"""Tests for aggregations."""

import asyncio
import http
import unittest
from unittest import mock

from absl.testing import absltest
import tensorflow as tf

from fcp.demo import aggregations
from fcp.demo import http_actions
from fcp.demo import media
from fcp.demo import test_utils
from fcp.protos import plan_pb2
from fcp.protos.federatedcompute import aggregations_pb2
from fcp.protos.federatedcompute import common_pb2
from pybind11_abseil import status as absl_status
from tensorflow_federated.cc.core.impl.aggregation.protocol import aggregation_protocol_messages_pb2
from tensorflow_federated.cc.core.impl.aggregation.protocol.python import aggregation_protocol
from tensorflow_federated.cc.core.impl.aggregation.tensorflow.python import aggregation_protocols

INPUT_TENSOR = 'in'
OUTPUT_TENSOR = 'out'
AGGREGATION_REQUIREMENTS = aggregations.AggregationRequirements(
    minimum_clients_in_server_published_aggregate=3,
    plan=plan_pb2.Plan(phase=[
        plan_pb2.Plan.Phase(
            server_phase_v2=plan_pb2.ServerPhaseV2(aggregations=[
                plan_pb2.ServerAggregationConfig(
                    intrinsic_uri='federated_sum',
                    intrinsic_args=[
                        plan_pb2.ServerAggregationConfig.IntrinsicArg(
                            input_tensor=tf.TensorSpec((
                            ), tf.int32, INPUT_TENSOR).experimental_as_proto())
                    ],
                    output_tensors=[
                        tf.TensorSpec((
                        ), tf.int32, OUTPUT_TENSOR).experimental_as_proto(),
                    ]),
            ])),
    ]))
FORWARDING_INFO = common_pb2.ForwardingInfo(
    target_uri_prefix='https://forwarding.example/')


class NotOkStatus:
  """Matcher for a not-ok Status."""

  def __eq__(self, other) -> bool:
    return isinstance(other, absl_status.Status) and not other.ok()


class AggregationsTest(absltest.TestCase, unittest.IsolatedAsyncioTestCase):

  def setUp(self):
    super().setUp()
    self.mock_media_service = self.enter_context(
        mock.patch.object(media, 'Service', autospec=True))
    self.mock_media_service.register_upload.return_value = 'upload-id'
    self.mock_media_service.finalize_upload.return_value = (
        test_utils.create_checkpoint({INPUT_TENSOR: 0}))

  def test_pre_authorize_clients(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 3)
    self.assertLen(tokens, 3)
    # The tokens should all be unique.
    self.assertLen(set(tokens), 3)

  def test_pre_authorize_clients_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    with self.assertRaises(KeyError):
      service.pre_authorize_clients('does-not-exist', 1)

  def test_pre_authorize_clients_with_bad_count(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    self.assertEmpty(service.pre_authorize_clients(session_id, 0))
    self.assertEmpty(service.pre_authorize_clients(session_id, -2))

  def test_create_session(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING))

  def test_complete_session(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)

    # Upload results from the client.
    num_clients = (
        AGGREGATION_REQUIREMENTS.minimum_clients_in_server_published_aggregate)
    for i in range(num_clients):
      tokens = service.pre_authorize_clients(session_id, 1)

      self.mock_media_service.register_upload.return_value = f'upload-{i}'
      operation = service.start_aggregation_data_upload(
          aggregations_pb2.StartAggregationDataUploadRequest(
              aggregation_id=session_id, authorization_token=tokens[0]))
      self.assertTrue(operation.done)
      start_upload_response = (
          aggregations_pb2.StartAggregationDataUploadResponse())
      operation.response.Unpack(start_upload_response)

      self.mock_media_service.finalize_upload.return_value = (
          test_utils.create_checkpoint({INPUT_TENSOR: i}))
      service.submit_aggregation_result(
          aggregations_pb2.SubmitAggregationResultRequest(
              aggregation_id=session_id,
              client_token=start_upload_response.client_token,
              resource_name=start_upload_response.resource.resource_name))

    # Now that all clients have contributed, the aggregation session can be
    # completed.
    status, aggregate = service.complete_session(session_id)
    self.assertEqual(
        status,
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.COMPLETED,
            num_clients_completed=num_clients,
            num_inputs_aggregated_and_included=num_clients))
    self.assertEqual(
        test_utils.read_tensor_from_checkpoint(aggregate,
                                               OUTPUT_TENSOR, tf.int32),
        sum(range(num_clients)))

    # get_session_status should no longer return results.
    with self.assertRaises(KeyError):
      service.get_session_status(session_id)

  @mock.patch.object(
      aggregation_protocols,
      'create_simple_aggregation_protocol',
      autospec=True)
  def test_complete_session_fails(self, mock_create_simple_agg_protocol):
    # Use a mock since it's not easy to cause
    # SimpleAggregationProtocol::Complete to fail.
    mock_agg_protocol = mock.create_autospec(
        aggregation_protocol.AggregationProtocol, instance=True)
    mock_create_simple_agg_protocol.return_value = mock_agg_protocol

    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)

    required_clients = (
        AGGREGATION_REQUIREMENTS.minimum_clients_in_server_published_aggregate)
    agg_status = aggregation_protocol_messages_pb2.StatusMessage(
        num_inputs_aggregated_and_included=required_clients
    )
    mock_agg_protocol.GetStatus.side_effect = lambda: agg_status

    def on_complete():
      agg_status.num_inputs_discarded = (
          agg_status.num_inputs_aggregated_and_included)
      agg_status.num_inputs_aggregated_and_included = 0
      raise absl_status.StatusNotOk(absl_status.unknown_error('message'))

    mock_agg_protocol.Complete.side_effect = on_complete

    status, aggregate = service.complete_session(session_id)
    self.assertEqual(
        status,
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.FAILED,
            num_inputs_discarded=required_clients))
    self.assertIsNone(aggregate)
    mock_agg_protocol.Complete.assert_called_once()
    mock_agg_protocol.Abort.assert_not_called()

  def test_complete_session_without_enough_inputs(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(
        aggregations.AggregationRequirements(
            minimum_clients_in_server_published_aggregate=3,
            plan=AGGREGATION_REQUIREMENTS.plan))
    tokens = service.pre_authorize_clients(session_id, 2)

    # Upload results for one client.
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)
    service.submit_aggregation_result(
        aggregations_pb2.SubmitAggregationResultRequest(
            aggregation_id=session_id,
            client_token=start_upload_response.client_token,
            resource_name=start_upload_response.resource.resource_name))

    # Complete the session before there are 2 completed clients.
    status, aggregate = service.complete_session(session_id)
    self.assertEqual(
        status,
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.FAILED,
            num_clients_completed=1,
            num_inputs_discarded=1))
    self.assertIsNone(aggregate)

  def test_complete_session_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    with self.assertRaises(KeyError):
      service.complete_session('does-not-exist')

  def test_abort_session_with_no_uploads(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    self.assertEqual(
        service.abort_session(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.ABORTED))

  def test_abort_session_with_uploads(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 3)

    # Upload results for one client.
    self.mock_media_service.register_upload.return_value = 'upload1'
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)
    service.submit_aggregation_result(
        aggregations_pb2.SubmitAggregationResultRequest(
            aggregation_id=session_id,
            client_token=start_upload_response.client_token,
            resource_name=start_upload_response.resource.resource_name))

    # Start a partial upload from a second client.
    self.mock_media_service.register_upload.return_value = 'upload2'
    service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[1]))

    # Abort the session. The pending client should be treated as failed.
    self.assertEqual(
        service.abort_session(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.ABORTED,
            num_clients_completed=1,
            num_clients_aborted=1,
            num_inputs_discarded=1))
    # The registered upload for the second client should have been finalized.
    self.mock_media_service.finalize_upload.assert_called_with('upload2')
    # get_session_status should no longer return results.
    with self.assertRaises(KeyError):
      service.get_session_status(session_id)

  def test_abort_session_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    with self.assertRaises(KeyError):
      service.abort_session('does-not-exist')

  def test_get_session_status_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    with self.assertRaises(KeyError):
      service.get_session_status('does-not-exist')

  async def test_wait(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    task = asyncio.create_task(
        service.wait(session_id, num_inputs_aggregated_and_included=1))
    # The awaitable should not be done yet.
    await asyncio.wait([task], timeout=0.1)
    self.assertFalse(task.done())

    # Upload results for one client.
    tokens = service.pre_authorize_clients(session_id, 1)
    self.mock_media_service.register_upload.return_value = 'upload'
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)
    service.submit_aggregation_result(
        aggregations_pb2.SubmitAggregationResultRequest(
            aggregation_id=session_id,
            client_token=start_upload_response.client_token,
            resource_name=start_upload_response.resource.resource_name))

    # The awaitable should now return.
    await asyncio.wait([task], timeout=1)
    self.assertTrue(task.done())
    self.assertEqual(
        task.result(),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_completed=1,
            num_inputs_aggregated_and_included=1))

  async def test_wait_already_satisfied(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)

    # Upload results for one client.
    tokens = service.pre_authorize_clients(session_id, 1)
    self.mock_media_service.register_upload.return_value = 'upload'
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)
    service.submit_aggregation_result(
        aggregations_pb2.SubmitAggregationResultRequest(
            aggregation_id=session_id,
            client_token=start_upload_response.client_token,
            resource_name=start_upload_response.resource.resource_name))

    # Since a client has already reported, the condition should already be
    # satisfied.
    task = asyncio.create_task(
        service.wait(session_id, num_inputs_aggregated_and_included=1))
    await asyncio.wait([task], timeout=1)
    self.assertTrue(task.done())
    self.assertEqual(
        task.result(),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_completed=1,
            num_inputs_aggregated_and_included=1))

  async def test_wait_with_abort(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    task = asyncio.create_task(
        service.wait(session_id, num_inputs_aggregated_and_included=1))
    # The awaitable should not be done yet.
    await asyncio.wait([task], timeout=0.1)
    self.assertFalse(task.done())

    # The awaitable should return once the session is aborted.
    status = service.abort_session(session_id)
    await asyncio.wait([task], timeout=1)
    self.assertTrue(task.done())
    self.assertEqual(task.result(), status)

  async def test_wait_with_complete(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(
        aggregations.AggregationRequirements(
            minimum_clients_in_server_published_aggregate=0,
            plan=AGGREGATION_REQUIREMENTS.plan))
    task = asyncio.create_task(
        service.wait(session_id, num_inputs_aggregated_and_included=1))
    # The awaitable should not be done yet.
    await asyncio.wait([task], timeout=0.1)
    self.assertFalse(task.done())

    # The awaitable should return once the session is completed.
    status, _ = service.complete_session(session_id)
    await asyncio.wait([task], timeout=1)
    self.assertTrue(task.done())
    self.assertEqual(task.result(), status)

  async def test_wait_without_condition(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    task = asyncio.create_task(service.wait(session_id))
    # If there are no conditions, the wait should be trivially satisfied.
    await asyncio.wait([task], timeout=1)
    self.assertTrue(task.done())
    self.assertEqual(task.result(), service.get_session_status(session_id))

  async def test_wait_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    task = asyncio.create_task(service.wait('does-not-exist'))
    await asyncio.wait([task], timeout=1)
    self.assertTrue(task.done())
    self.assertIsInstance(task.exception(), KeyError)

  def test_start_aggregation_data_upload(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    self.mock_media_service.register_upload.return_value = 'upload'
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertNotEmpty(operation.name)
    self.assertTrue(operation.done)

    metadata = aggregations_pb2.StartAggregationDataUploadMetadata()
    operation.metadata.Unpack(metadata)
    self.assertEqual(metadata,
                     aggregations_pb2.StartAggregationDataUploadMetadata())

    response = aggregations_pb2.StartAggregationDataUploadResponse()
    operation.response.Unpack(response)
    # The client token should be set and different from the authorization token.
    self.assertNotEmpty(response.client_token)
    self.assertNotEqual(response.client_token, tokens[0])
    self.assertEqual(
        response,
        aggregations_pb2.StartAggregationDataUploadResponse(
            aggregation_protocol_forwarding_info=FORWARDING_INFO,
            resource=common_pb2.ByteStreamResource(
                data_upload_forwarding_info=FORWARDING_INFO,
                resource_name='upload'),
            client_token=response.client_token))
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_pending=1))

  def test_start_aggregagation_data_upload_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.start_aggregation_data_upload(
          aggregations_pb2.StartAggregationDataUploadRequest(
              aggregation_id='does-not-exist', authorization_token=tokens[0]))
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING))

  def test_start_aggregagation_data_upload_with_invalid_token(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.start_aggregation_data_upload(
          aggregations_pb2.StartAggregationDataUploadRequest(
              aggregation_id=session_id, authorization_token='does-not-exist'))
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING))

  def test_start_aggregagation_data_upload_twice(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    with self.assertRaises(http_actions.HttpError) as cm:
      service.start_aggregation_data_upload(
          aggregations_pb2.StartAggregationDataUploadRequest(
              aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)

  def test_submit_aggregation_result(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)

    # Upload results from the client.
    tokens = service.pre_authorize_clients(session_id, 1)

    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)

    submit_response = service.submit_aggregation_result(
        aggregations_pb2.SubmitAggregationResultRequest(
            aggregation_id=session_id,
            client_token=start_upload_response.client_token,
            resource_name=start_upload_response.resource.resource_name))
    self.assertEqual(submit_response,
                     aggregations_pb2.SubmitAggregationResultResponse())
    self.mock_media_service.finalize_upload.assert_called_with(
        start_upload_response.resource.resource_name)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_completed=1,
            num_inputs_aggregated_and_included=1))

  def test_submit_aggregation_result_with_invalid_client_input(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)

    tokens = service.pre_authorize_clients(session_id, 1)
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)

    self.mock_media_service.finalize_upload.return_value = b'invalid'
    with self.assertRaises(http_actions.HttpError):
      service.submit_aggregation_result(
          aggregations_pb2.SubmitAggregationResultRequest(
              aggregation_id=session_id,
              client_token=start_upload_response.client_token,
              resource_name=start_upload_response.resource.resource_name))
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_failed=1))

  def test_submit_aggregation_result_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.submit_aggregation_result(
          aggregations_pb2.SubmitAggregationResultRequest(
              aggregation_id='does-not-exist',
              client_token='client-token',
              resource_name='upload-id'))
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING))

  def test_submit_aggregation_result_with_invalid_token(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.submit_aggregation_result(
          aggregations_pb2.SubmitAggregationResultRequest(
              aggregation_id=session_id,
              client_token='does-not-exist',
              resource_name='upload-id'))
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING))

  def test_submit_aggregation_result_with_finalize_upload_error(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)

    # If the resource_name doesn't correspond to a registered upload,
    # finalize_upload will raise a KeyError.
    self.mock_media_service.finalize_upload.side_effect = KeyError()
    with self.assertRaises(http_actions.HttpError) as cm:
      service.submit_aggregation_result(
          aggregations_pb2.SubmitAggregationResultRequest(
              aggregation_id=session_id,
              client_token=start_upload_response.client_token,
              resource_name=start_upload_response.resource.resource_name))
    self.assertEqual(cm.exception.code, http.HTTPStatus.INTERNAL_SERVER_ERROR)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_failed=1))

  def test_submit_aggregation_result_with_unuploaded_resource(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)

    # If the resource_name is valid but no resource was uploaded,
    # finalize_resource will return None.
    self.mock_media_service.finalize_upload.return_value = None
    with self.assertRaises(http_actions.HttpError) as cm:
      service.submit_aggregation_result(
          aggregations_pb2.SubmitAggregationResultRequest(
              aggregation_id=session_id,
              client_token=start_upload_response.client_token,
              resource_name=start_upload_response.resource.resource_name))
    self.assertEqual(cm.exception.code, http.HTTPStatus.BAD_REQUEST)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_failed=1))

  def test_abort_aggregation(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)
    self.assertEqual(
        service.abort_aggregation(
            aggregations_pb2.AbortAggregationRequest(
                aggregation_id=session_id,
                client_token=start_upload_response.client_token)),
        aggregations_pb2.AbortAggregationResponse())
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_completed=1,
            num_inputs_discarded=1))

  def test_abort_aggregation_with_missing_session_id(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    tokens = service.pre_authorize_clients(session_id, 1)
    operation = service.start_aggregation_data_upload(
        aggregations_pb2.StartAggregationDataUploadRequest(
            aggregation_id=session_id, authorization_token=tokens[0]))
    self.assertTrue(operation.done)
    start_upload_response = (
        aggregations_pb2.StartAggregationDataUploadResponse())
    operation.response.Unpack(start_upload_response)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.abort_aggregation(
          aggregations_pb2.AbortAggregationRequest(
              aggregation_id='does-not-exist',
              client_token=start_upload_response.client_token))
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING,
            num_clients_pending=1))

  def test_abort_aggregation_with_invalid_token(self):
    service = aggregations.Service(lambda: FORWARDING_INFO,
                                   self.mock_media_service)
    session_id = service.create_session(AGGREGATION_REQUIREMENTS)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.abort_aggregation(
          aggregations_pb2.AbortAggregationRequest(
              aggregation_id=session_id, client_token='does-not-exist'))
    self.assertEqual(cm.exception.code, http.HTTPStatus.UNAUTHORIZED)
    self.assertEqual(
        service.get_session_status(session_id),
        aggregations.SessionStatus(
            status=aggregations.AggregationStatus.PENDING))


if __name__ == '__main__':
  absltest.main()
