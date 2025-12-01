# Copyright 2025 Google LLC
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
"""Utilities for compiling a TFF computation into composed tee form."""

from collections.abc import Iterator
import enum
from typing import Union

import federated_language
import immutabledict
import tensorflow_federated as tff


_COMPOSED_TEE_URI = 'composed_tee'


class TeeStage(enum.IntEnum):
  """The stages of a "composed_tee" intrinsic call.

  Each ComposedTeeCall object stores a BaseLambda object for each TeeStage.
  """

  # The accumulate stage refers to the pre-aggregation logic (e.g. federated_map
  # calls over client data) and the accumulate portion of the aggregation logic
  # that will execute within a given "composed_tee" intrinsic call. The
  # BaseLambda at this stage within a ComposedTeeCall object is an
  # AccumulateLambda object.
  ACCUMULATE = 0
  # The report stage refers to the merge+report portion of the aggregation logic
  # and the post-aggregation logic that will execute within a given
  # "composed_tee" intrinsic call. The BaseLambda at this stage within a
  # ComposedTeeCall object is a ReportLambda object.
  REPORT = 1
  # The number of stages within a ComposedTeeCall object.
  MAX_STAGES = 2


# A map from non-aggregation intrinsic uris to the TeeStage associated with that
# intrinsic. Calls to these intrinsics will be placed in the corresponding
# BaseLambda within a ComposedTeeCall object.
_INTRINSIC_URI_TO_TEE_STAGE_MAP = immutabledict.immutabledict({
    federated_language.framework.FEDERATED_MAP.uri: TeeStage.ACCUMULATE,
    federated_language.framework.FEDERATED_MAP_ALL_EQUAL.uri: (
        TeeStage.ACCUMULATE
    ),
    federated_language.framework.FEDERATED_ZIP_AT_CLIENTS.uri: (
        TeeStage.ACCUMULATE
    ),
    federated_language.framework.FEDERATED_BROADCAST.uri: TeeStage.ACCUMULATE,
    federated_language.framework.FEDERATED_VALUE_AT_CLIENTS.uri: (
        TeeStage.ACCUMULATE
    ),
    federated_language.framework.FEDERATED_EVAL_AT_CLIENTS.uri: (
        TeeStage.ACCUMULATE
    ),
    federated_language.framework.FEDERATED_APPLY.uri: TeeStage.REPORT,
    federated_language.framework.FEDERATED_ZIP_AT_SERVER.uri: TeeStage.REPORT,
    federated_language.framework.FEDERATED_VALUE_AT_SERVER.uri: TeeStage.REPORT,
    federated_language.framework.FEDERATED_EVAL_AT_SERVER.uri: TeeStage.REPORT,
})

# A map from TeeStage to an index specifying the position within the output
# struct of the "composed_tee" intrinsic call that corresponds to that TeeStage.
_TEE_STAGE_OUTPUT_POSITION_MAP = immutabledict.immutabledict(
    {TeeStage.ACCUMULATE: 0, TeeStage.REPORT: 1}
)


