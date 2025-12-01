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
"""Provides the `make_slices_selector_example_selector` operation.

This wraps the generated op and ensures that necessary shared libraries
are loaded.
"""

import tensorflow as tf

from fcp.tensorflow import gen_make_slices_selector_example_selector_py

_make_slices_selector_example_selector_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile(
        './_make_slices_selector_example_selector_op.so'))


def make_slices_selector_example_selector(served_at_id, keys):
  """Serializes a proto `ExampleSelector` containing a `SlicesSelector`."""
  return gen_make_slices_selector_example_selector_py.make_slices_selector_example_selector(
      served_at_id=tf.convert_to_tensor(served_at_id, tf.string),
      keys=tf.convert_to_tensor(keys, tf.int32),
  )
