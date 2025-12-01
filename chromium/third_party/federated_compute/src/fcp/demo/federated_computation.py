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
"""federated_language.Computation subclass for the demo Federated Computation platform."""

import functools
import re

import federated_language
import tensorflow_federated as tff

COMPUTATION_NAME_REGEX = re.compile(r'\w+(/\w+)*')


class FederatedComputation(federated_language.Computation):
  """A federated_language.Computation that should be run in a federated_language.program.FederatedContext."""

  def __init__(self, comp: federated_language.Computation, *, name: str):
    """Constructs a new FederatedComputation object.

    Args:
      comp: The DistributeAggregateForm- compatible computation that will be
        run.
      name: A unique name for the computation.
    """
    tff.backends.mapreduce.check_computation_compatible_with_map_reduce_form(
        comp
    )  # pytype: disable=wrong-arg-types
    if not COMPUTATION_NAME_REGEX.fullmatch(name):
      raise ValueError(f'name must match "{COMPUTATION_NAME_REGEX.pattern}".')
    self._comp = comp
    self._name = name

  @functools.cached_property
  def distribute_aggregate_form(
      self,
  ) -> tff.backends.mapreduce.DistributeAggregateForm:
    """The underlying DistributeAggregateForm representation."""
    return tff.backends.mapreduce.get_distribute_aggregate_form_for_computation(  # pytype: disable=wrong-arg-types
        self._comp
    )

  @property
  def wrapped_computation(self) -> federated_language.Computation:
    """The underlying federated_language.Computation."""
    return self._comp

  @property
  def name(self) -> str:
    """The name of the computation."""
    return self._name

  @property
  def type_signature(self) -> federated_language.Type:
    return self._comp.type_signature

  def __call__(self, *args, **kwargs) ->...:
    arg = tff.structure.Struct([(None, arg) for arg in args] +
                               list(kwargs.items()))
    return federated_language.framework.get_context_stack().current.invoke(
        self, arg
    )

  def __hash__(self) -> int:
    return hash((self._comp, self._name))
