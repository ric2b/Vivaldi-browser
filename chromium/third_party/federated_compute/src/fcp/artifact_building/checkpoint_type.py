# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A module holding enum determining checkpoint format type.

This FCP module should be kept lightweight as it is used for enum flag
definition purposes.
"""

import enum


@enum.unique
class CheckpointFormatType(enum.Enum):
  """Option adjusting checkpoint format between client and server.

  Values:
    TF1_SAVE_SLICES: The default value. Uses a standard TFv1 format.
    APPEND_SLICES_MERGE_WRITE: Experimental value allowing to stream data to
      checkpoint. The conversion from stream of checkpoint slices to TFv1 format
      happens right after all the chunks are written. So the transport format is
      identical to the TF1_SAVE_SLICES option.
    APPEND_SLICES_MERGE_READ: Experimental value allowing to stream data to
      checkpoint. The conversion from stream of checkpoint slices to TFv1 format
      happens before the data is read. So the transport format is the
      appended slices format. This setting has the smallest write memory
      overhead.
  """

  TF1_SAVE_SLICES = 'tf1_save_slices'
  APPEND_SLICES_MERGE_WRITE = 'append_slices_merge_write'
  APPEND_SLICES_MERGE_READ = 'append_slices_merge_read'
