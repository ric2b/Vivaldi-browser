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
from absl.testing import absltest
from absl.testing import parameterized
import federated_language
import numpy as np
import tensorflow_federated as tff

from fcp.confidentialcompute.python import compiler


# Helper function to create the input to the AccumulateLambda computation.
# Returns the input and its type signature.
def _create_accumulate_input(
    input_from_original_computation: list[
        federated_language.framework.ComputationBuildingBlock
    ],
    input_from_previous_composed_tee: list[
        federated_language.framework.ComputationBuildingBlock
    ],
) -> tuple[federated_language.framework.Struct, federated_language.StructType]:
  complete_accumulate_input = federated_language.framework.Struct([
      federated_language.framework.Struct(input_from_original_computation),
      federated_language.framework.Struct(input_from_previous_composed_tee),
  ])
  complete_accumulate_input_type = federated_language.StructType([
      federated_language.StructType(
          [x.type_signature for x in input_from_original_computation]
      ),
      federated_language.StructType(
          [x.type_signature for x in input_from_previous_composed_tee]
      ),
  ])
  return complete_accumulate_input, complete_accumulate_input_type


# Helper function to create the input to the ReportLambda computation.
# Returns a tuple containing the base input (no partial aggregate information
# included), the base input type (no partial aggregate information included),
# and the complete input type (with partial aggregate information included).
def _create_report_input(
    accumulate_fn: federated_language.framework.ComputationBuildingBlock,
    num_client_workers: int,
    input_from_original_computation: list[
        federated_language.framework.ComputationBuildingBlock
    ],
    input_from_previous_composed_tee: list[
        federated_language.framework.ComputationBuildingBlock
    ],
) -> tuple[
    federated_language.framework.Struct,
    federated_language.StructType,
    federated_language.StructType,
]:
  base_report_input = federated_language.framework.Struct([
      federated_language.framework.Struct(input_from_original_computation),
      federated_language.framework.Struct(input_from_previous_composed_tee),
  ])
  base_report_input_type = federated_language.StructType([
      federated_language.StructType(
          [x.type_signature for x in input_from_original_computation]
      ),
      federated_language.StructType(
          [x.type_signature for x in input_from_previous_composed_tee]
      ),
  ])
  complete_report_input_type = federated_language.StructType([
      federated_language.StructType(
          [
              accumulate_fn.type_signature.result[
                  compiler.AccumulateLambda.INDEX_OF_PARTIAL_AGGREGATE_OUTPUTS
              ]
          ]
          * num_client_workers
      ),
      base_report_input_type,
  ])
  return base_report_input, base_report_input_type, complete_report_input_type


# Helper function to create a "composed_tee" call and a reference to it.
def _create_composed_tee_call_and_reference(
    name: str,
    accumulate_input: federated_language.framework.Struct,
    accumulate_input_type: federated_language.StructType,
    base_report_input: federated_language.framework.Struct,
    base_report_input_type: federated_language.StructType,
    accumulate_fn: federated_language.framework.ComputationBuildingBlock,
    report_fn: federated_language.framework.ComputationBuildingBlock,
) -> tuple[
    federated_language.framework.Call, federated_language.framework.Reference
]:
  call_bb = federated_language.framework.Call(
      federated_language.framework.Intrinsic(
          uri=compiler._COMPOSED_TEE_URI,
          type_signature=federated_language.FunctionType(
              parameter=federated_language.StructType([
                  accumulate_input_type,
                  base_report_input_type,
                  accumulate_fn.type_signature,
                  report_fn.type_signature,
              ]),
              result=federated_language.StructType([
                  accumulate_fn.type_signature.result[
                      compiler.AccumulateLambda.INDEX_OF_PRE_AGGREGATE_OUTPUTS
                  ],
                  report_fn.type_signature.result,
              ]),
          ),
      ),
      federated_language.framework.Struct([
          accumulate_input,
          base_report_input,
          accumulate_fn.to_building_block(),
          report_fn.to_building_block(),
      ]),
  )
  ref_bb = federated_language.framework.Reference(name, call_bb.type_signature)
  return call_bb, ref_bb


