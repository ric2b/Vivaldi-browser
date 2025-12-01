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
"""Action handlers for the EligibilityEvalTasks service.

Eligibility Eval tasks are not currently supported by this demo implementation.
"""

import dataclasses
import datetime
import http
import threading
from typing import Callable
import uuid

from absl import logging

from google.rpc import code_pb2
from fcp.demo import http_actions
from fcp.protos import population_eligibility_spec_pb2
from fcp.protos.federatedcompute import common_pb2
from fcp.protos.federatedcompute import eligibility_eval_tasks_pb2

_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)


@dataclasses.dataclass(frozen=True)
class _Task:
  task_name: str
  task_assignment_mode: _TaskAssignmentMode


class Service:
  """Implements the EligibilityEvalTasks service."""

  def __init__(self, population_name: str,
               forwarding_info: Callable[[], common_pb2.ForwardingInfo]):
    self._population_name = population_name
    self._forwarding_info = forwarding_info
    self._tasks: dict[str, _Task] = {}
    self._tasks_lock = threading.Lock()

  def add_task(self, task_name: str, task_assignment_mode: _TaskAssignmentMode):
    """Informs the service that a new task has been added to the system."""
    with self._tasks_lock:
      self._tasks[task_name] = _Task(task_name, task_assignment_mode)

  def remove_task(self, task_name: str):
    """Informs the service that a task has been removed from the system."""
    with self._tasks_lock:
      del self._tasks[task_name]

  @property
  def _population_eligibility_spec(
      self,
  ) -> population_eligibility_spec_pb2.PopulationEligibilitySpec:
    with self._tasks_lock:
      return population_eligibility_spec_pb2.PopulationEligibilitySpec(
          task_info=[
              population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo(
                  task_name=task.task_name,
                  task_assignment_mode=task.task_assignment_mode,
              )
              for task in self._tasks.values()
          ]
      )

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.EligibilityEvalTasks',
      method='RequestEligibilityEvalTask')
  def request_eligibility_eval_task(
      self, request: eligibility_eval_tasks_pb2.EligibilityEvalTaskRequest
  ) -> eligibility_eval_tasks_pb2.EligibilityEvalTaskResponse:
    """Handles a RequestEligibilityEvalTask request."""
    if request.population_name != self._population_name:
      raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND)
    # NOTE: A production implementation should use
    # `request.attestation_measurement` to verify the device is valid, e.g.
    # using the SafetyNet Attestation API.
    session_id = str(uuid.uuid4())
    logging.debug('[%s] RequestEligibilityEvalTask', session_id)

    # NOTE: A production implementation should vary the retry windows based on
    # the population size and other factors, as described in TFLaS §2.3.
    retry_window = common_pb2.RetryWindow()
    retry_window.delay_min.FromTimedelta(datetime.timedelta(seconds=10))
    retry_window.delay_max.FromTimedelta(datetime.timedelta(seconds=30))

    response = eligibility_eval_tasks_pb2.EligibilityEvalTaskResponse(
        task_assignment_forwarding_info=self._forwarding_info(),
        session_id=str(uuid.uuid4()),
        retry_window_if_accepted=retry_window,
        retry_window_if_rejected=retry_window,
    )

    # This implementation does not support Eligibility Eval tasks. However, the
    # EligibilityEvalTask response is also used to provide the
    # PopulationEligibilitySpec to clients, so the service returns an
    # EligibilityEvalTask instead of NoEligibilityEvalConfigured if the client
    # supports multiple task assignment.
    capabilities = request.eligibility_eval_task_capabilities
    if capabilities.supports_multiple_task_assignment:
      spec_resource = response.eligibility_eval_task.population_eligibility_spec
      spec_resource.inline_resource.data = (
          self._population_eligibility_spec.SerializeToString()
      )
    else:
      response.no_eligibility_eval_configured.SetInParent()
    return response

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.EligibilityEvalTasks',
      method='ReportEligibilityEvalTaskResult')
  def report_eligibility_eval_task_result(
      self,
      request: eligibility_eval_tasks_pb2.ReportEligibilityEvalTaskResultRequest
  ) -> eligibility_eval_tasks_pb2.ReportEligibilityEvalTaskResultResponse:
    """Handles a ReportEligibilityEvalTaskResult request."""
    if request.population_name != self._population_name:
      raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND)
    # NOTE: A production implementation should collect and report metrics. In
    # this implementation, we simply log the result.
    logging.log(
        logging.DEBUG if request.status_code == code_pb2.OK else logging.WARN,
        '[%s] ReportEligibilityEvalTaskResult: %s', request.session_id,
        code_pb2.Code.Name(request.status_code))
    return eligibility_eval_tasks_pb2.ReportEligibilityEvalTaskResultResponse()
