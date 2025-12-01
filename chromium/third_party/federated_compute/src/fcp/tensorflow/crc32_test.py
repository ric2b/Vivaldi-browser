# Copyright 2020 Google LLC
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

import numpy as np
import tensorflow as tf
from fcp.tensorflow import crc32


class CRC32Test(tf.test.TestCase):

  def test_crc32(self):
    assert 0 == crc32.crc32([])

    # Constants taken from rfc3720 B.4
    # 1. Tests on byte arrays
    assert 0x8a9136aa == crc32.crc32(np.zeros(32, dtype=np.uint8))
    assert 0x62a8ab43 == crc32.crc32(255 * np.ones(32, dtype=np.uint8))
    x = np.arange(0, 0x20, dtype=np.uint8)
    assert 0x46dd794e == crc32.crc32(x)
    # 2. Tests for higher dimensional tensor shapes
    assert 0x46dd794e == crc32.crc32(x.reshape(2, -1))
    assert 0x46dd794e == crc32.crc32(x.reshape(4, 4, 2))
    # Transpose will change memory order so checksum should change.
    assert 0x46dd794e != crc32.crc32(x.reshape(2, -1).transpose())

    # 3. Tests on int32, int64
    assert crc32.crc32(tf.constant(0x123456789abcdef0,
                                   dtype=tf.int64)) == crc32.crc32(
                                       tf.constant([0x9abcdef0, 0x12345678],
                                                   dtype=tf.int32))

    # 4. IEEE float test. Not much to test here other than that the checksum
    # produces the same result as it would on an integer tensor with the same
    # memory representation.
    assert crc32.crc32(tf.constant([-0.0, 0.0],
                                   dtype=tf.float32)) == crc32.crc32(
                                       tf.constant([0x80000000, 0x00000000],
                                                   dtype=tf.int32))


if __name__ == "__main__":
  tf.test.main()
