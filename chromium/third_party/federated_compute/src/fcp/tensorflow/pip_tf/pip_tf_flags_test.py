# Copyright 2024 Google LLC
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
"""Verifies that build flags for custom TF ops are correct."""

import re

from absl import flags
from absl.testing import absltest
import tensorflow as tf

_COPTS = flags.DEFINE_list('copts', [], 'TF copts')
_CXXOPTS = flags.DEFINE_list('cxxopts', [], 'TF cxxopts')
_LINKOPTS = flags.DEFINE_list('linkopts', [], 'TF linkopts')

_ERROR_MSG = """
If the TensorFlow version has been updated, copy the new value to
fcp/tensorflow/pip_tf/defs.bzl.
"""


class PipTfFlagsTest(absltest.TestCase):

  def test_compile_flags(self):
    copts = []
    cxxopts = []
    for flag in tf.sysconfig.get_compile_flags():
      # Ignore include flags, which are handled by bazel.
      if flag.startswith('-I'):
        continue

      if flag.startswith('--std=c++'):  # Don't add C++-only flags to copts.
        cxxopts.append(flag)
      else:
        copts.append(flag)

    self.assertSameElements(copts, _COPTS.value, _ERROR_MSG)
    self.assertSameElements(cxxopts, _CXXOPTS.value, _ERROR_MSG)

  def test_link_flags(self):
    linkopts = []
    for flag in tf.sysconfig.get_link_flags():
      # Ignore library search paths, which are handled by bazel.
      if flag.startswith('-L'):
        continue
      # Ignore -ltensorflow_framework, which is handled by bazel.
      if re.search(r'^-l(:lib)?tensorflow_framework', flag):
        continue
      linkopts.append(flag)

    self.assertSameElements(linkopts, _LINKOPTS.value, _ERROR_MSG)


if __name__ == '__main__':
  absltest.main()
