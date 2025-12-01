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

from absl.testing import parameterized
import tensorflow as tf
from google.protobuf import text_format
from fcp.dictionary import dictionary_pb2
from fcp.tensorflow import dictionary_ops


class DictionaryOpsTest(tf.test.TestCase, parameterized.TestCase):

  def test_direct_tf_use_literal_dictionary(self):
    dictionary = dictionary_pb2.DictionaryDescription()
    text_format.Merge(
        'special_ids: < unk: 0 > '
        'vocabulary: < '
        '  index: < token: "a" token: "b" token: "c" token: "d" >'
        '>',
        dictionary)

    lookup = dictionary_ops.dictionary_lookup(
        tf.constant(['a', 'b', 'a', 'a', 'd', 'X']),
        dictionary_description_proto=dictionary.SerializeToString())
    with tf.compat.v1.Session() as sess:
      tokenized = sess.run(lookup)
    self.assertEqual([1, 2, 1, 1, 4, 0], tokenized.tolist())

  @parameterized.named_parameters(
      ('token_index', dictionary_ops.VocabularyType.TOKEN_INDEX))
  def test_build_dictionary_with_output_blocklist(self, vocabulary_type):
    # Build a dictionary, explicitly blocklisting the first token and
    # implicitly blocklisting the last token via output_size.
    dictionary = dictionary_ops.Dictionary.from_tokens(
        ['01', '02', '10', '11'],
        unk_id=0,
        output_blocklist_tokens=['01'],
        output_size=4,
        vocabulary_type=vocabulary_type)

    if vocabulary_type in (
        dictionary_ops.VocabularyType.TOKEN_INDEX,
    ):
      result = dictionary_ops.dictionary_lookup(
          [['01', '02', '10', '11', '12']],
          dictionary_description_proto=dictionary.dictionary_description_proto)

    with tf.compat.v1.Session() as sess:
      tokenized = sess.run(result)
    self.assertEqual([[1, 2, 3, 4, 0]], tokenized.tolist())
    self.assertEqual(
        [1, 4], list(dictionary.dictionary_description.output_blocklist_ids.id))

  @parameterized.named_parameters(
      ('token_index', dictionary_ops.VocabularyType.TOKEN_INDEX))
  def test_build_dictionary(self, vocabulary_type):
    dictionary = dictionary_ops.Dictionary.from_tokens(
        ['A', 'a', 'B', 'c'],
        unk_id=0,
        vocabulary_type=vocabulary_type)

    result = dictionary_ops.dictionary_lookup(
        [['A', 'a', 'B', 'b', 'C', 'c', 'D', 'd']],
        dictionary_description_proto=dictionary.dictionary_description_proto)
    expected = [[1, 2, 3, 0, 0, 4, 0, 0]]
    with tf.compat.v1.Session() as sess:
      tokenized = sess.run(result)
    self.assertEqual(expected, tokenized.tolist())

  @parameterized.named_parameters(
      ('token_index', dictionary_ops.VocabularyType.TOKEN_INDEX))
  def test_dictionary_should_raise_with_duplicate_tokens(self, vocabulary_type):
    with self.assertRaisesRegex(ValueError, 'Duplicate tokens'):
      dictionary_ops.Dictionary.from_tokens(['01', '02', '11', '10', '11'],
                                            vocabulary_type=vocabulary_type)

  @parameterized.named_parameters(
      ('token_index', dictionary_ops.VocabularyType.TOKEN_INDEX))
  def test_lookup_in_python(self, vocabulary_type):
    dictionary = dictionary_ops.Dictionary.from_tokens(
        ['01', '02', '10', '11'], unk_id=0, vocabulary_type=vocabulary_type)
    self.assertLen(dictionary, 5)
    self.assertListEqual([1, 2, 3, 4, 0],
                         dictionary.lookup(['01', '02', '10', '11', '12']))

  @parameterized.named_parameters(
      ('token_index', dictionary_ops.VocabularyType.TOKEN_INDEX))
  def test_reverse_lookup_in_python(self, vocabulary_type):
    dictionary = dictionary_ops.Dictionary.from_tokens(
        ['01', '02', '10', '11'], unk_id=0, vocabulary_type=vocabulary_type)
    self.assertLen(dictionary, 5)
    rlookup = [
        t.decode('utf-8') for t in dictionary.reverse_lookup([3, 2, 1, 4, 0])
    ]
    self.assertListEqual(['10', '02', '01', '11', ''], rlookup)

  def test_literal_dictionary_in_python(self):
    dictionary_description = dictionary_pb2.DictionaryDescription()
    text_format.Merge(
        'special_ids: < unk: 0 > '
        'vocabulary: < '
        '  index: < token: "a" token: "b" token: "c" token: "d" >'
        '>',
        dictionary_description)
    dictionary = dictionary_ops.Dictionary.from_dictionary_description(
        dictionary_description)
    self.assertListEqual([b'a', b'b', b'c', b'd'], dictionary.tokens)


if __name__ == '__main__':
  # Required since the test still relies on v1 Session.run behavior.
  tf.compat.v1.disable_v2_behavior()
  tf.test.main()
