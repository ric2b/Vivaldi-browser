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
"""Tests for task_assignments."""

import http
from unittest import mock
import uuid

from absl.testing import absltest

from google.rpc import code_pb2
from fcp.demo import aggregations
from fcp.demo import http_actions
from fcp.demo import task_assignments
from fcp.protos import population_eligibility_spec_pb2
from fcp.protos.federatedcompute import common_pb2
from fcp.protos.federatedcompute import task_assignments_pb2

_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)

POPULATION_NAME = 'test/population'
FORWARDING_INFO = common_pb2.ForwardingInfo(
    target_uri_prefix='https://forwarding.example/')


class TaskAssignmentsTest(absltest.TestCase):

  def setUp(self):
    super().setUp()
    self.mock_aggregations_service = self.enter_context(
        mock.patch.object(aggregations, 'Service', autospec=True))
    self.mock_aggregations_service.pre_authorize_clients.return_value = ['']

  def test_start_task_assignment_with_wrong_population(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)
    request = task_assignments_pb2.StartTaskAssignmentRequest(
        population_name='other/population', session_id='session-id')
    with self.assertRaises(http_actions.HttpError) as cm:
      service.start_task_assignment(request)
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  @mock.patch.object(uuid, 'uuid4', return_value=uuid.uuid4(), autospec=True)
  def test_start_task_assignment_with_no_tasks(self, mock_uuid):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)
    request = task_assignments_pb2.StartTaskAssignmentRequest(
        population_name=POPULATION_NAME, session_id='session-id')
    operation = service.start_task_assignment(request)
    self.assertEqual(operation.name, f'operations/{mock_uuid.return_value}')
    self.assertTrue(operation.done)

    metadata = task_assignments_pb2.StartTaskAssignmentMetadata()
    operation.metadata.Unpack(metadata)
    self.assertEqual(metadata,
                     task_assignments_pb2.StartTaskAssignmentMetadata())

    response = task_assignments_pb2.StartTaskAssignmentResponse()
    operation.response.Unpack(response)
    self.assertEqual(
        response,
        task_assignments_pb2.StartTaskAssignmentResponse(
            rejection_info=common_pb2.RejectionInfo()
        ),
    )

  def test_start_task_assignment_with_multiple_assignment_task(self):
    service = task_assignments.Service(
        POPULATION_NAME, lambda: FORWARDING_INFO, self.mock_aggregations_service
    )
    service.add_task(
        'task',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE,
        'aggregation-session',
        common_pb2.Resource(uri='https://task.example/plan'),
        common_pb2.Resource(uri='https://task.example/checkpoint'),
        'https://task.example/{key_base10}',
    )

    request = task_assignments_pb2.StartTaskAssignmentRequest(
        population_name=POPULATION_NAME, session_id='session-id'
    )
    operation = service.start_task_assignment(request)
    self.assertTrue(operation.done)

    response = task_assignments_pb2.StartTaskAssignmentResponse()
    operation.response.Unpack(response)
    self.assertEqual(
        response,
        task_assignments_pb2.StartTaskAssignmentResponse(
            rejection_info=common_pb2.RejectionInfo()))

  @mock.patch.object(uuid, 'uuid4', return_value=uuid.uuid4(), autospec=True)
  def test_start_task_assignment_with_one_task(self, mock_uuid):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)

    self.mock_aggregations_service.pre_authorize_clients.return_value = [
        'token'
    ]

    task_plan = common_pb2.Resource(uri='https://task.example/plan')
    task_checkpoint = common_pb2.Resource(uri='https://task.example/checkpoint')
    task_federated_select_uri_template = 'https://task.example/{key_base10}'
    service.add_task(
        'task',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE,
        'aggregation-session',
        task_plan,
        task_checkpoint,
        task_federated_select_uri_template,
    )

    request = task_assignments_pb2.StartTaskAssignmentRequest(
        population_name=POPULATION_NAME, session_id='session-id')
    operation = service.start_task_assignment(request)
    self.assertEqual(operation.name, f'operations/{mock_uuid.return_value}')
    self.assertTrue(operation.done)

    metadata = task_assignments_pb2.StartTaskAssignmentMetadata()
    operation.metadata.Unpack(metadata)
    self.assertEqual(metadata,
                     task_assignments_pb2.StartTaskAssignmentMetadata())

    response = task_assignments_pb2.StartTaskAssignmentResponse()
    operation.response.Unpack(response)
    self.assertEqual(
        response,
        task_assignments_pb2.StartTaskAssignmentResponse(
            task_assignment=task_assignments_pb2.TaskAssignment(
                aggregation_data_forwarding_info=FORWARDING_INFO,
                aggregation_info=(
                    task_assignments_pb2.TaskAssignment.AggregationInfo()
                ),
                session_id=request.session_id,
                aggregation_id='aggregation-session',
                authorization_token='token',
                task_name='task',
                plan=task_plan,
                init_checkpoint=task_checkpoint,
                federated_select_uri_info=(
                    task_assignments_pb2.FederatedSelectUriInfo(
                        uri_template=task_federated_select_uri_template
                    )
                ),
            )
        ),
    )

    self.mock_aggregations_service.pre_authorize_clients.assert_called_once_with(
        'aggregation-session', num_tokens=1)

  def test_start_task_assignment_with_multiple_tasks(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)

    self.mock_aggregations_service.pre_authorize_clients.return_value = [
        'token'
    ]

    task1_plan = common_pb2.Resource(uri='https://task1.example/plan')
    task1_checkpoint = common_pb2.Resource(
        uri='https://task1.example/checkpoint')
    task1_federated_select_uri_template = 'https://task1.example/{key_base10}'
    service.add_task(
        'task1',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE,
        'aggregation-session1',
        task1_plan,
        task1_checkpoint,
        task1_federated_select_uri_template,
    )
    task2_plan = common_pb2.Resource(uri='https://task2.example/plan')
    task2_checkpoint = common_pb2.Resource(
        uri='https://task2.example/checkpoint')
    task2_federated_select_uri_template = 'https://task2.example/{key_base10}'
    service.add_task(
        'task2',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE,
        'aggregation-session2',
        task2_plan,
        task2_checkpoint,
        task2_federated_select_uri_template,
    )

    request = task_assignments_pb2.StartTaskAssignmentRequest(
        population_name=POPULATION_NAME, session_id='session-id')

    # Initially, task1 should be used.
    operation = service.start_task_assignment(request)
    response = task_assignments_pb2.StartTaskAssignmentResponse()
    operation.response.Unpack(response)
    self.assertEqual(
        response,
        task_assignments_pb2.StartTaskAssignmentResponse(
            task_assignment=task_assignments_pb2.TaskAssignment(
                aggregation_data_forwarding_info=FORWARDING_INFO,
                aggregation_info=(
                    task_assignments_pb2.TaskAssignment.AggregationInfo()
                ),
                session_id=request.session_id,
                aggregation_id='aggregation-session1',
                authorization_token='token',
                task_name='task1',
                plan=task1_plan,
                init_checkpoint=task1_checkpoint,
                federated_select_uri_info=(
                    task_assignments_pb2.FederatedSelectUriInfo(
                        uri_template=task1_federated_select_uri_template
                    )
                ),
            )
        ),
    )
    self.mock_aggregations_service.pre_authorize_clients.assert_called_with(
        'aggregation-session1', num_tokens=1)

    # After task1 is removed, task2 should be used.
    service.remove_task('aggregation-session1')
    operation = service.start_task_assignment(request)
    response = task_assignments_pb2.StartTaskAssignmentResponse()
    operation.response.Unpack(response)
    self.assertEqual(
        response,
        task_assignments_pb2.StartTaskAssignmentResponse(
            task_assignment=task_assignments_pb2.TaskAssignment(
                aggregation_data_forwarding_info=FORWARDING_INFO,
                aggregation_info=(
                    task_assignments_pb2.TaskAssignment.AggregationInfo()
                ),
                session_id=request.session_id,
                aggregation_id='aggregation-session2',
                authorization_token='token',
                task_name='task2',
                plan=task2_plan,
                init_checkpoint=task2_checkpoint,
                federated_select_uri_info=(
                    task_assignments_pb2.FederatedSelectUriInfo(
                        uri_template=task2_federated_select_uri_template
                    )
                ),
            )
        ),
    )
    self.mock_aggregations_service.pre_authorize_clients.assert_called_with(
        'aggregation-session2', num_tokens=1)

    # After task2 is removed, the client should be rejected.
    service.remove_task('aggregation-session2')
    operation = service.start_task_assignment(request)
    response = task_assignments_pb2.StartTaskAssignmentResponse()
    operation.response.Unpack(response)
    self.assertEqual(
        response,
        task_assignments_pb2.StartTaskAssignmentResponse(
            rejection_info=common_pb2.RejectionInfo()))

  def test_perform_multiple_task_assignments_with_wrong_population(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)
    request = task_assignments_pb2.PerformMultipleTaskAssignmentsRequest(
        population_name='other/population',
        session_id='session-id',
        task_names=['task1', 'task2', 'task3'])
    with self.assertRaises(http_actions.HttpError) as cm:
      service.perform_multiple_task_assignments(request)
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  def test_perform_multiple_task_assignments_without_tasks(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)

    request = task_assignments_pb2.PerformMultipleTaskAssignmentsRequest(
        population_name=POPULATION_NAME,
        session_id='session-id',
        task_names=['task1', 'task2', 'task3'])
    self.assertEqual(
        service.perform_multiple_task_assignments(request),
        task_assignments_pb2.PerformMultipleTaskAssignmentsResponse())

  def test_perform_multiple_task_assignments_with_multiple_tasks(self):
    self.mock_aggregations_service.pre_authorize_clients.side_effect = (
        lambda session_id, num_tokens=1: [f'token-for-{session_id}'])
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)

    task1_plan = common_pb2.Resource(uri='https://task1.example/plan')
    task1_checkpoint = common_pb2.Resource(
        uri='https://task1.example/checkpoint')
    task1_federated_select_uri_template = 'https://task1.example/{key_base10}'
    service.add_task(
        'task1',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE,
        'aggregation-session1',
        task1_plan,
        task1_checkpoint,
        task1_federated_select_uri_template,
    )
    task2_plan = common_pb2.Resource(uri='https://task2.example/plan')
    task2_checkpoint = common_pb2.Resource(
        uri='https://task2.example/checkpoint')
    task2_federated_select_uri_template = 'https://task2.example/{key_base10}'
    service.add_task(
        'task2',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE,
        'aggregation-session2',
        task2_plan,
        task2_checkpoint,
        task2_federated_select_uri_template,
    )
    # Tasks using other TaskAssignmentModes should be skipped.
    task3_plan = common_pb2.Resource(uri='https://task3.example/plan')
    task3_checkpoint = common_pb2.Resource(
        uri='https://task3.example/checkpoint'
    )
    task3_federated_select_uri_template = 'https://task3.example/{key_base10}'
    service.add_task(
        'task3',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE,
        'aggregation-session3',
        task3_plan,
        task3_checkpoint,
        task3_federated_select_uri_template,
    )

    request = task_assignments_pb2.PerformMultipleTaskAssignmentsRequest(
        population_name=POPULATION_NAME,
        session_id='session-id',
        task_names=['task1', 'task2', 'task3'])
    self.assertCountEqual(
        service.perform_multiple_task_assignments(request).task_assignments,
        [
            task_assignments_pb2.TaskAssignment(
                aggregation_data_forwarding_info=FORWARDING_INFO,
                aggregation_info=(
                    task_assignments_pb2.TaskAssignment.AggregationInfo()
                ),
                session_id=request.session_id,
                aggregation_id='aggregation-session1',
                authorization_token='token-for-aggregation-session1',
                task_name='task1',
                plan=task1_plan,
                init_checkpoint=task1_checkpoint,
                federated_select_uri_info=(
                    task_assignments_pb2.FederatedSelectUriInfo(
                        uri_template=task1_federated_select_uri_template
                    )
                ),
            ),
            task_assignments_pb2.TaskAssignment(
                aggregation_data_forwarding_info=FORWARDING_INFO,
                aggregation_info=(
                    task_assignments_pb2.TaskAssignment.AggregationInfo()
                ),
                session_id=request.session_id,
                aggregation_id='aggregation-session2',
                authorization_token='token-for-aggregation-session2',
                task_name='task2',
                plan=task2_plan,
                init_checkpoint=task2_checkpoint,
                federated_select_uri_info=(
                    task_assignments_pb2.FederatedSelectUriInfo(
                        uri_template=task2_federated_select_uri_template
                    )
                ),
            ),
            # 'task3' should be omitted since there isn't a corresponding task.
        ],
    )

  def test_add_task_with_invalid_task_assignment_mode(self):
    service = task_assignments.Service(
        POPULATION_NAME, lambda: FORWARDING_INFO, self.mock_aggregations_service
    )
    with self.assertRaises(ValueError):
      service.add_task(
          'task',
          _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_UNSPECIFIED,
          'aggregation-session',
          common_pb2.Resource(uri='https://task.example/plan'),
          common_pb2.Resource(uri='https://task.example/checkpoint'),
          'https://task.example/{key_base10}',
      )

  def test_remove_multiple_assignment_task(self):
    service = task_assignments.Service(
        POPULATION_NAME, lambda: FORWARDING_INFO, self.mock_aggregations_service
    )
    service.add_task(
        'task',
        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE,
        'aggregation-session',
        common_pb2.Resource(uri='https://task.example/plan'),
        common_pb2.Resource(uri='https://task.example/checkpoint'),
        'https://task.example/{key_base10}',
    )
    service.remove_task('aggregation-session')

    request = task_assignments_pb2.PerformMultipleTaskAssignmentsRequest(
        population_name=POPULATION_NAME,
        session_id='session-id',
        task_names=['task'],
    )
    self.assertEmpty(
        service.perform_multiple_task_assignments(request).task_assignments
    )

  def test_remove_missing_task(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)
    with self.assertRaises(KeyError):
      service.remove_task('does-not-exist')

  def test_report_task_result(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)
    request = task_assignments_pb2.ReportTaskResultRequest(
        population_name=POPULATION_NAME,
        session_id='session-id',
        aggregation_id='aggregation-id',
        computation_status_code=code_pb2.ABORTED)
    self.assertEqual(
        service.report_task_result(request),
        task_assignments_pb2.ReportTaskResultResponse())

  def test_report_task_result_with_wrong_population(self):
    service = task_assignments.Service(POPULATION_NAME, lambda: FORWARDING_INFO,
                                       self.mock_aggregations_service)
    request = task_assignments_pb2.ReportTaskResultRequest(
        population_name='other/population',
        session_id='session-id',
        aggregation_id='aggregation-id',
        computation_status_code=code_pb2.ABORTED)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.report_task_result(request)
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)


if __name__ == '__main__':
  absltest.main()
