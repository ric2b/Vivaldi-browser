# Copyright 2019 Google LLC
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

"""Provides the 'ExternalDataset' implementation of tf.Data.Dataset.

This wraps the generated op (in external_dataset_py_wrapper).
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import tensorflow as tf

from fcp.tensorflow import gen_external_dataset_py

_external_dataset_so = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile(
        "./_external_dataset_op.so"))


class ExternalDataset(tf.data.Dataset):
  """An ExternalDataset is defined by whomever is running the graph.

  To use an ExternalDataset, the graph must be fed a 'token' indicating what
  external dataset to use. It also takes a 'selector' input - an opaque string,
  to be interpreted by that external implementation.
  """

  def __init__(self, token, selector):
    token = tf.convert_to_tensor(token, dtype=tf.string, name="token")
    selector = tf.convert_to_tensor(selector, dtype=tf.string, name="selector")
    variant_tensor = gen_external_dataset_py.ExternalDataset(
        token=token, selector=selector)
    super(ExternalDataset, self).__init__(variant_tensor)

  @property
  def element_spec(self):
    return tf.TensorSpec([], tf.string)

  def _inputs(self):
    return []