class BaseLambda:
  """Base class for component Lambda args provided to a "composed_tee" call.

  Attributes:
    local_vals: A list of the (local_name, local_value) tuples that are assigned
      to this BaseLambda object.
    inputs_from_original_computation_param: A list containing the portions of
      the original computation input parameter that are needed as inputs to the
      Lambda computation represented by this BaseLambda object.
    inputs_from_previous_composed_tee: A map containing the portions of the
      outputs of prior "composed_tee" calls that are needed as inputs to the
      Lambda computation represented by this BaseLambda object. Since we don't
      know the final output type of the "composed_tee" calls until we have
      finished constructing all of the ComposedTeeCall objects, we use this map
      to store preliminary information about how to extract the needed portions
      of the prior "composed_tee" call outputs. For example, suppose we're
      trying to process a dependency this BaseLambda object has on a Reference
      with name ref_name, which refers to the result of a call stored within the
      BaseLambda corresponding to TeeStage tee_stage in the ComposedTeeCall at
      position tee_call_index. If the value referenced by ref_name will be
      positioned at index output_index within the portion of the output struct
      corresponding to tee_stage, then we add the tuple (ref_name, output_index)
      to the value associated with the key (tee_call_index, tee_stage) in the
      map.
    outputs_for_future_composed_tee: A list containing the References the Lambda
      computation represented by this BaseLambda object should return.
    lambda_index: The index to associate with this object.
    tee_stage: The TeeStage associated with this object.
  """

  # The BaseLambda class tracks inputs that form a struct with two elements:
  # * Inputs derived from the original computation input parameter
  # * Inputs derived from prior "composed_tee" calls
  INDEX_OF_ORIGINAL_PARAM_DERIVED_INPUTS = 0
  INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS = 1

  def __init__(self, lambda_index: int, tee_stage: TeeStage):
    """Initializes a BaseLambda object.

    Args:
      lambda_index: The index to associate with this object. BaseLambda objects
        should be assigned indices that reflect the order in which the Lambda
        computations derived from these BaseLambdas will ultimately be executed.
        The BaseLambda objects associated with an earlier ComposedTeeCall object
        have lower indices than the BaseLambda objects associated with a later
        ComposedTeeCall object. Within a single ComposedTeeCall object, the
        BaseLambda objects associated with an earlier TeeStage should have lower
        indices than the BaseLambda objects associated with a later TeeStage.
      tee_stage: The TeeStage associated with this object.
    """
    self._lambda_index = lambda_index
    self._tee_stage = tee_stage

    self.local_vals: list[tuple[str, federated_language.framework.Call]] = []
    self.inputs_from_original_computation_param: list[
        Union[
            federated_language.framework.Selection,
            federated_language.framework.Reference,
        ]
    ] = []
    self.inputs_from_previous_composed_tee: dict[
        tuple[int, TeeStage], set[tuple[str, int]]
    ] = {}
    self.outputs_for_future_composed_tee: list[
        federated_language.framework.Reference
    ] = []

  @property
  def lambda_index(self) -> int:
    return self._lambda_index

  @property
  def tee_stage(self) -> TeeStage:
    return self._tee_stage

  def get_inputs(
      self,
      composed_tee_call_locals: list[
          tuple[str, federated_language.framework.Call]
      ],
  ) -> federated_language.framework.Struct:
    """Returns a struct representing a portion of the BaseLambda's input.

    Returns the portion of the input to the Lambda computation that is derived
    from the original computation input parameter or from prior "composed_tee"
    call outputs.

    When this method is called, we have all of the information needed to
    determine the final output type of the prior "composed_tee" calls. We can
    use this information to finalize some of the preliminary information stored
    in self.inputs_from_previous_composed_tee.

    Args:
      composed_tee_call_locals: A list containing (local_name, local_value)
        tuples for all of the "composed_tee" calls that have already been
        finalized for inclusion in the final computation that is in composed tee
        form.

    Returns:
      A struct containing two elements:
        * Inputs derived from the original computation input parameter
        * Inputs derived from prior "composed_tee" calls.
    """
    combined_previous_composed_tee_deps = []
    for (
        composed_tee_index,
        tee_stage,
    ) in self.inputs_from_previous_composed_tee:
      local_name, local_value = composed_tee_call_locals[composed_tee_index]
      combined_previous_composed_tee_deps.append(
          federated_language.framework.Selection(
              federated_language.framework.Reference(
                  local_name, local_value.type_signature
              ),
              index=_TEE_STAGE_OUTPUT_POSITION_MAP[tee_stage],
          )
      )
    return federated_language.framework.Struct([
        federated_language.framework.Struct(
            self.inputs_from_original_computation_param
        ),
        federated_language.framework.Struct(
            combined_previous_composed_tee_deps
        ),
    ])

  def get_processed_local_values(
      self,
      input_name: str,
      input_type: federated_language.Type,
  ) -> list[tuple[str, federated_language.framework.ComputationBuildingBlock]]:
    """Returns a list of (local_name, local_value) tuples for this BaseLambda.

    Performs processing of the input vals and the stored local_vals to determine
    the (local_name, local_value) tuples that should be included as locals
    within the Lambda computation's return Block.

    Args:
      input_name: The name of the input parameter for the Lambda computation
        represented by this BaseLambda object.
      input_type: The type of the input parameter for the Lambda computation
        represented by this BaseLambda object.

    Returns:
      A list of (local_name, local_value) tuples.
    """
    all_local_vals = []

    # Local values within this Lambda computation may refer to selections of
    # references found within other Lambda computations. For example, we might
    # see z=federated_map(y[1]) in this Lambda computation, which references a
    # y=federated_map(x) call in a prior Lambda computation. During the earlier
    # dependency tracking steps, we will have already designated y as a value
    # that should be returned by the prior Lambda computation and provided as
    # as input to this Lambda computation. However, we still need to update
    # operations within this Lambda computation to replace y[1] with an
    # appropriate selection into this Lambda's input parameter.
    for i, dependency_tuple_set in enumerate(
        self.inputs_from_previous_composed_tee.values()
    ):
      for dependency_name, output_index in dependency_tuple_set:
        previous_composed_tee_inputs = federated_language.framework.Selection(
            federated_language.framework.Reference(input_name, input_type),
            index=BaseLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS,
        )
        previous_composed_tee_inputs_for_dependency_tuple_set = (
            federated_language.framework.Selection(
                previous_composed_tee_inputs, index=i
            )
        )
        all_local_vals.append((
            dependency_name,
            federated_language.framework.Selection(
                previous_composed_tee_inputs_for_dependency_tuple_set,
                index=output_index,
            ),
        ))

    # Parts of the input to this Lambda computation may refer to selections of
    # the original computation's input parameter (e.g. original_param[0]). When
    # we encounter operations that refer to such selections within local_vals,
    # we must replace them with an appropriate selection into this Lambda's
    # input parameter (e.g. calls such as
    # federated_language.federated_sum(original_param[0]) must be updated since
    # original_param will not be directly accessible to this Lambda
    # computation).
    #
    # First, we create a dictionary that maps the original computation input
    # parameter selections to selections into this Lambda's input parameter
    # that represent the same value. Then, we iterate through local_vals and
    # make substitutions according to the dictionary.
    selection_substitutions = {}
    for i, input_param_selection in enumerate(
        self.inputs_from_original_computation_param
    ):
      # Note that inputs derived from the original computation's input
      # parameter are stored at index 0 within the Lambda's input struct (and
      # inputs derived from prior "composed_tee" calls are stored at index 1).
      selection_substitutions[input_param_selection] = (
          federated_language.framework.Selection(
              federated_language.framework.Selection(
                  federated_language.framework.Reference(
                      input_name, input_type
                  ),
                  index=BaseLambda.INDEX_OF_ORIGINAL_PARAM_DERIVED_INPUTS,
              ),
              index=i,
          )
      )
    for local_name, local_value in self.local_vals:

      def update_original_param_selection(subvalue):
        if subvalue in selection_substitutions:
          return selection_substitutions[subvalue], True
        return subvalue, False

      updated_local_value, _ = federated_language.framework.transform_preorder(
          local_value, update_original_param_selection
      )
      all_local_vals.append((local_name, updated_local_value))

    return all_local_vals


