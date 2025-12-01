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

"""Prints a GraphDef to stdout (for testing ExternalDataset)."""

import argparse
import numpy as np
import tensorflow.compat.v1 as tf

from  fcp.tensorflow import external_dataset


def _ParseSingleExample(p):
  # parse_example doesn't like scalars, so we reshape with [-1].
  features = tf.parse_example(
      tf.reshape(p, [-1]), {"val": tf.FixedLenFeature([], dtype=tf.int64)})
  return features["val"]


def MakeGraph():
  """Makes a GraphDef."""

  graph = tf.Graph()

  with graph.as_default():
    serialized_examples = external_dataset.ExternalDataset(
        token=tf.placeholder(name="token", dtype=tf.string),
        selector=tf.placeholder(name="selector", dtype=tf.string))

    examples = serialized_examples.map(_ParseSingleExample)

    total = examples.reduce(np.int64(0), lambda x, y: x + y)
    total = tf.identity(total, name="total")

  return graph


def _ParseArgs():
  parser = argparse.ArgumentParser()
  parser.add_argument("--output", required=True, type=argparse.FileType("w"))
  return parser.parse_args()

if __name__ == "__main__":
  args = _ParseArgs()
  with args.output:
    graph_def = MakeGraph().as_graph_def()
    args.output.write(str(graph_def))
