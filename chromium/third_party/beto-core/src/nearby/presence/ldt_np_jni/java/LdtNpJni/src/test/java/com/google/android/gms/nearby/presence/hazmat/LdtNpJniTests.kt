/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.android.gms.nearby.presence.hazmat

import org.junit.jupiter.api.Assertions.assertArrayEquals
import org.junit.jupiter.api.Assertions.assertEquals
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows

const val KEY_SEED = "CCDB2489E9FCAC42B39348B8941ED19A1D360E75E098C8C15E6B1CC2B620CD39"
const val HMAC_TAG = "B4C59FA599241B81758D976B5A621C05232FE1BF89AE5987CA254C3554DCE50E"
const val PLAINTEXT = "CD683FE1A1D1F846543D0A13D4AEA40040C8D67B"
const val SALT_BYTES = "0C0F"
const val EXPECTED_CIPHER_TEXT = "61E481C12F4DE24F2D4AB22D8908F80D3A3F9B40"

class LdtNpJniTests {
  @Test
  fun roundTripTest() {
    // Data taken from ldt_ffi_test_scenario()
    val keySeed = KEY_SEED.decodeHex()
    val hmacTag = HMAC_TAG.decodeHex()
    val plaintext = PLAINTEXT.decodeHex()
    val saltBytes = SALT_BYTES.decodeHex()
    val expectedCiphertext = EXPECTED_CIPHER_TEXT.decodeHex()
    val salt = Salt(saltBytes[0], saltBytes[1])

    val data = plaintext.copyOf()
    val encryptionCipher = LdtEncryptionCipher(keySeed)
    encryptionCipher.encrypt(salt, data)
    assertArrayEquals(expectedCiphertext, data)
    encryptionCipher.close()


    val decryptionCipher = LdtDecryptionCipher(keySeed, hmacTag)
    val result = decryptionCipher.decryptAndVerify(salt, data)
    assertEquals(LdtDecryptionCipher.DecryptAndVerifyResultCode.SUCCESS, result)
    assertArrayEquals(plaintext, data)
    decryptionCipher.close()
  }

  @Test
  fun createEncryptionCipherInvalidLength() {
    assertThrows<IllegalArgumentException> {
      val keySeed = ByteArray(31)
      LdtEncryptionCipher(keySeed)
    }

    assertThrows<IllegalArgumentException> {
      val keySeed = ByteArray(33)
      LdtEncryptionCipher(keySeed)
    }
  }

  @Test
  fun encryptInvalidLengthData() {
    val keySeed = KEY_SEED.decodeHex()
    val cipher = LdtEncryptionCipher(keySeed)
    assertThrows<IllegalArgumentException> {
      var data = ByteArray(15)
      cipher.encrypt(Salt(0x0, 0x0), data)
    }
    assertThrows<IllegalArgumentException> {
      var data = ByteArray(32)
      cipher.encrypt(Salt(0x0, 0x0), data)
    }
  }

  @Test
  fun encryptUseAfterClose() {
    val keySeed = KEY_SEED.decodeHex()
    val cipher = LdtEncryptionCipher(keySeed)
    val data = ByteArray(20)
    cipher.close()
    assertThrows<IllegalStateException> { cipher.encrypt(Salt(0x0, 0x0), data) }
  }

  @Test
  fun createDecryptionCipherInvalidLengths() {
    assertThrows<IllegalArgumentException> {
      val keySeed = ByteArray(31)
      val hmacTag = ByteArray(31)
      LdtDecryptionCipher(keySeed, hmacTag)
    }
    assertThrows<IllegalArgumentException> {
      val keySeed = ByteArray(33)
      val hmacTag = ByteArray(33)
      LdtDecryptionCipher(keySeed, hmacTag)
    }
    assertThrows<IllegalArgumentException> {
      val keySeed = ByteArray(32)
      val hmacTag = ByteArray(33)
      LdtDecryptionCipher(keySeed, hmacTag)
    }
    assertThrows<IllegalArgumentException> {
      val keySeed = ByteArray(33)
      val hmacTag = ByteArray(32)
      LdtDecryptionCipher(keySeed, hmacTag)
    }
  }

  @Test
  fun decryptInvalidLengthData() {
    val keySeed = KEY_SEED.decodeHex()
    val hmacTag = HMAC_TAG.decodeHex()
    val cipher = LdtDecryptionCipher(keySeed, hmacTag)
    assertThrows<IllegalArgumentException> {
      var data = ByteArray(15)
      cipher.decryptAndVerify(Salt(0x0, 0x0), data)
    }
    assertThrows<IllegalArgumentException> {
      var data = ByteArray(32)
      cipher.decryptAndVerify(Salt(0x0, 0x0), data)
    }
  }

  @Test
  fun decryptMacMismatch() {
    val keySeed = KEY_SEED.decodeHex()
    val hmacTag = HMAC_TAG.decodeHex()

    // alter first byte in the hmac tag
    hmacTag[0] = 0x00
    val cipher = LdtDecryptionCipher(keySeed, hmacTag)

    val cipherText = EXPECTED_CIPHER_TEXT.decodeHex()
    val saltBytes = SALT_BYTES.decodeHex()
    val salt = Salt(saltBytes[0], saltBytes[1])

    val result = cipher.decryptAndVerify(salt, cipherText);
    assertEquals(LdtDecryptionCipher.DecryptAndVerifyResultCode.MAC_MISMATCH, result)
  }

  @Test
  fun decryptUseAfterClose() {
    val keySeed = KEY_SEED.decodeHex()
    val hmacTag = HMAC_TAG.decodeHex()
    val cipher = LdtDecryptionCipher(keySeed, hmacTag)
    cipher.close()

    val data = ByteArray(20)
    assertThrows<IllegalStateException> { cipher.decryptAndVerify(Salt(0x0, 0x0), data) }
  }
}

private fun ByteArray.toHex(): String =
  joinToString(separator = "") { eachByte -> "%02x".format(eachByte) }

private fun String.decodeHex(): ByteArray {
  check(length % 2 == 0)
  return chunked(2)
    .map { it.toInt(16).toByte() }
    .toByteArray()
}