class AccumulateLambda(BaseLambda):
  """Specialized BaseLambda for the accumulate stage."""

  # The AccumulateLambda computation returns as output a struct containing two
  # elements:
  # * A struct of pre-aggregate values that may be consumed by a future
  #   AccumulateLambda.
  # * A struct of partial aggregate values that should be consumed by the
  #   next ReportLambda.
  INDEX_OF_PRE_AGGREGATE_OUTPUTS = 0
  INDEX_OF_PARTIAL_AGGREGATE_OUTPUTS = 1

  def __init__(self, lambda_index):
    super().__init__(lambda_index, TeeStage.ACCUMULATE)

    # The AccumulateLambda must additionally track the partially aggregated
    # values that should be outputs that are consumed by the ReportLambda
    # within the same ComposedTeeCall.
    self.partial_aggregate_outputs: list[
        federated_language.framework.Reference
    ] = []

  def to_building_block(
      self, input_type: federated_language.Type
  ) -> federated_language.framework.Lambda:
    """Converts the AccumulateLambda to a proper Lambda computation."""
    local_vals = self.get_processed_local_values('input_arg', input_type)
    return federated_language.framework.Lambda(
        'input_arg',
        input_type,
        federated_language.framework.Block(
            local_vals,
            federated_language.framework.Struct([
                federated_language.framework.Struct(
                    self.outputs_for_future_composed_tee
                ),
                federated_language.framework.Struct(
                    self.partial_aggregate_outputs
                ),
            ]),
        ),
    )


class ReportLambda(BaseLambda):
  """Specialized BaseLambda for the report stage."""

  # The ReportLambda computation takes as input a struct containing two
  # inputs:
  # * A struct of length num_client_workers containing the partially
  #   aggregated values generated by client workers executing the preceding
  #   AccumulateLambda computation.
  # * All other inputs (see the BaseLambda.get_inputs docstring)
  INDEX_OF_PARTIAL_AGGREGATE_INPUTS = 0
  INDEX_OF_BASE_LAMBDA_INPUTS = 1

  def __init__(self, lambda_index):
    super().__init__(lambda_index, TeeStage.REPORT)

    # The ReportLambda must additionally track the partial aggregated value
    # inputs that are generated by the AccumulateLambda within the same
    # ComposedTeeCall.
    self.partial_aggregate_inputs = []

  def to_building_block(
      self,
      input_type: federated_language.StructType,
      num_client_workers: int,
      name_generator: Iterator[str],
  ) -> federated_language.framework.Lambda:
    """Converts the ReportLambda to a proper Lambda computation."""

    # Suppose we have split two aggregation calls, x=federated_aggregate(...)
    # and y=federated_aggregate(...) between the AccumulateLambda and
    # ReportLambda within a given ComposedTeeCall. Part of the output provided
    # by the AccumulateLambda computation will be a struct [x_partial,y_partial]
    # that stores the partial aggregates. The ReportLambda computation will be
    # receiving as part of its input a struct of these structs (because each
    # client worker executing AccumulateLambda will provide its own instance).
    # We need to process this input struct of structs from
    # [[x_partial_1,y_partial_1],[x_partial_2,y_partial_2],...] to
    # x_partial = [x_partial_1, x_partial_2] and
    # y_partial = [y_partial_1, y_partial_2
    # so that the ReportLambda can perform the remainder of the aggregation
    # logic over the partial aggregates.
    all_local_vals = []
    for i, partial_aggregate_name in enumerate(self.partial_aggregate_inputs):
      partial_aggregate_inputs = federated_language.framework.Selection(
          federated_language.framework.Reference('input_arg', input_type),
          index=ReportLambda.INDEX_OF_PARTIAL_AGGREGATE_INPUTS,
      )
      vals_for_partial_aggregate_name = []
      for worker_index in range(num_client_workers):
        partial_aggregate_inputs_for_worker = (
            federated_language.framework.Selection(
                partial_aggregate_inputs, index=worker_index
            )
        )
        vals_for_partial_aggregate_name.append(
            federated_language.framework.Selection(
                partial_aggregate_inputs_for_worker, index=i
            )
        )
      all_local_vals.append((
          partial_aggregate_name,
          federated_language.framework.Struct(vals_for_partial_aggregate_name),
      ))

    # Process the other parts of the input (the parts dependent on the original
    # computation input param or the outputs of prior "composed_tee" calls).
    base_lambda_input_arg = next(name_generator)
    all_local_vals.append((
        base_lambda_input_arg,
        federated_language.framework.Selection(
            federated_language.framework.Reference('input_arg', input_type),
            index=ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS,
        ),
    ))
    all_local_vals.extend(
        self.get_processed_local_values(
            base_lambda_input_arg,
            input_type[ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS],
        )
    )

    return federated_language.framework.Lambda(
        'input_arg',
        input_type,
        federated_language.framework.Block(
            all_local_vals,
            federated_language.framework.Struct(
                self.outputs_for_future_composed_tee
            ),
        ),
    )


