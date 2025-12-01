from absl.testing import absltest
from absl.testing import parameterized

from fcp.artifact_building import data_upload_plan_builder
from fcp.protos import data_type_pb2
from fcp.protos import plan_pb2


def _example_query_spec_factory(
    collection_uri: str, output_name: str, direct_upload: bool = False
) -> plan_pb2.ExampleQuerySpec:
  """Creates an `ExampleQuerySpec` proto."""
  example_selector = plan_pb2.ExampleSelector(collection_uri=collection_uri)
  example_query = plan_pb2.ExampleQuerySpec.ExampleQuery(
      example_selector=example_selector
  )
  if direct_upload:
    example_query.direct_output_tensor_name = output_name
  else:
    output_vector = plan_pb2.ExampleQuerySpec.OutputVectorSpec(
        vector_name=output_name, data_type=data_type_pb2.DataType.INT64
    )
    example_query.output_vector_specs[output_name].CopyFrom(output_vector)

  example_query_spec = plan_pb2.ExampleQuerySpec()
  example_query_spec.example_queries.append(example_query)

  return example_query_spec


class ValidateExampleQuerySpecTest(parameterized.TestCase):

  @parameterized.named_parameters(
      dict(
          testcase_name='direct_tensor',
          direct_upload=True,
      ),
      dict(
          testcase_name='output_vectors',
          direct_upload=False,
      ),
  )
  def test_does_not_raise_if_valid(self, direct_upload):
    example_query_spec = _example_query_spec_factory(
        collection_uri='uri', output_name='output', direct_upload=direct_upload
    )
    # Should not raise.
    data_upload_plan_builder._validate_example_query_spec(example_query_spec)

  def test_raises_if_both_direct_tensor_and_output_vectors_set(self):
    example_query_spec = _example_query_spec_factory(
        collection_uri='uri', output_name='output', direct_upload=False
    )
    example_query_spec.example_queries[0].direct_output_tensor_name = 'tensor'

    with self.assertRaisesRegex(ValueError, 'must contain exactly one of'):
      data_upload_plan_builder._validate_example_query_spec(example_query_spec)

  def test_raises_if_neither_direct_tensor_and_output_vectors_set(self):
    example_selector = plan_pb2.ExampleSelector(collection_uri='uri')
    example_query = plan_pb2.ExampleQuerySpec.ExampleQuery(
        example_selector=example_selector
    )
    example_query_spec = plan_pb2.ExampleQuerySpec()
    example_query_spec.example_queries.append(example_query)

    with self.assertRaisesRegex(ValueError, 'must contain exactly one of'):
      data_upload_plan_builder._validate_example_query_spec(example_query_spec)

  @parameterized.named_parameters(
      dict(
          testcase_name='direct_tensor',
          direct_upload=True,
      ),
      dict(
          testcase_name='output_vectors',
          direct_upload=False,
      ),
  )
  def test_raises_if_duplicate_vector_names(self, direct_upload):
    example_query_spec = _example_query_spec_factory(
        collection_uri='uri', output_name='output', direct_upload=direct_upload
    )
    example_query_spec.example_queries.add().CopyFrom(
        example_query_spec.example_queries[0]
    )
    with self.assertRaisesRegex(ValueError, 'Duplicate vector names'):
      data_upload_plan_builder._validate_example_query_spec(example_query_spec)


class BuildClientPhaseWithExampleQuerySpecTest(parameterized.TestCase):

  @parameterized.named_parameters(
      dict(
          testcase_name='direct_tensor',
          direct_upload=True,
          aggregation_type=plan_pb2.AggregationConfig(
              federated_compute_checkpoint_aggregation=plan_pb2.FederatedComputeCheckpointAggregation()
          ),
      ),
      dict(
          testcase_name='output_vectors',
          direct_upload=False,
          aggregation_type=plan_pb2.AggregationConfig(
              federated_compute_checkpoint_aggregation=plan_pb2.FederatedComputeCheckpointAggregation()
          ),
      ),
  )
  def test_builds_client_phase_with_example_query_spec(
      self, direct_upload, aggregation_type
  ):
    output_name = 'output'
    collection_uri = 'uri'
    example_query_spec = _example_query_spec_factory(
        collection_uri=collection_uri,
        output_name=output_name,
        direct_upload=direct_upload,
    )
    client_phase = (
        data_upload_plan_builder._build_client_phase_with_example_query_spec(
            example_query_spec
        )
    )
    self.assertLen(client_phase.example_query_spec.example_queries, 1)
    self.assertEqual(
        client_phase.example_query_spec.example_queries[
            0
        ].example_selector.collection_uri,
        collection_uri,
    )
    self.assertEqual(
        client_phase.federated_example_query.aggregations[output_name],
        aggregation_type,
    )


class BuildPlanTest(parameterized.TestCase):

  @parameterized.named_parameters(
      dict(
          testcase_name='direct_tensor',
          direct_upload=True,
          aggregation_type=plan_pb2.AggregationConfig(
              federated_compute_checkpoint_aggregation=plan_pb2.FederatedComputeCheckpointAggregation()
          ),
      ),
      dict(
          testcase_name='output_vectors',
          direct_upload=False,
          aggregation_type=plan_pb2.AggregationConfig(
              federated_compute_checkpoint_aggregation=plan_pb2.FederatedComputeCheckpointAggregation()
          ),
      ),
  )
  def test_builds_plan_with_example_query_spec(
      self, direct_upload, aggregation_type
  ):
    output_name = 'output'
    collection_uri = 'uri'
    example_query_spec = _example_query_spec_factory(
        collection_uri=collection_uri,
        output_name=output_name,
        direct_upload=direct_upload,
    )
    plan = data_upload_plan_builder.build_plan(example_query_spec)
    self.assertLen(plan.phase, 1)
    server_phase = plan.phase[0].server_phase_v2
    self.assertEqual(server_phase.name, '')

    client_phase = plan.phase[0].client_phase
    self.assertEqual(
        client_phase.example_query_spec.example_queries[
            0
        ].example_selector.collection_uri,
        collection_uri,
    )
    self.assertEqual(
        client_phase.federated_example_query.aggregations[output_name],
        aggregation_type,
    )

    self.assertEqual(1, plan.version)


if __name__ == '__main__':
  absltest.main()
