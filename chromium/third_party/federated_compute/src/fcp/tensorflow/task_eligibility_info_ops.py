# Copyright 2021 Google LLC
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
"""Ops for creating TaskEligibilityInfo results."""

import tensorflow as tf

# Ops implemented in C++
from fcp.tensorflow import gen_task_eligibility_info_ops

_task_eligibility_info_ops_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile(
        "./_task_eligibility_info_ops.so"))


def create_task_eligibility_info(version, task_names, task_weights):
  """Outputs a serialized `TaskEligibilityInfo` proto based on the given inputs.

  This op is used to generate `TaskEligibilityInfo` protos from a model at
  runtime, since TF Mobile does not support the standard TensorFlow ops for
  encoding/decoding protos.

  See the `TaskEligibilityInfo` and `TaskWeight` proto message documentation for
  more information.

  Args:
    version: an int64 value to place in the `TaskEligibilityInfo.version` field.
    task_names: a rank-1 string tensor containing the task names to assign
      weights to. Each entry in this tensor will be combined with the
      corresponding entry into the `task_weights` tensor at the same index, to
      form a `TaskWeight` message.
    task_weights: a rank-1 float tensor containing the task weight for each task
      (see `task_names`). Note: this tensor must have the same number of
        elements as `task_names`.

  Returns:
    a string tensor containing the serialized proto.
  """
  # Convert the inputs to tensors, as a convenience to callers. This ensures
  # that they can easily pass regular Python or numpy types in addition to
  # actual tensors.
  version = tf.convert_to_tensor(version, dtype=tf.int64, name="version")
  task_names = tf.convert_to_tensor(
      task_names, dtype=tf.string, name="task_names")
  task_weights = tf.convert_to_tensor(
      task_weights, dtype=tf.float32, name="task_weights")
  return gen_task_eligibility_info_ops.create_task_eligibility_info(
      version=version, task_names=task_names, task_weights=task_weights)