class ComposedTeeCall:
  """A class that represents a call to the "composed_tee" intrinsic.

  It holds `BaseLambda` objects associated with each `TeeStage`.
  """

  def __init__(self, lambda_index_start):
    self.accumulate_lambda = AccumulateLambda(lambda_index_start)
    self.report_lambda = ReportLambda(lambda_index_start + 1)

  def get_lambda_for_tee_stage(self, tee_stage: TeeStage) -> BaseLambda:
    if tee_stage == TeeStage.ACCUMULATE:
      return self.accumulate_lambda
    elif tee_stage == TeeStage.REPORT:
      return self.report_lambda
    else:
      raise ValueError('Found unsupported tee stage: ' + str(tee_stage))

  def to_call(
      self,
      composed_tee_call_locals: list[
          tuple[str, federated_language.framework.Call]
      ],
      num_client_workers: int,
      name_generator: Iterator[str],
  ) -> federated_language.framework.Call:
    """Returns a representative "composed_tee" intrinsic call.

    Args:
      composed_tee_call_locals: A list of the (local_name, local_value) tuples
        describing "composed_tee" intrinsic calls constructed thus far.
      num_client_workers: The number of client workers.
      name_generator: An iterator that generates unique names for the local
        values.

    Returns:
      A new "composed_tee" intrinsic call.
    """
    accumulate_input = self.accumulate_lambda.get_inputs(
        composed_tee_call_locals
    )
    accumulate_input_type = accumulate_input.type_signature
    accumulate_lambda_bb = self.accumulate_lambda.to_building_block(
        accumulate_input_type
    )
    accumulate_output_type = accumulate_lambda_bb.type_signature.result
    if not isinstance(accumulate_output_type, federated_language.StructType):
      raise ValueError(
          'Accumulate lambda must return a struct but got: '
          + str(accumulate_output_type)
      )
    if len(accumulate_output_type) != 2:
      raise ValueError(
          'Accumulate lambda must return a struct with two elements but got: '
          + str(accumulate_output_type)
      )

    report_base_input = self.report_lambda.get_inputs(composed_tee_call_locals)
    report_fn_input_type = federated_language.StructType([
        federated_language.StructType(
            [
                accumulate_output_type[
                    AccumulateLambda.INDEX_OF_PARTIAL_AGGREGATE_OUTPUTS
                ]
            ]
            * num_client_workers
        ),
        report_base_input.type_signature,
    ])

    report_lambda_bb = self.report_lambda.to_building_block(
        report_fn_input_type, num_client_workers, name_generator
    )

    output_type = federated_language.StructType([
        accumulate_lambda_bb.type_signature.result[
            AccumulateLambda.INDEX_OF_PRE_AGGREGATE_OUTPUTS
        ],
        report_lambda_bb.type_signature.result,
    ])

    return federated_language.framework.Call(
        federated_language.framework.Intrinsic(
            uri=_COMPOSED_TEE_URI,
            type_signature=federated_language.FunctionType(
                parameter=federated_language.StructType([
                    accumulate_input_type,
                    report_base_input.type_signature,
                    accumulate_lambda_bb.type_signature,
                    report_lambda_bb.type_signature,
                ]),
                result=output_type,
            ),
        ),
        federated_language.framework.Struct([
            accumulate_input,
            report_base_input,
            accumulate_lambda_bb,
            report_lambda_bb,
        ]),
    )


def _get_dependencies(
    input_param_name: str, local_value: federated_language.framework.Call
) -> tuple[
    set[
        Union[
            federated_language.framework.Reference,
            federated_language.framework.Selection,
        ]
    ],
    set[federated_language.framework.Reference],
]:
  """Retrieves dependency information for a given Call.

  Traces the given Call to identify dependencies on the original computation
  input param as well as other References.

  Args:
    input_param_name: The name of the original computation input param.
    local_value: The Call to trace.

  Returns:
    A tuple containing two elements:
      * A set of References or Selections associated with the original input
        param that this Call depends on.
      * A set of References not associated with the original input param that
        this Call depends on.
  """
  # Recording input param top-level Selections and References
  input_param_dependencies = set()

  def record_input_param_dependencies(subvalue):
    if isinstance(subvalue, federated_language.framework.Reference):
      if subvalue.name == input_param_name:
        input_param_dependencies.add(subvalue)
      return subvalue, True
    elif isinstance(subvalue, federated_language.framework.Selection):
      subvalue_iterator = subvalue
      while isinstance(
          subvalue_iterator, federated_language.framework.Selection
      ):
        subvalue_iterator = subvalue_iterator.source
      if (
          isinstance(subvalue_iterator, federated_language.framework.Reference)
          and subvalue_iterator.name == input_param_name
      ):
        input_param_dependencies.add(subvalue)
      return subvalue, True
    return subvalue, False

  federated_language.framework.transform_preorder(
      local_value, record_input_param_dependencies
  )

  # Recording non-input param References
  non_input_param_dependencies = set()

  def record_dependencies(subvalue):
    if isinstance(subvalue, federated_language.framework.Lambda):
      return subvalue, True
    if (
        isinstance(subvalue, federated_language.framework.Reference)
        and subvalue.name != input_param_name
    ):
      non_input_param_dependencies.add(subvalue)
    return subvalue, False

  federated_language.framework.transform_preorder(
      local_value, record_dependencies
  )

  return input_param_dependencies, non_input_param_dependencies


def _get_lambda_to_update(
    non_input_param_dependencies: set[federated_language.framework.Reference],
    tee_stage_to_update: TeeStage,
    composed_tee_calls: list[ComposedTeeCall],
    ref_to_lambda_map: dict[str, BaseLambda],
) -> BaseLambda:
  """Determines the first BaseLambda that can satisfy the given criteria.

  If none of the BaseLambdas associated with the current composed_tee_calls
  list satisfy the provided criteria, this method adds a new ComposedTeeCall
  object to composed_tee_calls.

  Args:
    non_input_param_dependencies: A set of References that must be accessible
      from the BaseLambda (meaning they must be produced either by the
      BaseLambda or by a different BaseLambda that will run earlier).
    tee_stage_to_update: The TeeStage that the BaseLambda must be associated
      with.
    composed_tee_calls: A list of the ComposedTeeCall objects constructed thus
      far.
    ref_to_lambda_map: A dictionary mapping Reference names to the BaseLambda
      that owns the call associated with the Reference name.

  Returns:
    The BaseLambda to update.
  """
  # Find the first eligible BaseLambda that satisfies the dependency criteria.
  lambda_index = 0
  for dependency in non_input_param_dependencies:
    if dependency.name not in ref_to_lambda_map:
      raise ValueError(
          'Dependency key not found in ref_to_lambda_map: '
          + str(dependency.name)
      )
    lambda_index = max(
        lambda_index, ref_to_lambda_map[dependency.name].lambda_index
    )

  # Find the next BaseLambda that satisfies the TeeStage criteria.
  while lambda_index % TeeStage.MAX_STAGES != tee_stage_to_update:
    lambda_index += 1
    if lambda_index >= len(composed_tee_calls) * TeeStage.MAX_STAGES:
      composed_tee_calls.append(
          ComposedTeeCall(
              lambda_index_start=len(composed_tee_calls) * TeeStage.MAX_STAGES
          )
      )

  # Retrieve the selected BaseLambda.
  return composed_tee_calls[
      int(lambda_index / TeeStage.MAX_STAGES)
  ].get_lambda_for_tee_stage(tee_stage_to_update)


