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
"""Python and TensorFlow functions to work with dictionaries.

Please see fcp/dictionary/dictionary.h for more on this type of
dictionary.

Python Classes:

* `Dictionary`: A Python analogue to fcp/dictionary/dictionary.h
    that includes additional helpers for dictionary construction.

TensorFlow ops:

*  dictionary_size
   Queries the size of a dictionary.

*  dictionary_lookup
   Looks up ids for string tokens in the dictionary.

*  dictionary_reverse_lookup
   Looks up string tokens from ids in the dictionary.

Canonical use (note that the dictionary is known at graph construction time):
    dictionary = Dictionary.from_tokens(
       tokens=['some', 'token', 'list'], unk_id=0,
       vocabulary_type=VocabularyType.TOKEN_INDEX)

    with tf.Graph().as_default():
      tokens = tf.compat.v1.placeholder(tf.String, ...)  # Tokens to look up.
      ids = dictionary_lookup(
          tokens, dictionary.dictionary_description_proto)
"""

import collections
import enum

import tensorflow as tf

from fcp.dictionary.dictionary_pb2 import DictionaryDescription  # pylint: disable=g-importing-member
from fcp.tensorflow.gen_dictionary_ops import dictionary_lookup
from fcp.tensorflow.gen_dictionary_ops import dictionary_reverse_lookup
from fcp.tensorflow.gen_dictionary_ops import dictionary_size

_dictionary_ops = tf.load_op_library(
    tf.compat.v1.resource_loader.get_path_to_datafile('./_dictionary_ops.so'))


def ignore_ids_mask(token_ids, ignore_ids, name=None):
  """Creates a bool mask with True everywhere token_ids is not in ignore_ids."""
  with tf.op_scope([token_ids, ignore_ids], name, 'ignore_ids_mask'):
    # Yay broadcasting
    all_check = tf.not_equal(tf.expand_dims(token_ids, -1), ignore_ids)
    check = tf.reduce_all(all_check, reduction_indices=tf.rank(all_check) - 1)
    check.set_shape(token_ids.get_shape())
    return check


def mask_and_replace_padding(token_ids,
                             lengths,
                             eos_id=None,
                             special_tokens=(),
                             name=None):
  """Creates a mask of valid tokens and sets padded values in id space.

  This creates a mask the same shape as token_ids with a boolean indicating
  if the id was a valid token (i.e not padding or a special token). If
  provided, this also remaps tokens after lengths to the eos_id. Since the
  dictionary doesn't map tokens to eos or bos ids, it would generally be the
  unknown token id which is not correct if you need to predict the eos.

  Args:
    token_ids: A matrix `Tensor` of integer ids.
    lengths: A vector `Tensor` of lengths for each row in token_ids.
    eos_id: The end of sequence id, if provided then all token ids after length
      in a row will be replaced with `eos_id`.
    special_tokens: An iterable of special tokens for ids that are not
      considered valid.
    name: Name scope for these ops.

  Returns:
    token_ids: `token_ids` with all tokens after a row's length replaced with
      eos if provided.
    mask: A bool `Tensor` the same shape as `token_ids` indicating which tokens
      are valid.
  """
  with tf.op_scope([token_ids, lengths, eos_id, special_tokens], name,
                   'mask_and_replace_padding'):
    ranges = tf.range(0, tf.gather(tf.shape(token_ids), 1))

    # Yay! Broadcasting.
    selected = tf.less(ranges, tf.expand_dims(lengths, -1))

    if eos_id is not None:
      token_ids = tf.where(
          selected, token_ids,
          tf.fill(
              tf.shape(token_ids), tf.constant(eos_id, dtype=token_ids.dtype)))
    if special_tokens:
      mask = tf.logical_and(
          ignore_ids_mask(token_ids, special_tokens), selected)
    else:
      mask = selected
    return token_ids, mask

tf.no_gradient('DictionarySize')
tf.no_gradient('DictionaryLookup')
tf.no_gradient('DictionaryReverseLookup')


