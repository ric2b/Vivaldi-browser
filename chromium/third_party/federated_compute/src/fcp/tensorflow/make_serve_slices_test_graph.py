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
"""Writes a GraphDef to a file for testing `ServeSlices`."""

from absl import app
from absl import flags
import tensorflow as tf

from fcp.tensorflow import serve_slices

CALLBACK_TOKEN_PLACEHOLDER_TENSOR = 'callback_token'
SERVED_AT_TENSOR = 'served_at_id'
SERVER_VAL = (1, 2.0, 'foo')
MAX_KEY = 44
SELECT_FN_INITIALIZE_OP = 'init_the_things'
SELECT_FN_SERVER_VAL_INPUT_TENSOR_NAMES = ['a', 'b', 'c']
SELECT_FN_KEY_INPUT_TENSOR_NAME = 'bar'
SELECT_FN_FILENAME_TENSOR_NAME = 'goofy'
SELECT_FN_TARGET_TENSOR_NAME = 'goobler'

flags.DEFINE_string('output', None, 'The path to the output file.')
FLAGS = flags.FLAGS


def make_graph():
  """Builds and returns a `tf.Graph` which calls `ServeSlices`."""
  graph = tf.Graph()
  with graph.as_default():
    # Create a placeholder with a fixed name to allow the code running the graph
    # to provide input.
    callback_token = tf.compat.v1.placeholder(
        name=CALLBACK_TOKEN_PLACEHOLDER_TENSOR, dtype=tf.string)
    served_at_id = serve_slices.serve_slices(
        callback_token=callback_token,
        server_val=SERVER_VAL,
        max_key=MAX_KEY,
        select_fn_initialize_op=SELECT_FN_INITIALIZE_OP,
        select_fn_server_val_input_tensor_names=SELECT_FN_SERVER_VAL_INPUT_TENSOR_NAMES,
        select_fn_key_input_tensor_name=SELECT_FN_KEY_INPUT_TENSOR_NAME,
        select_fn_filename_input_tensor_name=SELECT_FN_FILENAME_TENSOR_NAME,
        select_fn_target_tensor_name=SELECT_FN_TARGET_TENSOR_NAME)
    # Create a tensor with a fixed name to allow the code running the graph to
    # receive output.
    tf.identity(served_at_id, name=SERVED_AT_TENSOR)
  return graph


def main(argv):
  del argv
  graph_def_str = str(make_graph().as_graph_def())
  with open(FLAGS.output, 'w') as output_file:
    output_file.write(graph_def_str)


if __name__ == '__main__':
  flags.mark_flag_as_required('output')
  app.run(main)