def _link_dependencies(
    input_param_dependencies: set[
        Union[
            federated_language.framework.Reference,
            federated_language.framework.Selection,
        ]
    ],
    non_input_param_dependencies: set[federated_language.framework.Reference],
    current_lambda: BaseLambda,
    ref_to_lambda_map: dict[str, BaseLambda],
):
  """Processes dependency requirements across BaseLambdas.

  Args:
    input_param_dependencies: A set of References or Selections describing
      dependencies current_lambda must be able to have on the original
      computation input param.
    non_input_param_dependencies: A set of References not associated with the
      original computation input param that current_lambda must be able to
      resolve.
    current_lambda: A BaseLambda that may need to record additional dependencies
      on the original input param or on prior lambdas, depending on the values
      in the preceding args.
    ref_to_lambda_map: A dictionary mapping Reference names to the BaseLambda
      that owns the call associated with the Reference name.
  """
  for dependency in input_param_dependencies:
    if dependency not in current_lambda.inputs_from_original_computation_param:
      current_lambda.inputs_from_original_computation_param.append(dependency)

  for dependency in non_input_param_dependencies:
    if dependency.name not in ref_to_lambda_map:
      raise ValueError(
          'Dependency key not found in ref_to_lambda_map: '
          + str(dependency.name)
      )
    lambda_producing_dependency_ref = ref_to_lambda_map[dependency.name]

    # If current_lambda is the BaseLambda that already produces the value
    # associated with `dependency``, there is no need to adjust the inputs/
    # outputs of any BaseLambdas.
    if lambda_producing_dependency_ref == current_lambda:
      continue

    # Ensure the BaseLambda that produces the value associated with `dependency`
    # will provide it as an output.
    if (
        dependency
        not in lambda_producing_dependency_ref.outputs_for_future_composed_tee
    ):
      lambda_producing_dependency_ref.outputs_for_future_composed_tee.append(
          dependency
      )

    # Record information that ensures current_lambda will be able to receive
    # an input value that can be processed to retrieve the `dependency` value.
    # See the comment describing the inputs_from_previous_composed_tee dict
    # in BaseLambda for an explanation of why the keys and values are structured
    # the way they are.
    tee_index_and_stage_producing_dependency_ref = (
        int(lambda_producing_dependency_ref.lambda_index / TeeStage.MAX_STAGES),
        lambda_producing_dependency_ref.tee_stage,
    )
    dep_name_and_output_index = (
        dependency.name,
        lambda_producing_dependency_ref.outputs_for_future_composed_tee.index(
            dependency
        ),
    )
    if (
        tee_index_and_stage_producing_dependency_ref
        not in current_lambda.inputs_from_previous_composed_tee
    ):
      current_lambda.inputs_from_previous_composed_tee[
          tee_index_and_stage_producing_dependency_ref
      ] = set()
    current_lambda.inputs_from_previous_composed_tee[
        tee_index_and_stage_producing_dependency_ref
    ].add(dep_name_and_output_index)


def _distribute_federated_aggregate_accumulate_logic(
    value: federated_language.framework.Reference,
    zero: federated_language.framework.Reference,
    accumulate_fn: federated_language.framework.Lambda,
    accumulate_lambda: AccumulateLambda,
    name_generator: Iterator[str],
) -> str:
  """Adds federated_aggregate accumulate logic to an AccumulateLambda.

  Args:
    value: The value to be aggregated.
    zero: The zero value of the original federated_aggregate call.
    accumulate_fn: The accumulate_fn of the original federated_aggregate call.
    accumulate_lambda: The AccumulateLambda that should receive the accumulate
      portion of the aggregation.
    name_generator: An iterator that generates unique names for the local
      values.

  Returns:
    The name assigned to the partial aggregate result in the AccumulateLambda.
  """
  if not isinstance(value.type_signature, federated_language.FederatedType):
    raise ValueError(
        'Value must be a federated value, but is instead: '
        + str(value.type_signature)
    )

  zero_type = zero.type_signature
  ignore_merge_fn = federated_language.framework.Lambda(
      'input_arg',
      federated_language.StructType([zero_type, zero_type]),
      federated_language.framework.Block(
          [],
          federated_language.framework.Selection(
              federated_language.framework.Reference(
                  'input_arg',
                  federated_language.StructType([zero_type, zero_type]),
              ),
              index=0,
          ),
      ),
  )
  identity_report_fn = federated_language.framework.Lambda(
      'input_arg',
      zero_type,
      federated_language.framework.Block(
          [], federated_language.framework.Reference('input_arg', zero_type)
      ),
  )

  federated_aggregate_call_for_accumulate_lambda = federated_language.framework.Call(
      federated_language.framework.Intrinsic(
          uri=federated_language.framework.FEDERATED_AGGREGATE.uri,
          type_signature=federated_language.FunctionType(
              parameter=federated_language.StructType([
                  federated_language.FederatedType(
                      value.type_signature.member,
                      value.type_signature.placement,
                  ),
                  zero_type,
                  accumulate_fn.type_signature,
                  ignore_merge_fn.type_signature,
                  identity_report_fn.type_signature,
              ]),
              result=federated_language.FederatedType(
                  zero_type, federated_language.SERVER
              ),
          ),
      ),
      federated_language.framework.Struct([
          value,
          zero,
          accumulate_fn,
          ignore_merge_fn,  # merge step won't actually run on client workers
          identity_report_fn,  # report step will run
      ]),
  )
  partial_aggregate_name = next(name_generator)
  accumulate_lambda.local_vals.append((
      partial_aggregate_name,
      federated_aggregate_call_for_accumulate_lambda,
  ))
  accumulate_lambda.partial_aggregate_outputs.append(
      federated_language.framework.Reference(
          partial_aggregate_name,
          federated_language.FederatedType(
              zero_type, federated_language.SERVER
          ),
      )
  )
  return partial_aggregate_name


