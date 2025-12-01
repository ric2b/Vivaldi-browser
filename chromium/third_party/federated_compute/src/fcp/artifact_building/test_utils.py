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
"""Utilities used in tests."""

import tensorflow as tf

from fcp.protos import plan_pb2


def set_checkpoint_op(
    checkpoint_op_proto: plan_pb2.CheckpointOp,
    saver: tf.compat.v1.train.SaverDef,
):
  """Sets the saver_def from saver onto checkpoint_op_proto and fixes a name."""
  if not saver:
    return
  saver_def_proto = checkpoint_op_proto.saver_def

  saver_def_proto.CopyFrom(saver.as_saver_def())
  # They are calling an Op a Tensor and it works in python and
  # breaks in C++.  However, for use in the python Saver class, we
  # need the tensor because we need sess.run() to return the
  # tensor's value. So, we only strip the ":0" in the case of
  # plan execution, where we use the write_checkpoint and
  # read_checkpoint methods below instead of the Saver.
  saver_def_proto.save_tensor_name = saver_def_proto.save_tensor_name.replace(
      ':0', ''
  )
  assert saver_def_proto.save_tensor_name.rfind(':') == -1
