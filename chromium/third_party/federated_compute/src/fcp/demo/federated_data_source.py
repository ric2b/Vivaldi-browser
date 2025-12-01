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
"""TFF FederatedDataSource for the demo Federated Computation platform."""

import dataclasses
import functools
import re
from typing import Optional, Union

import federated_language
import numpy as np

from fcp.protos import plan_pb2
from fcp.protos import population_eligibility_spec_pb2

POPULATION_NAME_REGEX = re.compile(r'\w+(/\w+)*')

_NestedExampleSelector = Union[plan_pb2.ExampleSelector,
                               dict[str, '_NestedExampleSelector']]
_TaskAssignmentMode = (
    population_eligibility_spec_pb2.PopulationEligibilitySpec.TaskInfo.TaskAssignmentMode
)


@dataclasses.dataclass
class DataSelectionConfig:
  population_name: str
  example_selector: _NestedExampleSelector
  task_assignment_mode: _TaskAssignmentMode
  num_clients: int


class FederatedDataSource(federated_language.program.FederatedDataSource):
  """A FederatedDataSource for use with the demo platform.

  A FederatedDataSource represents a population of client devices and the set of
  on-device data over which computations should be invoked.
  """

  _FEDERATED_TYPE = federated_language.FederatedType(
      federated_language.SequenceType(np.str_), federated_language.CLIENTS
  )

  def __init__(
      self,
      population_name: str,
      example_selector: _NestedExampleSelector,
      task_assignment_mode: _TaskAssignmentMode = (
          _TaskAssignmentMode.TASK_ASSIGNMENT_MODE_SINGLE
      ),
  ):
    """Constructs a new FederatedDataSource object.

    Args:
      population_name: The name of the population to execute computations on.
      example_selector: A `plan_pb2.ExampleSelector` or a structure of
        ExampleSelectors indicating CLIENTS-placed data to execute over.
      task_assignment_mode: The TaskAssignmentMode to use for this computation.
    """
    if not POPULATION_NAME_REGEX.fullmatch(population_name):
      raise ValueError(
          f'population_name must match "{POPULATION_NAME_REGEX.pattern}".')
    self._population_name = population_name
    self._example_selector = example_selector
    self._task_assignment_mode = task_assignment_mode

  @property
  def population_name(self) -> str:
    """The name of the population from which examples will be retrieved."""
    return self._population_name

  @property
  def example_selector(self) -> _NestedExampleSelector:
    """The NestedExampleSelector used to obtain the examples."""
    return self._example_selector

  @property
  def task_assignment_mode(self) -> _TaskAssignmentMode:
    """The TaskAssignmentMode to use for this computation."""
    return self._task_assignment_mode

  @functools.cached_property
  def federated_type(self) -> federated_language.FederatedType:

    def get_struct_type(value):
      if isinstance(value, dict):
        return federated_language.StructType(
            [(k, get_struct_type(v)) for k, v in value.items()]
        )
      # ExternalDataset always returns a sequence of tf.strings, which should be
      # serialized `tf.train.Example` protos.
      return federated_language.SequenceType(np.str_)

    return federated_language.FederatedType(
        get_struct_type(self._example_selector), federated_language.CLIENTS
    )

  def iterator(self) -> federated_language.program.FederatedDataSourceIterator:
    return _FederatedDataSourceIterator(self)


class _FederatedDataSourceIterator(
    federated_language.program.FederatedDataSourceIterator
):
  """A `FederatedDataSourceIterator` for use with the demo platform."""

  def __init__(self, data_source: FederatedDataSource):
    self._data_source = data_source

  @classmethod
  def from_bytes(cls, data: bytes) -> '_FederatedDataSourceIterator':
    """Deserializes the object from bytes."""
    raise NotImplementedError

  def to_bytes(self) -> bytes:
    """Serializes the object to bytes."""
    raise NotImplementedError

  @property
  def federated_type(self):
    return self._data_source.federated_type

  def select(self, k: Optional[int] = None) -> DataSelectionConfig:
    """Returns a new selection of data from this iterator.

    Args:
      k: A number of clients to select. Must be a positive integer.

    Raises:
      ValueError: If `k` is not a positive integer.
    """
    if k is None or k <= 0:
      raise ValueError('k must be positive.')
    return DataSelectionConfig(
        self._data_source.population_name,
        self._data_source.example_selector,
        self._data_source.task_assignment_mode,
        k,
    )