def _distribute_federated_aggregate_merge_logic(
    partial_aggregate_name: str,
    zero: federated_language.framework.Reference,
    merge_fn: federated_language.framework.Lambda,
    num_client_workers: int,
    report_lambda: ReportLambda,
    name_generator: Iterator[str],
) -> str:
  """Adds federated_aggregate merge logic to a ReportLambda.

  Creates a loop that merges together the results received from
  num_client_workers workers.

  Args:
    partial_aggregate_name: The name the ReportLambda should assign to the
      combined partial aggregate results received from the previous
      AccumulateLambda.
    zero: The zero value of the original federated_aggregate call.
    merge_fn: The merge_fn of the original federated_aggregate call.
    num_client_workers: The number of client workers that will be executing the
      accumulate portion of the aggregation.
    report_lambda: The ReportLambda that should receive the merge portion of the
      aggregation.
    name_generator: An iterator that generates unique names for the local
      values.

  Returns:
    The name assigned to the final merge result in the ReportLambda.
  """
  zero_type = zero.type_signature
  federated_aggregate_merge_result_name = next(name_generator)

  # Ensure that the partial aggregate result is provided as an input to the
  # ReportLambda.
  report_lambda.partial_aggregate_inputs.append(partial_aggregate_name)

  # Initialize the merge result to zero.
  report_lambda.local_vals.append((
      federated_aggregate_merge_result_name,
      federated_language.framework.Call(
          federated_language.framework.Intrinsic(
              uri=federated_language.framework.FEDERATED_VALUE_AT_SERVER.uri,
              type_signature=federated_language.FunctionType(
                  parameter=zero_type,
                  result=federated_language.FederatedType(
                      zero_type, federated_language.SERVER
                  ),
              ),
          ),
          zero,
      ),
  ))
  # Merge the current merge result with the partial accumulate value from
  # each client worker. This must be done sequentially for each worker.
  for i in range(num_client_workers):
    federated_aggregate_merge_zip_name = next(name_generator)
    report_lambda.local_vals.append((
        federated_aggregate_merge_zip_name,
        federated_language.framework.Call(
            federated_language.framework.Intrinsic(
                uri=federated_language.framework.FEDERATED_ZIP_AT_SERVER.uri,
                type_signature=federated_language.FunctionType(
                    parameter=federated_language.StructType([
                        federated_language.FederatedType(
                            zero_type, federated_language.SERVER
                        ),
                        federated_language.FederatedType(
                            zero_type, federated_language.SERVER
                        ),
                    ]),
                    result=federated_language.FederatedType(
                        federated_language.StructType([zero_type, zero_type]),
                        federated_language.SERVER,
                    ),
                ),
            ),
            federated_language.framework.Struct([
                federated_language.framework.Reference(
                    federated_aggregate_merge_result_name,
                    federated_language.FederatedType(
                        zero_type, federated_language.SERVER
                    ),
                ),
                federated_language.framework.Selection(
                    federated_language.framework.Reference(
                        partial_aggregate_name,
                        federated_language.StructType(
                            [
                                federated_language.FederatedType(
                                    zero_type, federated_language.SERVER
                                )
                            ]
                            * num_client_workers
                        ),
                    ),
                    index=i,
                ),
            ]),
        ),
    ))
    federated_aggregate_next_merge_result_name = next(name_generator)
    report_lambda.local_vals.append((
        federated_aggregate_next_merge_result_name,
        federated_language.framework.Call(
            federated_language.framework.Intrinsic(
                uri=federated_language.framework.FEDERATED_APPLY.uri,
                type_signature=federated_language.FunctionType(
                    parameter=federated_language.StructType([
                        federated_language.FunctionType(
                            parameter=federated_language.StructType(
                                [zero_type, zero_type]
                            ),
                            result=zero_type,
                        ),
                        federated_language.FederatedType(
                            federated_language.StructType(
                                [zero_type, zero_type]
                            ),
                            federated_language.SERVER,
                        ),
                    ]),
                    result=federated_language.FederatedType(
                        zero_type, federated_language.SERVER
                    ),
                ),
            ),
            federated_language.framework.Struct([
                merge_fn,
                federated_language.framework.Reference(
                    federated_aggregate_merge_zip_name,
                    federated_language.FederatedType(
                        federated_language.StructType([zero_type, zero_type]),
                        federated_language.SERVER,
                    ),
                ),
            ]),
        ),
    ))
    federated_aggregate_merge_result_name = (
        federated_aggregate_next_merge_result_name
    )
  return federated_aggregate_merge_result_name


def _distribute_federated_aggregate_report_logic(
    merge_result_name: str,
    report_fn: federated_language.framework.Lambda,
    report_lambda: ReportLambda,
    original_local_name: str,
):
  """Adds federated_aggregate report logic to a ReportLambda.

  Args:
    merge_result_name: The name assigned to the final merge result in the
      ReportLambda after running the merge logic.
    report_fn: The report_fn of the original federated_aggregate call.
    report_lambda: The ReportLambda that should receive the report portion of
      the aggregation.
    original_local_name: The name of the original local value that the
      federated_aggregate call was associated with.
  """
  merge_output_type = federated_language.FederatedType(
      report_fn.type_signature.parameter, federated_language.SERVER
  )
  report_output_type = federated_language.FederatedType(
      report_fn.type_signature.result, federated_language.SERVER
  )
  report_lambda.local_vals.append((
      original_local_name,
      federated_language.framework.Call(
          federated_language.framework.Intrinsic(
              uri=federated_language.framework.FEDERATED_APPLY.uri,
              type_signature=federated_language.FunctionType(
                  parameter=federated_language.StructType(
                      [report_fn.type_signature, merge_output_type]
                  ),
                  result=report_output_type,
              ),
          ),
          federated_language.framework.Struct([
              report_fn,
              federated_language.framework.Reference(
                  merge_result_name, merge_output_type
              ),
          ]),
      ),
  ))


