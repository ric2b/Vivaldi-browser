"""Methods for building a Plan proto for a data upload task."""

from fcp.protos import plan_pb2


def build_plan(example_query_spec: plan_pb2.ExampleQuerySpec) -> plan_pb2.Plan:
  """Builds the Plan proto for a data upload task.

  Args:
    example_query_spec: A `plan_pb2.ExampleQuerySpec` containing output vector
      information for the client query. This should either contain
      `output_vector_specs` or a `direct_upload_tensor_name` but not both. The
      example queries should have unique vector names.

  Returns:
    A plan proto message containing just a `ClientPhase` with the
    `ExampleQuerySpec` populated and an empty `ServerPhaseV2`.
  """
  return plan_pb2.Plan(
      version=1,
      phase=[
          plan_pb2.Plan.Phase(
              server_phase_v2=plan_pb2.ServerPhaseV2(),
              client_phase=_build_client_phase_with_example_query_spec(
                  example_query_spec
              ),
          ),
      ],
  )


def _validate_example_query_spec(
    example_query_spec: plan_pb2.ExampleQuerySpec,
) -> None:
  """Validates the ExampleQuerySpec.

  Args:
    example_query_spec: A `plan_pb2.ExampleQuerySpec` containing output vector
      information for the client query. This should either contain
      `output_vector_specs` or a `direct_upload_tensor_name` but not both. The
      example queries should have unique vector names.

  Raises:
    ValueError: If there are duplicate vector names in the
      `example_query_spec` or if the `example_query_spec` contains both
      `output_vector_specs` and `direct_upload_tensor_name`.
  """
  used_names = set()

  for example_query in example_query_spec.example_queries:
    if bool(example_query.direct_output_tensor_name) == bool(
        example_query.output_vector_specs
    ):
      raise ValueError(
          'ExampleQuerySpec must contain exactly one of `output_vector_specs`'
          f' `direct_output_tensor_name`. Found: {example_query}.'
      )

    if example_query.output_vector_specs:
      vector_names = set(example_query.output_vector_specs.keys())
      if vector_names & used_names:
        raise ValueError(
            'Duplicate vector names found in supplied `example_query_spec`. '
            f'Duplicates: {vector_names & used_names}.'
        )
      used_names.update(vector_names)
    else:
      if example_query.direct_output_tensor_name in used_names:
        raise ValueError(
            'Duplicate vector names found in supplied `example_query_spec`. '
            f'Duplicate: {example_query.direct_output_tensor_name}'
        )
      used_names.add(example_query.direct_output_tensor_name)


def _build_client_phase_with_example_query_spec(
    example_query_spec: plan_pb2.ExampleQuerySpec,
) -> plan_pb2.ClientPhase:
  """Builds the `ClientPhase` with `ExampleQuerySpec` field populated.

  Args:
    example_query_spec: A `plan_pb2.ExampleQuerySpec` containing output vector
      information for the client query. This should either contain
      `output_vector_specs` or a `direct_upload_tensor_name` but not both. The
      example queries should have unique vector names.

  Returns:
    A client phase proto message.
  """
  _validate_example_query_spec(example_query_spec)
  io_router = plan_pb2.FederatedExampleQueryIORouter()
  for example_query in example_query_spec.example_queries:
    if example_query.output_vector_specs:
      for vector_name in set(example_query.output_vector_specs.keys()):
        io_router.aggregations[vector_name].CopyFrom(
            plan_pb2.AggregationConfig(
                federated_compute_checkpoint_aggregation=plan_pb2.FederatedComputeCheckpointAggregation()
            )
        )
    else:
      io_router.aggregations[example_query.direct_output_tensor_name].CopyFrom(
          plan_pb2.AggregationConfig(
              federated_compute_checkpoint_aggregation=plan_pb2.FederatedComputeCheckpointAggregation()
          )
      )

  return plan_pb2.ClientPhase(
      example_query_spec=example_query_spec, federated_example_query=io_router
  )