# Helper function to create a lambda computation in composed tee form.
def _create_composed_tee_form_lambda(
    original_comp_param_name: str,
    original_comp_param_type: federated_language.Type,
    composed_tee_calls: list[
        tuple[
            federated_language.framework.Call,
            federated_language.framework.Reference,
        ]
    ],
) -> federated_language.framework.Lambda:
  locals_list: list[tuple[str, federated_language.framework.Call]] = []
  for composed_tee_call, ref_to_composed_tee_call in composed_tee_calls:
    locals_list.append((ref_to_composed_tee_call.name, composed_tee_call))
  result = federated_language.framework.Selection(
      federated_language.framework.Selection(
          composed_tee_calls[-1][1],  # reference to the final composed tee call
          index=compiler._TEE_STAGE_OUTPUT_POSITION_MAP[
              compiler.TeeStage.REPORT
          ],
      ),
      index=0,
  )
  return tff.framework.to_call_dominant(
      federated_language.framework.Lambda(
          original_comp_param_name,
          original_comp_param_type,
          federated_language.framework.Block(locals_list, result),
      )
  )


_, _ZERO, _SUM_FN, _, _ = tff.tensorflow.replace_intrinsics_with_bodies(
    federated_language.framework.Intrinsic(
        federated_language.framework.FEDERATED_SUM.uri,
        federated_language.FunctionType(
            federated_language.FederatedType(
                np.int32, federated_language.CLIENTS
            ),
            federated_language.FederatedType(
                np.int32, federated_language.SERVER
            ),
        ),
    )
)[0].result.argument

_IGNORE_MERGE_FN = federated_language.framework.Lambda(
    'input_arg',
    federated_language.StructType([np.int32, np.int32]),
    federated_language.framework.Block(
        [],
        federated_language.framework.Selection(
            federated_language.framework.Reference(
                'input_arg', federated_language.StructType([np.int32, np.int32])
            ),
            index=0,
        ),
    ),
)

_IDENTITY_REPORT_FN = federated_language.framework.Lambda(
    'input_arg',
    federated_language.TensorType(np.int32),
    federated_language.framework.Block(
        [],
        federated_language.framework.Reference(
            'input_arg', federated_language.TensorType(np.int32)
        ),
    ),
)