def _distribute_federated_aggregate_call(
    local_name: str,
    local_value: federated_language.framework.Call,
    accumulate_lambda: AccumulateLambda,
    report_lambda: ReportLambda,
    num_client_workers: int,
    name_generator: Iterator[str],
):
  """Distributes a federated_aggregate call across two BaseLambdas.

  Args:
    local_name: The name associated with the result of local_value in the
      original computation.
    local_value: The federated_aggregate call to distribute.
    accumulate_lambda: The AccumulateLambda that should receive the accumulate
      portion of the aggregation.
    report_lambda: The ReportLambda that should receive the merge+report
      portions of the aggregation.
    num_client_workers: The number of client workers that will be executing the
      accumulate_lambda.
    name_generator: An iterator that generates unique names for the local
      values.
  """
  if not isinstance(local_value.argument, federated_language.framework.Struct):
    raise ValueError(
        'Expected arg_type to be a struct but got: ' + str(local_value.argument)
    )
  value, zero, accumulate_fn, merge_fn, report_fn = local_value.argument
  partial_aggregate_result_name = (
      _distribute_federated_aggregate_accumulate_logic(
          value,
          zero,
          accumulate_fn,
          accumulate_lambda,
          name_generator,
      )
  )
  merge_result_name = _distribute_federated_aggregate_merge_logic(
      partial_aggregate_result_name,
      zero,
      merge_fn,
      num_client_workers,
      report_lambda,
      name_generator,
  )
  _distribute_federated_aggregate_report_logic(
      merge_result_name, report_fn, report_lambda, local_name
  )


def is_called_intrinsic(
    comp: federated_language.framework.ComputationBuildingBlock,
) -> bool:
  """Returns whether the computation is a call to an intrinsic."""
  return isinstance(comp, federated_language.framework.Call) and isinstance(
      comp.function, federated_language.framework.Intrinsic
  )


