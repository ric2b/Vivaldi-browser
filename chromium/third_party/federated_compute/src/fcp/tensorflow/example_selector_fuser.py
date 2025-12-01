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
"""Provides the `example_selector_fuser` operation.

This wraps the generated op and ensures that necessary shared libraries
are loaded.
"""

from typing import Optional

import tensorflow as tf

from fcp.tensorflow import gen_example_selector_fuser_op

_example_selector_fuser_op_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile(
        './_example_selector_fuser_op.so'))


def example_selector_fuser(example_selector: tf.Tensor,
                           resumption_token_type_url: tf.Tensor,
                           resumption_token_content: tf.Tensor,
                           name: Optional[str] = None) -> tf.Operation:
  """Fills the resumption token of an existing ExampleSelector message.

  Args:
    example_selector: The serialized ExampleSelector message.
    resumption_token_type_url: The type URL of the resumption token.
    resumption_token_content: The serialized content of the resumption token.
    name: A name for the operation (optional).

  Returns:
    The created `Operation`.
  """
  return gen_example_selector_fuser_op.example_selector_fuser(
      example_selector,
      resumption_token_type_url,
      resumption_token_content,
      name=name)
