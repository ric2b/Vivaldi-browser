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
"""Action handlers for the TaskAssignments service."""

import collections
import dataclasses
import http
import threading
from typing import Callable, Optional
import uuid

from absl import logging

from google.longrunning import operations_pb2
from google.rpc import code_pb2
from google.protobuf import text_format
from fcp.demo import aggregations
from fcp.demo import http_actions
from fcp.protos import population_eligibility_spec_pb2
from fcp.protos.federatedcompute import common_pb2
from fcp.protos.federatedcompute import task_assignments_pb2

_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)


@dataclasses.dataclass(frozen=True)
class _Task:
  task_name: str
  aggregation_session_id: str
  init_checkpoint: common_pb2.Resource
  plan: common_pb2.Resource
  federated_select_uri_template: str


class Service:
  """Implements the TaskAssignments service."""

  def __init__(self, population_name: str,
               forwarding_info: Callable[[], common_pb2.ForwardingInfo],
               aggregations_service: aggregations.Service):
    self._population_name = population_name
    self._forwarding_info = forwarding_info
    self._aggregations_service = aggregations_service
    self._single_assignment_tasks = collections.deque()
    self._multiple_assignment_tasks: list[_Task] = []
    self._tasks_lock = threading.Lock()

  def add_task(
      self,
      task_name: str,
      task_assignment_mode: _TaskAssignmentMode,
      aggregation_session_id: str,
      plan: common_pb2.Resource,
      init_checkpoint: common_pb2.Resource,
      federated_select_uri_template: str,
  ):
    """Adds a new task to the service."""
    task = _Task(
        task_name=task_name,
        aggregation_session_id=aggregation_session_id,
        init_checkpoint=init_checkpoint,
        plan=plan,
        federated_select_uri_template=federated_select_uri_template,
    )
    if task_assignment_mode == _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE:
      with self._tasks_lock:
        self._single_assignment_tasks.append(task)
    elif (
        task_assignment_mode
        == _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_MULTIPLE
    ):
      with self._tasks_lock:
        self._multiple_assignment_tasks.append(task)
    else:
      raise ValueError(f'Unsupport TaskAssignmentMode {task_assignment_mode}.')

  def remove_task(self, aggregation_session_id: str):
    """Removes a task from the service."""
    with self._tasks_lock:
      for task in self._single_assignment_tasks:
        if task.aggregation_session_id == aggregation_session_id:
          self._single_assignment_tasks.remove(task)
          return
      for task in self._multiple_assignment_tasks:
        if task.aggregation_session_id == aggregation_session_id:
          self._multiple_assignment_tasks.remove(task)
          return
      raise KeyError(aggregation_session_id)

  @property
  def _current_task(self) -> Optional[_Task]:
    with self._tasks_lock:
      return (
          self._single_assignment_tasks[0]
          if self._single_assignment_tasks
          else None
      )

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.TaskAssignments',
      method='StartTaskAssignment')
  def start_task_assignment(
      self, request: task_assignments_pb2.StartTaskAssignmentRequest
  ) -> operations_pb2.Operation:
    """Handles a StartTaskAssignment request."""
    if request.population_name != self._population_name:
      raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND)

    # NOTE: A production implementation should consider whether the current task
    # supports `request.client_version` before assigning the client. Given that
    # all clients may not be eligible for all tasks, consider more sophisticated
    # assignment than a FIFO queue.
    task = self._current_task
    if task:
      logging.debug('[%s] StartTaskAssignment: assigned %s', request.session_id,
                    task.task_name)
      # NOTE: If a production implementation of the Aggregations service cannot
      # always pre-authorize clients (e.g., due to rate-limiting incoming
      # clients), this code should either retry the operation or return a
      # non-permanent error to the client (e.g., UNAVAILABLE).
      authorization_token = self._aggregations_service.pre_authorize_clients(
          task.aggregation_session_id, num_tokens=1)[0]
      response = task_assignments_pb2.StartTaskAssignmentResponse(
          task_assignment=task_assignments_pb2.TaskAssignment(
              aggregation_data_forwarding_info=self._forwarding_info(),
              aggregation_info=(
                  task_assignments_pb2.TaskAssignment.AggregationInfo()
              ),
              session_id=request.session_id,
              aggregation_id=task.aggregation_session_id,
              authorization_token=authorization_token,
              task_name=task.task_name,
              init_checkpoint=task.init_checkpoint,
              plan=task.plan,
              federated_select_uri_info=(
                  task_assignments_pb2.FederatedSelectUriInfo(
                      uri_template=task.federated_select_uri_template
                  )
              ),
          )
      )
    else:
      # NOTE: Instead of immediately rejecting clients, a production
      # implementation may keep around some number of clients to be assigned to
      # queued tasks or even future rounds of the current task (depending on how
      # quickly rounds complete).
      logging.debug('[%s] StartTaskAssignment: rejected', request.session_id)
      response = task_assignments_pb2.StartTaskAssignmentResponse(
          rejection_info=common_pb2.RejectionInfo())

    # If task assignment took significant time, we return a longrunning
    # Operation; since this implementation makes assignment decisions right
    # away, we can return an already-completed operation.
    op = operations_pb2.Operation(name=f'operations/{uuid.uuid4()}', done=True)
    op.metadata.Pack(task_assignments_pb2.StartTaskAssignmentMetadata())
    op.response.Pack(response)
    return op

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.TaskAssignments',
      method='PerformMultipleTaskAssignments')
  def perform_multiple_task_assignments(
      self, request: task_assignments_pb2.PerformMultipleTaskAssignmentsRequest
  ) -> task_assignments_pb2.PerformMultipleTaskAssignmentsResponse:
    """Handles a PerformMultipleTaskAssignments request."""
    if request.population_name != self._population_name:
      raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND)

    task_assignments = []
    with self._tasks_lock:
      for task in self._multiple_assignment_tasks:
        if task.task_name not in request.task_names:
          continue

        # NOTE: A production implementation should consider whether the task
        # supports `request.client_version` before assigning the client.

        authorization_token = self._aggregations_service.pre_authorize_clients(
            task.aggregation_session_id, num_tokens=1)[0]
        task_assignments.append(
            task_assignments_pb2.TaskAssignment(
                aggregation_data_forwarding_info=self._forwarding_info(),
                aggregation_info=(
                    task_assignments_pb2.TaskAssignment.AggregationInfo()
                ),
                session_id=request.session_id,
                aggregation_id=task.aggregation_session_id,
                authorization_token=authorization_token,
                task_name=task.task_name,
                init_checkpoint=task.init_checkpoint,
                plan=task.plan,
                federated_select_uri_info=(
                    task_assignments_pb2.FederatedSelectUriInfo(
                        uri_template=task.federated_select_uri_template
                    )
                ),
            )
        )

    return task_assignments_pb2.PerformMultipleTaskAssignmentsResponse(
        task_assignments=task_assignments)

  @http_actions.proto_action(
      service='google.internal.federatedcompute.v1.TaskAssignments',
      method='ReportTaskResult')
  def report_task_result(
      self, request: task_assignments_pb2.ReportTaskResultRequest
  ) -> task_assignments_pb2.ReportTaskResultResponse:
    """Handles a ReportTaskResult request."""
    if request.population_name != self._population_name:
      raise http_actions.HttpError(http.HTTPStatus.NOT_FOUND)
    logging.log(
        (logging.DEBUG if request.computation_status_code == code_pb2.OK else
         logging.WARN), '[%s] ReportTaskResult: %s (%s)', request.session_id,
        code_pb2.Code.Name(request.computation_status_code),
        text_format.MessageToString(request.client_stats, as_one_line=True))
    return task_assignments_pb2.ReportTaskResultResponse()