class CompilerTest(parameterized.TestCase):

  # Checks that the provided comp is in composed tee form.
  def check_valid_composed_tee_form(self, comp):
    self.assertIsInstance(
        comp, federated_language.framework.ConcreteComputation
    )
    self.assertIsInstance(
        comp.to_building_block(), federated_language.framework.Lambda
    )
    self.assertIsInstance(
        comp.to_building_block().result, federated_language.framework.Block
    )
    for _, local_value in comp.to_building_block().result.locals:
      self.assertIsInstance(local_value, federated_language.framework.Call)
      self.assertIsInstance(
          local_value.function, federated_language.framework.Intrinsic
      )
      self.assertEqual(local_value.function.uri, compiler._COMPOSED_TEE_URI)

  # Checks that the provided computations are equal after updating the reference
  # names in the provided computations to draw from a predictable namespace.
  def check_computations_equal(self, comp1, comp2):

    def _regularize_ref_names(comp):
      old_name_to_new_name = {}

      def _get_new_name(old_name):
        if old_name not in old_name_to_new_name:
          old_name_to_new_name[old_name] = 'var_' + str(
              len(old_name_to_new_name)
          )
        return old_name_to_new_name[old_name]

      def _replace_ref_names(subvalue):
        # Update the Lambda parameter name.
        if isinstance(subvalue, federated_language.framework.Lambda):
          return (
              federated_language.framework.Lambda(
                  _get_new_name(subvalue.parameter_name),
                  subvalue.parameter_type,
                  subvalue.result,
              ),
              True,
          )

        # Update the local names in a Block and sort the locals alphabetically
        # by local name.
        if isinstance(subvalue, federated_language.framework.Block):
          new_locals = []
          for local_name, local_value in subvalue.locals:
            new_locals.append((_get_new_name(local_name), local_value))
          sorted_new_locals = sorted(new_locals, key=lambda x: x[0])
          return (
              federated_language.framework.Block(
                  sorted_new_locals, subvalue.result
              ),
              True,
          )

        # Update the Reference name.
        if isinstance(subvalue, federated_language.framework.Reference):
          return (
              federated_language.framework.Reference(
                  _get_new_name(subvalue.name), subvalue.type_signature
              ),
              True,
          )

        return subvalue, False

      return federated_language.framework.transform_postorder(
          comp, _replace_ref_names
      )[0]

    self.assertEqual(
        _regularize_ref_names(comp1).compact_representation(),
        _regularize_ref_names(comp2).compact_representation(),
    )

  @parameterized.named_parameters(
      ('one_client_executor', 1),
      ('two_client_executors', 2),
      ('third_client_executors', 3),
  )
  def test_scalar_server_state(self, num_client_workers):

    @tff.tensorflow.computation(np.int32)
    def double(value):
      return value * 2

    client_data_type = federated_language.FederatedType(
        np.int32, federated_language.CLIENTS
    )
    server_state_type = federated_language.FederatedType(
        np.int32, federated_language.SERVER
    )

    @federated_language.federated_computation(
        [client_data_type, server_state_type]
    )
    def comp_fn(client_data, server_state):
      summed_client_data = federated_language.federated_sum(client_data)
      return (
          federated_language.federated_map(double, server_state),
          summed_client_data,
      )

    comp_fn_composed_tee_form = compiler.to_composed_tee_form(
        comp_fn, num_client_workers
    )
    self.check_valid_composed_tee_form(comp_fn_composed_tee_form)

    original_comp_param_name = 'original_comp_param'
    original_comp_param = federated_language.framework.Reference(
        original_comp_param_name, comp_fn.type_signature.parameter
    )
    client_data = federated_language.framework.Selection(
        original_comp_param, index=0
    )
    server_state = federated_language.framework.Selection(
        original_comp_param, index=1
    )

    accumulate_input_1, accumulate_input_type_1 = _create_accumulate_input(
        input_from_original_computation=[],
        input_from_previous_composed_tee=[],
    )

    @federated_language.federated_computation(accumulate_input_type_1)
    def expected_accumulate_fn_1(value):
      del value
      pre_aggregate_values = []
      partial_aggregate_values = []
      return [pre_aggregate_values, partial_aggregate_values]

    base_report_input_1, base_report_input_type_1, full_report_input_type_1 = (
        _create_report_input(
            accumulate_fn=expected_accumulate_fn_1,
            num_client_workers=num_client_workers,
            input_from_original_computation=[server_state],
            input_from_previous_composed_tee=[],
        )
    )

    @federated_language.federated_computation(full_report_input_type_1)
    def expected_report_fn_1(value):
      server_state = value[compiler.ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS][
          compiler.BaseLambda.INDEX_OF_ORIGINAL_PARAM_DERIVED_INPUTS
      ][0]
      return [_ZERO, federated_language.federated_map(double, server_state)]

    composed_tee_call_1, ref_to_composed_tee_call_1 = (
        _create_composed_tee_call_and_reference(
            name='composed_tee_call_1',
            accumulate_input=accumulate_input_1,
            accumulate_input_type=accumulate_input_type_1,
            base_report_input=base_report_input_1,
            base_report_input_type=base_report_input_type_1,
            accumulate_fn=expected_accumulate_fn_1,
            report_fn=expected_report_fn_1,
        )
    )

    accumulate_input_2, accumulate_input_type_2 = _create_accumulate_input(
        input_from_original_computation=[client_data],
        input_from_previous_composed_tee=[
            federated_language.framework.Selection(
                ref_to_composed_tee_call_1,
                index=compiler._TEE_STAGE_OUTPUT_POSITION_MAP[
                    compiler.TeeStage.REPORT
                ],
            )
        ],
    )

    @federated_language.federated_computation(accumulate_input_type_2)
    def expected_accumulate_fn_2(value):
      client_data = value[
          compiler.AccumulateLambda.INDEX_OF_ORIGINAL_PARAM_DERIVED_INPUTS
      ][0]
      zero = value[
          compiler.AccumulateLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS
      ][0][0]
      pre_aggregate_values = []
      partial_aggregate_values = [
          federated_language.federated_aggregate(
              client_data, zero, _SUM_FN, _IGNORE_MERGE_FN, _IDENTITY_REPORT_FN
          )
      ]
      return [pre_aggregate_values, partial_aggregate_values]

    base_report_input_2, base_report_input_type_2, full_report_input_type_2 = (
        _create_report_input(
            accumulate_fn=expected_accumulate_fn_2,
            num_client_workers=num_client_workers,
            input_from_original_computation=[],
            input_from_previous_composed_tee=[
                federated_language.framework.Selection(
                    ref_to_composed_tee_call_1,
                    index=compiler._TEE_STAGE_OUTPUT_POSITION_MAP[
                        compiler.TeeStage.REPORT
                    ],
                )
            ],
        )
    )

    @federated_language.federated_computation(full_report_input_type_2)
    def expected_report_fn_2(value):
      partially_aggregated_values = value[
          compiler.ReportLambda.INDEX_OF_PARTIAL_AGGREGATE_INPUTS
      ]
      federated_sum_zero = value[
          compiler.ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS
      ][compiler.BaseLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS][0][0]
      current_merge_result = federated_language.federated_value(
          federated_sum_zero, federated_language.SERVER
      )
      for i in range(num_client_workers):
        current_merge_result = federated_language.federated_map(
            _SUM_FN,
            federated_language.federated_zip(
                [current_merge_result, partially_aggregated_values[i][0]]
            ),
        )
      final_aggregate = federated_language.federated_map(
          _IDENTITY_REPORT_FN, current_merge_result
      )

      doubled_server_state = value[
          compiler.ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS
      ][compiler.BaseLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS][0][1]

      return [[doubled_server_state, final_aggregate]]

    composed_tee_call_2, ref_to_composed_tee_call_2 = (
        _create_composed_tee_call_and_reference(
            name='composed_tee_call_2',
            accumulate_input=accumulate_input_2,
            accumulate_input_type=accumulate_input_type_2,
            base_report_input=base_report_input_2,
            base_report_input_type=base_report_input_type_2,
            accumulate_fn=expected_accumulate_fn_2,
            report_fn=expected_report_fn_2,
        )
    )

    expected_comp = _create_composed_tee_form_lambda(
        original_comp_param_name,
        comp_fn.type_signature.parameter,
        [
            (composed_tee_call_1, ref_to_composed_tee_call_1),
            (composed_tee_call_2, ref_to_composed_tee_call_2),
        ],
    )

    self.check_computations_equal(
        comp_fn_composed_tee_form.to_building_block(), expected_comp
    )

  @parameterized.named_parameters(
      ('one_client_executor', 1),
      ('two_client_executors', 2),
      ('third_client_executors', 3),
  )
  def test_multiple_accumulate_fns(self, num_client_workers):

    @tff.tensorflow.computation(np.int32, np.int32)
    def multiply(val1, val2):
      return val1 * val2

    @federated_language.federated_computation([
        federated_language.FederatedType(np.int32, federated_language.CLIENTS),
        federated_language.FederatedType(np.int32, federated_language.SERVER),
    ])
    def comp_fn(client_data, server_state):
      scaled_client_data = federated_language.federated_map(
          multiply,
          federated_language.federated_zip([
              federated_language.federated_value(
                  10, federated_language.CLIENTS
              ),
              client_data,
          ]),
      )
      scaled_server_state = federated_language.federated_map(
          multiply,
          federated_language.federated_zip([
              federated_language.federated_value(2, federated_language.SERVER),
              server_state,
          ]),
      )
      broadcasted_server_state = federated_language.federated_broadcast(
          scaled_server_state
      )
      rescaled_client_data = federated_language.federated_map(
          multiply,
          federated_language.federated_zip(
              [broadcasted_server_state, scaled_client_data]
          ),
      )
      return (
          federated_language.federated_sum(rescaled_client_data),
          scaled_server_state,
      )

    comp_fn_composed_tee_form = compiler.to_composed_tee_form(
        comp_fn, num_client_workers
    )

    self.check_valid_composed_tee_form(comp_fn_composed_tee_form)

    original_comp_param_name = 'original_comp_param'
    original_comp_param = federated_language.framework.Reference(
        original_comp_param_name, comp_fn.type_signature.parameter
    )
    client_data = federated_language.framework.Selection(
        original_comp_param, index=0
    )
    server_state = federated_language.framework.Selection(
        original_comp_param, index=1
    )

    accumulate_input_1, accumulate_input_type_1 = _create_accumulate_input(
        input_from_original_computation=[client_data],
        input_from_previous_composed_tee=[],
    )

    @federated_language.federated_computation(accumulate_input_type_1)
    def expected_accumulate_fn_1(value):
      client_data = value[
          compiler.AccumulateLambda.INDEX_OF_ORIGINAL_PARAM_DERIVED_INPUTS
      ][0]
      pre_aggregate_values = [
          federated_language.federated_map(
              multiply,
              federated_language.federated_zip([
                  federated_language.federated_value(
                      10, federated_language.CLIENTS
                  ),
                  client_data,
              ]),
          )
      ]
      partial_aggregate_values = []
      return [pre_aggregate_values, partial_aggregate_values]

    base_report_input_1, base_report_input_type_1, full_report_input_type_1 = (
        _create_report_input(
            accumulate_fn=expected_accumulate_fn_1,
            num_client_workers=num_client_workers,
            input_from_original_computation=[server_state],
            input_from_previous_composed_tee=[],
        )
    )

    @federated_language.federated_computation(full_report_input_type_1)
    def expected_report_fn_1(value):
      server_state = value[compiler.ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS][
          compiler.BaseLambda.INDEX_OF_ORIGINAL_PARAM_DERIVED_INPUTS
      ][0]
      return [
          federated_language.federated_map(
              multiply,
              federated_language.federated_zip([
                  federated_language.federated_value(
                      2, federated_language.SERVER
                  ),
                  server_state,
              ]),
          ),
          _ZERO,
      ]

    composed_tee_call_1, ref_to_composed_tee_call_1 = (
        _create_composed_tee_call_and_reference(
            name='composed_tee_call_1',
            accumulate_input=accumulate_input_1,
            accumulate_input_type=accumulate_input_type_1,
            base_report_input=base_report_input_1,
            base_report_input_type=base_report_input_type_1,
            accumulate_fn=expected_accumulate_fn_1,
            report_fn=expected_report_fn_1,
        )
    )

    accumulate_input_2, accumulate_input_type_2 = _create_accumulate_input(
        input_from_original_computation=[],
        input_from_previous_composed_tee=[
            federated_language.framework.Selection(
                ref_to_composed_tee_call_1,
                index=compiler._TEE_STAGE_OUTPUT_POSITION_MAP[
                    compiler.TeeStage.REPORT
                ],
            ),
            federated_language.framework.Selection(
                ref_to_composed_tee_call_1,
                index=compiler._TEE_STAGE_OUTPUT_POSITION_MAP[
                    compiler.TeeStage.ACCUMULATE
                ],
            ),
        ],
    )

    @federated_language.federated_computation(accumulate_input_type_2)
    def expected_accumulate_fn_2(value):
      scaled_server_state = value[
          compiler.AccumulateLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS
      ][0][0]
      federated_sum_zero = value[
          compiler.AccumulateLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS
      ][0][1]
      scaled_client_data = value[
          compiler.AccumulateLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS
      ][1][0]
      rescaled_client_data = federated_language.federated_map(
          multiply,
          federated_language.federated_zip([
              federated_language.federated_broadcast(scaled_server_state),
              scaled_client_data,
          ]),
      )
      pre_aggregate_values = []
      partial_aggregate_values = [
          federated_language.federated_aggregate(
              rescaled_client_data,
              federated_sum_zero,
              _SUM_FN,
              _IGNORE_MERGE_FN,
              _IDENTITY_REPORT_FN,
          )
      ]
      return [pre_aggregate_values, partial_aggregate_values]

    base_report_input_2, base_report_input_type_2, full_report_input_type_2 = (
        _create_report_input(
            accumulate_fn=expected_accumulate_fn_2,
            num_client_workers=num_client_workers,
            input_from_original_computation=[],
            input_from_previous_composed_tee=[
                federated_language.framework.Selection(
                    ref_to_composed_tee_call_1,
                    index=compiler._TEE_STAGE_OUTPUT_POSITION_MAP[
                        compiler.TeeStage.REPORT
                    ],
                )
            ],
        )
    )

    @federated_language.federated_computation(full_report_input_type_2)
    def expected_report_fn_2(value):
      partially_aggregated_values = value[
          compiler.ReportLambda.INDEX_OF_PARTIAL_AGGREGATE_INPUTS
      ]
      scaled_server_state = value[
          compiler.ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS
      ][compiler.BaseLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS][0][0]
      federated_sum_zero = value[
          compiler.ReportLambda.INDEX_OF_BASE_LAMBDA_INPUTS
      ][compiler.BaseLambda.INDEX_OF_PREVIOUS_COMPOSED_TEE_DERIVED_INPUTS][0][1]
      current_merge_result = federated_language.federated_value(
          federated_sum_zero, federated_language.SERVER
      )
      for i in range(num_client_workers):
        current_merge_result = federated_language.federated_map(
            _SUM_FN,
            federated_language.federated_zip(
                [current_merge_result, partially_aggregated_values[i][0]]
            ),
        )
      final_aggregate = federated_language.federated_map(
          _IDENTITY_REPORT_FN, current_merge_result
      )
      return [[final_aggregate, scaled_server_state]]

    composed_tee_call_2, ref_to_composed_tee_call_2 = (
        _create_composed_tee_call_and_reference(
            name='composed_tee_call_2',
            accumulate_input=accumulate_input_2,
            accumulate_input_type=accumulate_input_type_2,
            base_report_input=base_report_input_2,
            base_report_input_type=base_report_input_type_2,
            accumulate_fn=expected_accumulate_fn_2,
            report_fn=expected_report_fn_2,
        )
    )

    expected_comp = _create_composed_tee_form_lambda(
        original_comp_param_name,
        comp_fn.type_signature.parameter,
        [
            (composed_tee_call_1, ref_to_composed_tee_call_1),
            (composed_tee_call_2, ref_to_composed_tee_call_2),
        ],
    )

    self.check_computations_equal(
        comp_fn_composed_tee_form.to_building_block(), expected_comp
    )


if __name__ == '__main__':
  absltest.main()