class VocabularyType(enum.Enum):
  """Valid vocabulary types for Dictionary construction.

  TOKEN_INDEX: dictionary.dictionary_description contains an embedded map of
    string names stored in order with ids assigned starting from the lowest
    non-special id. Preserves order but is not compact.
  """
  TOKEN_INDEX = 3


class Dictionary(object):
  """Utility for working with fcp/dictionary/ via TensorFlow."""

  def __init__(
      self,
      dictionary_description
  ):
    """Creates a dictionary from a dictionary_description.

    Use static from_* constructor methods for building dictionaries from
    common data types.

    Args:
      dictionary_description: A `dictionary_pb2.DictionaryDescription`
        describing the dictionary.

    Raises:
      ValueError: An invalid dictionary description.
    """
    if not isinstance(dictionary_description, DictionaryDescription):
      raise ValueError('Expected a DictionaryDescription')
    if not dictionary_description.HasField('vocabulary'):
      raise ValueError('dictionary_description has no vocabulary')

    self._dictionary_description = dictionary_description

    # Lazily constructed fields for lookup.
    self._lookup_graph = None
    self._lookup_placeholder = None
    self._lookup_result = None
    self._reverse_lookup_placeholder = None
    self._reverse_lookup_result = None

  @classmethod
  def from_tokens(
      cls,
      tokens,
      bos_id=None,
      eos_id=None,
      unk_id=None,
      output_blocklist_tokens=None,
      output_size=None,
      vocabulary_type=VocabularyType.TOKEN_INDEX
  ):
    """Creates a dictionary from a provided list of tokens.

    The id mappings to token ids depend on the vocabulary_type requested.

    NB: the special tokens must be the first ids [0, num-specials)

    Args:
      tokens: An unordered iterable of tokens for the dictionary.
      bos_id: Token id for start of sequence.
      eos_id: Token id for end of sequence.
      unk_id: Token id for unknown words.
      output_blocklist_tokens: A list of vocabulary tokens that should be
        filtered from predictions (e.g., punctuation, bad words etc.).
      output_size: If a positive integer, tokens with ids greater than this are
        automatically added to the output blocklist.
      vocabulary_type: `VocabularyType` to use, defaults to TOKEN_INDEX.

    Returns:
       A `Dictionary` instance.

    Raises:
      ValueError: If the special tokens don't have the lowest ids.
      ValueError: If there are duplicates in tokens.
    """
    dictionary_description = DictionaryDescription()

    # Special ids.
    special_ids = []
    if unk_id is not None:
      dictionary_description.special_ids.unk = unk_id
      special_ids.append(unk_id)
    if bos_id is not None:
      dictionary_description.special_ids.bos = bos_id
      special_ids.append(bos_id)
    if eos_id is not None:
      dictionary_description.special_ids.eos = eos_id
      special_ids.append(eos_id)
    if sorted(special_ids) != list(range(len(special_ids))):
      raise ValueError(
          'Special ids must be the first items of the dictionary starting at 0'
          'or None. eos: %s; bos %s; unk: %s' % (eos_id, bos_id, unk_id))

    # Vocabulary.
    if len(tokens) != len(set(tokens)):
      raise ValueError('Duplicate tokens provided')
    for token in tokens:
      if not isinstance(token, (str, bytes)):
        raise ValueError('Bad type in tokens %s' % token)
    if vocabulary_type == VocabularyType.TOKEN_INDEX:
      for token in tokens:
        dictionary_description.vocabulary.index.token.append(token)
    else:
      raise AssertionError('Unsupported vocabulary_type: %s' % vocabulary_type)

    # Output blocklist.
    output_blocklist_tokens = list(output_blocklist_tokens or [])
    if output_size:
      assert output_size >= len(special_ids), (
          'Cannot blocklist special tokens via output_size.')
      assert isinstance(tokens, list)  # Make sure order preserving pre-slice.
      output_blocklist_tokens.extend(tokens[output_size - len(special_ids):])
    for token in output_blocklist_tokens:
      assert token in tokens, "Unexpected blocklist token: '%s'" % token
    with tf.compat.v1.Session(graph=tf.Graph()) as sess:
      output_blocklist_ids = sess.run(
          dictionary_lookup(output_blocklist_tokens,
                            dictionary_description.SerializeToString()))
    dictionary_description.output_blocklist_ids.id.extend(
        sorted(output_blocklist_ids))
    assert (len(set(dictionary_description.output_blocklist_ids.id)) == len(
        output_blocklist_tokens)), 'blocklist contains dups or unks?'

    # Return completed dictionary.
    return cls(
        dictionary_description=dictionary_description)

  @classmethod
  def from_dictionary_description(cls,
                                  dictionary_description):
    """Returns a Dictionary from a DictionaryDescription."""
    return cls(
        dictionary_description=dictionary_description)

  def _get_lookup_graph(self):
    """Returns a graph to use for lookup, reverse lookup, and size queries."""
    if self._lookup_graph is None:
      self._lookup_graph = tf.Graph()
      serialized_description_proto = (
          self._dictionary_description.SerializeToString())
      with self._lookup_graph.as_default():
        self._lookup_placeholder = tf.compat.v1.placeholder(
            tf.string, shape=None)
        self._reverse_lookup_placeholder = tf.compat.v1.placeholder(
            tf.int64, shape=None)

        # Use Dictionary(Op) (without blob) variants.
        self._lookup_result = dictionary_lookup(
            self._lookup_placeholder,
            dictionary_description_proto=serialized_description_proto)
        self._reverse_lookup_result = dictionary_reverse_lookup(
            self._reverse_lookup_placeholder,
            dictionary_description_proto=serialized_description_proto)
        self._size_result = dictionary_size(
            dictionary_description_proto=serialized_description_proto)

    return self._lookup_graph

  def lookup(self, tokens):
    """Maps a list of tokens to a list of ids.

    Args:
      tokens: A list of tokens to lookup.

    Returns:
      A list of token ids of the same size.

    Raises:
      ValueError: If tokens is not a list.
    """
    if not isinstance(tokens, list):
      raise ValueError('lookup expected a list of tokens.')

    with tf.compat.v1.Session(graph=self._get_lookup_graph()) as sess:
      return sess.run(self._lookup_result, {
          self._lookup_placeholder: tokens
      }).tolist()

  def reverse_lookup(self, ids):
    """Maps a list of ids to tokens.

    Args:
      ids: A list of ids to map back to tokens.

    Returns:
      A list of tokens corresponding to those ids.

    Raises:
      ValueError: If ids is not a list.
    """
    if not isinstance(ids, list):
      raise ValueError('reverse_lookup expected a list of ids.')
    with tf.compat.v1.Session(graph=self._get_lookup_graph()) as sess:
      return list(
          sess.run(self._reverse_lookup_result,
                   {self._reverse_lookup_placeholder: ids}))

  @property
  def special_ids(self):
    """Returns a list of special token ids."""
    return [t for t in [self.unk_id, self.bos_id, self.eos_id] if t is not None]

  @property
  def eos_id(self):
    eos_id = self._dictionary_description.special_ids.eos
    return eos_id if eos_id >= 0 else None

  @property
  def bos_id(self):
    bos_id = self._dictionary_description.special_ids.bos
    return bos_id if bos_id >= 0 else None

  @property
  def unk_id(self):
    unk_id = self._dictionary_description.special_ids.unk
    return unk_id if unk_id >= 0 else None

  @property
  def size(self):
    with tf.compat.v1.Session(graph=self._get_lookup_graph()) as sess:
      return sess.run(self._size_result)

  @property
  def output_blocklist_ids(self):
    return list(self._dictionary_description.output_blocklist_ids.id)

  @property
  def output_blocklist_tokens(self):
    return self.reverse_lookup(self.output_blocklist_ids)

  @property
  def tokens(self):
    return self.reverse_lookup(list(range(len(self.special_ids), self.size)))

  @property
  def dictionary_description_proto(self):
    """Serialized proto containing self.dictionary_description."""
    return self.dictionary_description.SerializeToString()

  @property
  def dictionary_description(self):
    """Returns the `DictionaryDescription` proto describing this dictionary.
    """
    desc = self._dictionary_description
    return desc

  def __len__(self):
    return self.size
