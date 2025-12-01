"""An implementation of the DP MF aggregator using the JaxPrivacy library."""

import functools

import federated_language as flang
import jax
from jax.experimental import jax2tf
import jax.numpy as jnp
from jax_privacy.experimental import clipping
import optax
import tensorflow as tf
import tensorflow_federated as tff


def _shapes_dtypes_from_typespec(typespec: flang.Type):
  """Converts a spec to a tree of shapes and dtypes."""
  return tff.tensorflow.structure_from_tensor_type_tree(
      lambda x: jax.ShapeDtypeStruct(shape=x.shape, dtype=x.dtype),
      typespec,
  )


_jax2tf_convert_cpu_native = functools.partial(
    jax2tf.convert,
    native_serialization=True,
    native_serialization_platforms=['cpu'],
)


class DPMFAggregatorFactory(tff.aggregators.UnweightedAggregationFactory):
  """Aggregation factory for Differentially Private mean across clients.

  This factory expects a privatizer from the `jax_privacy` to be created,
  supporting methods such as those from [Scaling up the Banded Matrix
  Factorization Mechanism for Differentially Private
  ML](https://arxiv.org/abs/2405.15913).
  """

  def __init__(
      self,
      *,
      gradient_privatizer: optax.GradientTransformation,
      clients_per_round: int,
      l2_clip_norm: float = 1.0,
  ):
    """Initializes `DPMFAggregatorFactory`."""
    if clients_per_round <= 0.0:
      raise ValueError('clients_per_round must be a positive float.')
    if l2_clip_norm <= 0.0:
      raise ValueError('l2_clip_norm must be a positive float.')

    self._grad_privatizer = gradient_privatizer
    self._l2_clip_norm = l2_clip_norm
    self._clients_per_round = clients_per_round

  def create(
      self, value_type: flang.TensorType | flang.StructType
  ) -> tff.templates.AggregationProcess:
    """Creates a `tff.templates.AggregationProcess` for DP MF mean."""

    @flang.federated_computation()
    def init_fn():

      @tff.tensorflow.computation
      def init_privatizer():

        def jax_init_privatizer():
          return self._grad_privatizer.init(
              _shapes_dtypes_from_typespec(value_type)
          )

        return _jax2tf_convert_cpu_native(jax_init_privatizer)()

      return flang.federated_eval(init_privatizer, flang.SERVER)

    @flang.federated_computation(
        init_fn.type_signature.result,
        flang.FederatedType(value_type, flang.CLIENTS),
    )
    def next_fn(noise_state, value):

      @tff.tensorflow.computation
      def build_zero():

        def jax_zero():
          accumulator_zero = tff.tensorflow.structure_from_tensor_type_tree(
              lambda arr: jnp.zeros(shape=arr.shape, dtype=arr.dtype),
              value_type,
          )
          metrics_zero = {'num_clipped_updates': jnp.zeros([], dtype=jnp.int32)}
          return (accumulator_zero, metrics_zero)

        return _jax2tf_convert_cpu_native(jax_zero)()

      @tff.tensorflow.computation(build_zero.type_signature.result, value_type)
      def clipped_sum(state, client_updates):
        """Clips the updates by the global l2 norm and adds them to the sum."""

        def jax_clipped_sum(state, client_updates):
          clipped_updates, global_l2_norm = clipping.clip_pytree(
              client_updates,
              clip_norm=self._l2_clip_norm,
              rescale_to_unit_norm=False,
              nan_safe=True,
          )
          was_clipped = jnp.int32(global_l2_norm > self._l2_clip_norm)
          new_metrics = {'num_clipped_updates': was_clipped}
          accumulator_pytree, metrics_pytree = state
          new_accumulators = jax.tree.map(
              jnp.add, accumulator_pytree, clipped_updates
          )
          new_metrics = jax.tree.map(jnp.add, new_metrics, metrics_pytree)
          return new_accumulators, new_metrics

        return _jax2tf_convert_cpu_native(jax_clipped_sum)(
            state, client_updates
        )

      @tff.tensorflow.computation
      def add(a, b):
        """Merge two partial, clipped sums."""
        return tf.nest.map_structure(tf.add, a, b)

      @tff.tensorflow.computation
      def identity(a):
        return a

      unnoised_sum, metrics = flang.federated_aggregate(
          value,
          build_zero(),
          accumulate=clipped_sum,
          merge=add,
          report=identity,
      )

      @tff.tensorflow.computation
      def finalize_noise(noise_state, unnoised_aggregate):
        """Compute the noised mean.

        Adds noise to the unnoised sum and divides by number of clients that
        participated in the round.

        Args:
          noise_state: The state used to create a noise slice to add to the
            unnoised sum this round.
          unnoised_aggregate: The unnoised sum to add noise to.

        Returns:
          The noised mean.
        """

        def jax_finalize_noise(noise_state, unnoised_aggregate):
          noised_aggregate, new_noise_state = self._grad_privatizer.update(
              unnoised_aggregate, noise_state
          )
          noised_mean = jax.tree.map(
              lambda arr: arr / self._clients_per_round,
              noised_aggregate,
          )
          return new_noise_state, noised_mean

        return _jax2tf_convert_cpu_native(jax_finalize_noise)(
            noise_state, unnoised_aggregate
        )

      new_state, noised_aggregate = flang.federated_map(
          finalize_noise, (noise_state, unnoised_sum)
      )
      return tff.templates.MeasuredProcessOutput(
          new_state, noised_aggregate, metrics
      )

    return tff.templates.AggregationProcess(init_fn, next_fn)
