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
"""TFF FederatedContext subclass for the demo Federated Computation platform."""

import asyncio
from collections.abc import Coroutine, Mapping, Sequence
import socket
import ssl
import threading
from typing import Any, Optional, Union
import uuid

from absl import logging
import attr
import federated_language
import numpy as np
import tensorflow as tf
import tensorflow_federated as tff
import tree

from fcp.artifact_building import artifact_constants
from fcp.artifact_building import checkpoint_utils
from fcp.artifact_building import data_spec
from fcp.artifact_building import federated_compute_plan_builder
from fcp.artifact_building import plan_utils
from fcp.artifact_building import variable_helpers
from fcp.demo import checkpoint_tensor_reference
from fcp.demo import federated_computation
from fcp.demo import federated_data_source
from fcp.demo import server
from fcp.protos import plan_pb2


class FederatedContext(federated_language.program.FederatedContext):
  """A FederatedContext for use with the demo platform."""

  def __init__(
      self,
      population_name: str,
      *,
      base_context: Optional[federated_language.framework.SyncContext] = None,
      host: str = 'localhost',
      port: int = 0,
      certfile: Optional[str] = None,
      keyfile: Optional[str] = None,
      address_family: Optional[socket.AddressFamily] = None,
  ):
    """Initializes a `FederatedContext`.

    Args:
      population_name: The name of the population to execute computations on.
      base_context: The context used to run non-federated TFF computations
        (i.e., computations with a type other than FederatedComputation).
      host: The hostname the server should bind to.
      port: The port the server should listen on.
      certfile: The path to the certificate to use for https.
      keyfile: The path to the certificate's private key (if separate).
      address_family: An override for the HTTP server's address family.
    """
    # NOTE: The demo server only supports a single population, which must be
    # specified at startup. An implementation that supports multiple populations
    # should only use the population name from the PopulationDataSource.
    if not federated_data_source.POPULATION_NAME_REGEX.fullmatch(
        population_name):
      raise ValueError(
          'population_name must match '
          f'"{federated_data_source.POPULATION_NAME_REGEX.pattern}".')
    self._population_name = population_name
    self._base_context = base_context
    self._server = server.InProcessServer(
        population_name=population_name,
        host=host,
        port=port,
        address_family=address_family)
    if certfile is not None:
      context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
      context.load_cert_chain(certfile, keyfile)
      self._server.socket = context.wrap_socket(
          self._server.socket, server_side=True)
    self._server_thread = threading.Thread(
        target=self._server.serve_forever, daemon=True)
    self._cached_comps: dict[
        tuple[federated_language.Computation, int], plan_pb2.Plan
    ] = {}

  @property
  def server_port(self) -> int:
    """The port on which the Federated Compute server is running."""
    return self._server.server_port

  def __enter__(self):
    self._server_thread.start()
    logging.log(logging.INFO, 'Federated Compute server running on %s:%s',
                self._server.server_name, self._server.server_port)
    return self

  def __exit__(self, exc_type, exc_value, tb):
    self._server.shutdown()
    self._server_thread.join()
    logging.log(logging.INFO, 'Federated Compute server stopped')

  def invoke(self, comp: federated_language.Computation, arg: Any) -> Any:
    """Invokes a computation.

    Args:
      comp: The computation being invoked.
      arg: The arguments of the call encoded in a computation-specific way. For
        FederatedComputations, this should be a `(state, config)` tuple, where
        the state is a possibly nested structure and the configuration is
        provided by a FederatedDataSource.

    Returns:
      A value reference structure representing the result of the computation.
    """
    # Pass other computation types to the next FederatedContext.
    if not isinstance(comp, federated_computation.FederatedComputation):
      if not self._base_context:
        raise TypeError('computation must be a FederatedComputation if no '
                        'base_context was provided.')
      return self._base_context.invoke(comp, arg)

    state, config = self._parse_arg(arg)
    if config.population_name != self._population_name:
      raise ValueError('FederatedDataSource and FederatedContext '
                       'population_names must match.')

    # Since building the plan can be slow, we cache the result to speed up
    # subsequent invocations.
    cache_key = (comp.wrapped_computation, id(config.example_selector))
    try:
      plan = self._cached_comps[cache_key]
    except KeyError:
      plan = federated_compute_plan_builder.build_plan(
          None,  # MapReduceForm is being deprecated
          comp.distribute_aggregate_form,
          self._get_nested_data_spec(config.example_selector),
          grappler_config=tf.compat.v1.ConfigProto(),
      )
      # Add the TF Lite flatbuffer to the plan.
      plan = plan_utils.generate_and_add_flat_buffer_to_plan(plan)
      self._cached_comps[cache_key] = plan

    checkpoint_future = self._run_computation(comp.name, config, plan,
                                              comp.type_signature.parameter[0],
                                              state)
    result_value_ref = self._create_tensor_reference_struct(
        comp.type_signature.result, checkpoint_future)
    return federated_language.framework.type_to_py_container(
        result_value_ref, comp.type_signature.result
    )

  def _is_state_structure_of_allowed_types(
      self,
      structure: Union[
          tff.structure.Struct,
          tf.Tensor,
          federated_language.program.MaterializableValue,
      ],
  ) -> bool:
    """Checks if each node in `structure` is an allowed type for `state`."""
    if isinstance(structure, tff.structure.Struct):
      structure = tff.structure.to_odict_or_tuple(structure)
    structure = tree.flatten(structure)
    for item in structure:
      if not (
          tf.is_tensor(item)
          or isinstance(
              item,
              (
                  np.ndarray,
                  np.number,
                  int,
                  float,
                  str,
                  bytes,
                  federated_language.program.MaterializableValueReference,
              ),
          )
      ):
        return False
    return True

  def _parse_arg(self, arg: tff.structure.Struct) -> tuple[
      Union[
          Mapping[str, object],
          tf.Tensor,
          federated_language.program.MaterializableValueReference,
      ],
      federated_data_source.DataSelectionConfig,
  ]:
    """Parses and validates the invoke arguments."""
    if len(arg) != 2:
      raise ValueError(f'The argument structure is unsupported: {arg}.')

    state, config = arg
    if attr.has(type(state)):
      state = attr.asdict(state, recurse=True)
    if not self._is_state_structure_of_allowed_types(state):
      raise TypeError(
          'arg[0] must be a value or structure of values of '
          '`MaterializableValueReference`s, `tf.Tensor`s, '
          '`np.ndarray`s, `np.number`s, or Python scalars. Got: '
          f'{tf.nest.map_structure(type, state)!r})'
      )

    # Code below assumes single values are always `tf.Tensor`s.
    if isinstance(state, (int, float, str, bytes, np.ndarray, np.number)):
      state = tf.convert_to_tensor(state)

    if not isinstance(config, federated_data_source.DataSelectionConfig):
      raise TypeError('arg[1] must be the result of '
                      'FederatedDataSource.iterator().select().')
    return state, config

  def _get_nested_data_spec(self, example_selector) -> data_spec.NestedDataSpec:
    """Converts a NestedExampleSelector to a NestedDataSpec."""
    if isinstance(example_selector, dict):
      return {
          k: self._get_nested_data_spec(v) for k, v in example_selector.items()
      }
    return data_spec.DataSpec(example_selector)

  async def _run_computation(
      self,
      name: str,
      config: federated_data_source.DataSelectionConfig,
      plan: plan_pb2.Plan,
      input_type: federated_language.Type,
      input_state: Union[
          tff.structure.Struct,
          tf.Tensor,
          federated_language.program.MaterializableValueReference,
      ],
  ) -> bytes:
    """Prepares and runs a computation using the demo server."""
    input_checkpoint = self._state_to_checkpoint(
        input_type, await self._resolve_value_references(input_state))
    try:
      logging.log(logging.INFO, 'Started running %s', name)
      return await self._server.run_computation(
          name,
          plan,
          input_checkpoint,
          config.task_assignment_mode,
          config.num_clients,
      )
    finally:
      logging.log(logging.INFO, 'Finished running %s', name)

  async def _resolve_value_references(
      self,
      structure: Union[
          tff.structure.Struct,
          tf.Tensor,
          federated_language.program.MaterializableValueReference,
      ],
  ) -> Union[Mapping[str, object], Sequence[object], tf.Tensor]:
    """Dereferences any MaterializableValueReferences in a struct."""
    if isinstance(
        structure, federated_language.program.MaterializableValueReference
    ):
      return await structure.get_value()  # pytype: disable=bad-return-type  # numpy-scalars
    elif tf.is_tensor(structure):
      return structure
    elif isinstance(structure, tff.structure.Struct):
      structure = tff.structure.to_odict_or_tuple(structure)
      s = [self._resolve_value_references(x) for x in tree.flatten(structure)]
      return tree.unflatten_as(structure, s)
    else:
      raise ValueError(
          'arg[1] must be a struct, Tensor, or MaterializableValueReference.')

  def _state_to_checkpoint(
      self,
      state_type: federated_language.Type,
      state: Union[tff.structure.Struct, tf.Tensor],
  ) -> bytes:
    """Converts computation input state to a checkpoint file.

    The checkpoint file format is used to pass the state to
    InProcessServer.run_computation.

    Args:
      state_type: The TFF type of the state structure.
      state: A Tensor or TFF structure with input state for a computation.

    Returns:
      The state encoded as a checkpoint file.
    """
    var_names = variable_helpers.variable_names_from_type(
        state_type,  # pytype: disable=wrong-arg-types
        name=artifact_constants.SERVER_STATE_VAR_PREFIX,
    )

    # Write to a file in TensorFlow's RamFileSystem to avoid disk I/O.
    tmpfile = f'ram://{uuid.uuid4()}.ckpt'
    checkpoint_utils.save_tff_structure_to_checkpoint(state, var_names, tmpfile)
    try:
      with tf.io.gfile.GFile(tmpfile, 'rb') as f:
        return f.read()
    finally:
      tf.io.gfile.remove(tmpfile)

  def _create_tensor_reference_struct(
      self,
      result_type: federated_language.Type,
      checkpoint_future: Coroutine[Any, Any, bytes],
  ) -> tff.structure.Struct:
    """Creates the CheckpointTensorReference struct for a result type."""
    task = asyncio.create_task(checkpoint_future)
    tensor_specs = checkpoint_utils.tff_type_to_tensor_spec_list(result_type)  # pytype: disable=wrong-arg-types
    var_names = variable_helpers.variable_names_from_type(
        result_type[0],  # pytype: disable=unsupported-operands
        name=artifact_constants.SERVER_STATE_VAR_PREFIX,
    ) + variable_helpers.variable_names_from_type(
        result_type[1],  # pytype: disable=unsupported-operands
        name=artifact_constants.SERVER_METRICS_VAR_PREFIX,
    )
    tensor_refs = [
        checkpoint_tensor_reference.CheckpointTensorReference(
            var_name, spec.dtype, spec.shape, task
        )
        for var_name, spec in zip(var_names, tensor_specs)
    ]
    return checkpoint_utils.pack_tff_value(
        result_type,  # pytype: disable=wrong-arg-types
        tensor_refs,
    )
