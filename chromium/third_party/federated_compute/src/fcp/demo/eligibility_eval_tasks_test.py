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
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expresus or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Tests for eligibility_eval_tasks."""

import datetime
import http
from unittest import mock
import uuid

from absl.testing import absltest

from google.rpc import code_pb2
from fcp.demo import eligibility_eval_tasks
from fcp.demo import http_actions
from fcp.protos import population_eligibility_spec_pb2
from fcp.protos.federatedcompute import common_pb2
from fcp.protos.federatedcompute import eligibility_eval_tasks_pb2

_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)

POPULATION_NAME = 'test/population'
FORWARDING_INFO = common_pb2.ForwardingInfo(
    target_uri_prefix='https://forwarding.example/')


class EligibilityEvalTasksTest(absltest.TestCase):

  @mock.patch.object(uuid, 'uuid4', return_value=uuid.uuid4(), autospec=True)
  def test_request_eligibility_eval_task(self, mock_uuid):
    service = eligibility_eval_tasks.Service(POPULATION_NAME,
                                             lambda: FORWARDING_INFO)
    service.add_task('task1', _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE)
    request = eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest(
        population_name=POPULATION_NAME)
    retry_window = common_pb2.RetryWindow()
    retry_window.delay_min.FromTimedelta(datetime.timedelta(seconds=10))
    retry_window.delay_max.FromTimedelta(datetime.timedelta(seconds=30))
    self.assertEqual(
        service.request_eligibility_eval_task(request),
        eligibility_eval_tasks_pb2.EligibilityEvalTaskResponse(
            session_id=str(mock_uuid.return_value),
            task_assignment_forwarding_info=FORWARDING_INFO,
            no_eligibility_eval_configured=(
                eligibility_eval_tasks_pb2.NoEligibilityEvalConfigured()),
            retry_window_if_accepted=retry_window,
            retry_window_if_rejected=retry_window))

  def test_request_eligibility_eval_task_with_multiple_assignment(self):
    service = eligibility_eval_tasks.Service(
        POPULATION_NAME, lambda: FORWARDING_INFO
    )
    service.add_task('task1', _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE)
    service.add_task('task2', _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE)

    request = eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest(
        population_name=POPULATION_NAME,
        eligibility_eval_task_capabilities=(
            eligibility_eval_tasks_pb2.EligibilityEvalTaskCapabilities(
                supports_multiple_task_assignment=True
            )
        ),
    )
    response = service.request_eligibility_eval_task(request)
    self.assertTrue(response.HasField('eligibility_eval_task'))
    spec_resource = response.eligibility_eval_task.population_eligibility_spec
    population_eligibility_spec = (
        population_eligibility_spec_pb2.PopulationEligibilitySpec.FromString(
            spec_resource.inline_resource.data
        )
    )
    self.assertEqual(
        population_eligibility_spec,
        population_eligibility_spec_pb2.PopulationEligibilitySpec(
            task_info=[
                population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo(
                    task_name='task1',
                    task_assignment_mode=(
                        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE
                    ),
                ),
                population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo(
                    task_name='task2',
                    task_assignment_mode=(
                        _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE
                    ),
                ),
            ],
        ),
    )

  def test_request_eligibility_eval_task_with_wrong_population(self):
    service = eligibility_eval_tasks.Service(POPULATION_NAME,
                                             lambda: FORWARDING_INFO)
    request = eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest(
        population_name='other/population')
    with self.assertRaises(http_actions.HttpError) as cm:
      service.request_eligibility_eval_task(request)
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  def test_report_eligibility_eval_task_result(self):
    service = eligibility_eval_tasks.Service(POPULATION_NAME,
                                             lambda: FORWARDING_INFO)
    request = eligibility_eval_tasks_pb2.ReportEligibilityEvalTaskResultRequest(
        population_name=POPULATION_NAME,
        session_id='session-id',
        status_code=code_pb2.ABORTED)
    self.assertEqual(
        service.report_eligibility_eval_task_result(request),
        eligibility_eval_tasks_pb2.ReportEligibilityEvalTaskResultResponse())

  def test_report_eligibility_eval_task_result_with_wrong_population(self):
    service = eligibility_eval_tasks.Service(POPULATION_NAME,
                                             lambda: FORWARDING_INFO)
    request = eligibility_eval_tasks_pb2.ReportEligibilityEvalTaskResultRequest(
        population_name='other/population',
        session_id='session-id',
        status_code=code_pb2.ABORTED)
    with self.assertRaises(http_actions.HttpError) as cm:
      service.report_eligibility_eval_task_result(request)
    self.assertEqual(cm.exception.code, http.HTTPStatus.NOT_FOUND)

  def test_remove_task(self):
    service = eligibility_eval_tasks.Service(
        POPULATION_NAME, lambda: FORWARDING_INFO
    )
    service.add_task('task', _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE)
    service.remove_task('task')
    request = eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest(
        population_name=POPULATION_NAME,
        eligibility_eval_task_capabilities=(
            eligibility_eval_tasks_pb2.EligibilityEvalTaskCapabilities(
                supports_multiple_task_assignment=True
            )
        ),
    )
    response = service.request_eligibility_eval_task(request)
    spec_resource = response.eligibility_eval_task.population_eligibility_spec
    population_eligibility_spec = (
        population_eligibility_spec_pb2.PopulationEligibilitySpec.FromString(
            spec_resource.inline_resource.data
        )
    )
    self.assertEqual(
        population_eligibility_spec,
        population_eligibility_spec_pb2.PopulationEligibilitySpec(),
    )

  def test_remove_missing_task(self):
    service = eligibility_eval_tasks.Service(
        POPULATION_NAME, lambda: FORWARDING_INFO
    )
    with self.assertRaises(KeyError):
      service.remove_task('does-not-exist')


if __name__ == '__main__':
  absltest.main()