def to_composed_tee_form(
    comp: federated_language.framework.ConcreteComputation,
    num_client_workers: int,
) -> federated_language.framework.ConcreteComputation:
  """Transforms a computation into composed tee form.

  A computation in composed tee form is a Lambda computation with a result
  Block with an arbitrary number of local values that are exclusively calls to
  the "composed_tee" intrinsic.

  A "composed_tee" intrinsic call takes four args with the following types
  (<X, ...y> denotes a struct with y elements of type X):
    * accumulate_arg: 'A'
    * partial_report_arg: 'D'
    * accumulate_fn: '(A -> <B, C>)'
    * report_fn: '(<<C, ...num_client_workers>, D> -> E)'

  The "composed_tee" intrinsic itself has the following type:
  '(<A, D, (A -> <B,C>), (<<C, ...num_client_workers>,D> -> E)> ->
  <{B}@CLIENTS,E>)'

  During execution, each "composed_tee" intrinsic call should be executed as
  follows over many client child workers and one client server worker to
  maintain equivalence to the original computation:

  1. For each client child worker, run accumulate_fn over the parts of
      accumulate_arg assigned to that client child worker, which should produce
      a struct with two values: pre-aggregate values and partial aggregate
      values.
  2. Create a CLIENTS-placed federated value (combined_pre_aggregate_values)
      and populate it with the first element of each accumulate_fn result from
      step 1.
  3. Create a struct (combined_partial_aggregate_values) and populate it with
      the second element of each accumulate_fn result from step 1.
  4. Using the server child worker, run report_fn over a struct containing
      combined_partial_aggregate_values and partial_report_arg.
  6. Return a struct with two elements: combined_pre_aggregate_values and
      the result of step 4.

  Currently, the "composed_tee" intrinsic does not support execution using
  multiple layers of hierarchy.

  Args:
    comp: An instance of `federated_language.framework.ConcreteComputation` to
      convert into composed tee form. All Lambda computations are allowed; there
      are no restrictions on the number or placement of inputs/outputs.
    num_client_workers: The number of client workers to use when executing the
      resulting computation.

  Returns:
    An instance of `federated_language.framework.ConcreteComputation` in
    composed tee form.
  """

  # Convert all aggregation intrinsics to "federated_aggregate" calls so that
  # we only have to split one type of aggregation call later. If we decide to
  # integrate AggCores into TFF, this will be removed.
  comp_bb, _ = tff.tensorflow.replace_intrinsics_with_bodies(
      comp.to_building_block()
  )
  # Convert to CallDominantForm to regularize the structure of the computation.
  comp_bb = tff.framework.to_call_dominant(comp_bb)

  # Check that the computation is a Lambda with a Block result.
  if not isinstance(comp_bb, federated_language.framework.Lambda):
    raise ValueError(
        'Expected the computation to be a Lambda but got: ' + str(comp_bb)
    )
  lambda_block = comp_bb.result
  if not isinstance(lambda_block, federated_language.framework.Block):
    raise ValueError(
        'Expected the result of the computation to be a Block but got: '
        + str(lambda_block)
    )

  # Construct a list of local values to greedily iterate through to construct
  # the composed tee form. Include the final output of the computation as the
  # last element of the list.
  locals_to_process = lambda_block.locals
  name_generator = federated_language.framework.unique_name_generator(comp_bb)
  final_output_name = next(name_generator)
  locals_to_process.append((final_output_name, lambda_block.result))

  # Initialize a list of ComposedTeeCall objects, each of which represents a
  # "composed_tee" intrinsic call. Populate the list with an initial
  # ComposedTeeCall object. As we iterate through the locals_to_process list,
  # we will assign each local value to the first eligible ComposedTeeCall object
  # and extend this list with new ComposedTeeCall objects only when necessary.
  composed_tees = [ComposedTeeCall(lambda_index_start=0)]

  # Initialize a map from local name to BaseLambda that will be used to link
  # dependencies across BaseLambdas.
  ref_to_lambda_map = dict()

  # Iterate over the locals, assigning them to the first BaseLambda possible
  # and updating dependency information across BaseLambdas. This assignment
  # process takes into consideration the dependencies of each local, as well as
  # whether the local should be executed by a client worker or server worker.
  # It should be valid because we've transformed the computation into Call
  # Dominant Form, which ensures that each local_name has a unique name and that
  # each local_value will only depend on locals that have already been
  # processed.
  for local_name, local_value in locals_to_process:
    input_param_dependencies, non_input_param_dependencies = _get_dependencies(
        comp_bb.parameter_name, local_value
    )

    if (
        not is_called_intrinsic(local_value)
        or local_value.function.uri in _INTRINSIC_URI_TO_TEE_STAGE_MAP
    ):
      tee_stage = TeeStage.REPORT
      if is_called_intrinsic(local_value):
        tee_stage = _INTRINSIC_URI_TO_TEE_STAGE_MAP[local_value.function.uri]

      # Determine the BaseLambda to which this local will be distributed.
      current_lambda = _get_lambda_to_update(
          non_input_param_dependencies,
          tee_stage,
          composed_tees,
          ref_to_lambda_map,
      )
      current_lambda.local_vals.append((local_name, local_value))

      # Update dependency information.
      _link_dependencies(
          input_param_dependencies,
          non_input_param_dependencies,
          current_lambda,
          ref_to_lambda_map,
      )
      ref_to_lambda_map[local_name] = current_lambda
    elif (
        local_value.function.uri
        == federated_language.framework.FEDERATED_AGGREGATE.uri
    ):
      # Determine the AccumulateLambda and ReportLambda to which this
      # federated_aggregate call should be distributed.
      accumulate_lambda = _get_lambda_to_update(
          non_input_param_dependencies,
          TeeStage.ACCUMULATE,
          composed_tees,
          ref_to_lambda_map,
      )
      report_lambda = composed_tees[
          int(accumulate_lambda.lambda_index / TeeStage.MAX_STAGES)
      ].get_lambda_for_tee_stage(TeeStage.REPORT)
      _distribute_federated_aggregate_call(
          local_name,
          local_value,
          accumulate_lambda,
          report_lambda,
          num_client_workers,
          name_generator,
      )
      # The AccumulateLambda will depend on the same values that the original
      # "federated_aggregate" call depended on.
      _link_dependencies(
          input_param_dependencies,
          non_input_param_dependencies,
          accumulate_lambda,
          ref_to_lambda_map,
      )
      # Apart from the partial aggregates, the ReportLambda will depend only
      # on the values that the "zero" call within the original
      # "federated_aggregate" call depended on.
      (
          input_param_dependencies_for_zero_arg,
          non_input_param_dependencies_for_zero_arg,
      ) = _get_dependencies(comp_bb.parameter_name, local_value.argument[1])
      _link_dependencies(
          input_param_dependencies_for_zero_arg,
          non_input_param_dependencies_for_zero_arg,
          report_lambda,
          ref_to_lambda_map,
      )
      ref_to_lambda_map[local_name] = report_lambda
    else:
      raise ValueError(
          'Found unsupported intrinsic uri: ' + local_value.function.uri
      )

  # Ensure that the last BaseLambda will return the same output as the original
  # computation.
  if final_output_name not in ref_to_lambda_map:
    raise ValueError(
        'The final output name is not in ref_to_lambda_map: '
        + str(final_output_name)
    )
  lambda_with_result = ref_to_lambda_map[final_output_name]
  if lambda_with_result.tee_stage != TeeStage.REPORT:
    raise ValueError(
        'The tee stage of the lambda with final output is not REPORT: '
        + str(final_output_name)
    )
  if (
      lambda_with_result.lambda_index
      != len(composed_tees) * TeeStage.MAX_STAGES - 1
  ):
    raise ValueError('The lambda with the final output is not the last lambda.')
  lambda_with_result.outputs_for_future_composed_tee.append(
      federated_language.framework.Reference(
          final_output_name, comp_bb.type_signature.result
      )
  )

  # Convert all of the ComposedTeeCall objects to actual "composed_tee"
  # intrinsic calls.
  composed_tee_calls = []
  for composed_tee in composed_tees:
    composed_tee_calls.append((
        next(name_generator),
        composed_tee.to_call(
            composed_tee_calls, num_client_workers, name_generator
        ),
    ))

  # Return the final Lambda computation in composed tee form. The final output
  # will be the first (and only) element of the REPORT TeeStage's output from
  # the last "composed_tee" call.
  new_comp = federated_language.framework.Lambda(
      comp_bb.parameter_name,
      comp_bb.type_signature.parameter,
      federated_language.framework.Block(
          composed_tee_calls,
          federated_language.framework.Selection(
              federated_language.framework.Selection(
                  federated_language.framework.Reference(
                      composed_tee_calls[-1][0],
                      composed_tee_calls[-1][1].type_signature,
                  ),
                  index=_TEE_STAGE_OUTPUT_POSITION_MAP[TeeStage.REPORT],
              ),
              index=0,
          ),
      ),
  )

  return federated_language.framework.ConcreteComputation.from_building_block(
      tff.framework.to_call_dominant(new_comp)
  )
