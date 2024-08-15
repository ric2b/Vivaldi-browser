/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.android.gms.nearby.presence.hazmat

import androidx.annotation.IntDef
import com.google.android.gms.nearby.presence.hazmat.LdtNpJni.DecryptErrorCode
import com.google.android.gms.nearby.presence.hazmat.LdtNpJni.EncryptErrorCode

private const val KEY_SEED_SIZE = 32
private const val TAG_SIZE = 32
private const val BLOCK_SIZE = 16

// Error return value for create operations
private const val CREATE_HANDLE_ERROR = 0L

// Status code returned on successful cipher operations
private const val SUCCESS = 0

/**
 * A 2 byte salt that will be used in the advertisement with this encrypted payload.
 */
class Salt(b1: Byte, b2: Byte) {
  private val saltBytes: Char

  // Returns the bytes of the salt represented as a 2 byte Char type
  internal fun getBytesAsChar(): Char {
    return saltBytes
  }

  init {
    // byte widening conversion to int sign-extends
    val highBits = b1.toInt() shl 8
    val lowBits = b2.toInt() and 0xFF
    // narrowing conversion truncates to low 16 bits
    saltBytes = (highBits or lowBits).toChar()
  }
}

/** A runtime exception thrown if something fails during a Ldt jni call*/
class LdtJniException internal constructor(message: String?) : RuntimeException(message)

/**
 * A checked exception which occurs if the calculated hmac tag does not match the expected hmac
 * tag during decrypt operations
 */
class MacMismatchException internal constructor(message: String?) : Exception(message)

/**
 * Create a new LdtEncryptionCipher instance using LDT-XTS-AES128 with the "swap" mix function.
 *
 * @constructor Creates a new instance from the provided keySeed
 * @param keySeed is the key material from the Nearby Presence credential from which
 *     the LDT key will be derived.
 * @return an instance configured with the supplied key seed
 * @throws IllegalArgumentException if the keySeed is the wrong size
 * @throws LdtJniException if creating the instance fails
 */
class LdtEncryptionCipher @Throws(
  LdtJniException::class,
  IllegalArgumentException::class
) constructor(keySeed: ByteArray) :
  AutoCloseable {
  @Volatile
  private var closed = false
  private val handle: Long

  init {
    require(keySeed.size == KEY_SEED_SIZE)
    handle = LdtNpJni.createEncryptionCipher(keySeed)
    if (handle == CREATE_HANDLE_ERROR) {
      throw LdtJniException("Creating ldt encryption cipher native resources failed")
    }
  }

  /**
   * Encrypt a 16-31 byte buffer in-place.
   *
   * @param salt the salt that will be used in the advertisement with this encrypted payload.
   * @param data plaintext to encrypt in place: the metadata key followed by the data elements to be
   * encrypted. The length must be in [16, 31).
   * @throws IllegalStateException if this instance has already been closed
   * @throws IllegalArgumentException if data is the wrong length
   * @throws LdtJniException if encryption fails
   */
  @Throws(LdtJniException::class, IllegalArgumentException::class, IllegalStateException::class)
  fun encrypt(salt: Salt, data: ByteArray) {
    check(!closed) { "Use after free! This instance has already been closed" }
    requireValidSizeData(data.size)

    when (val res = LdtNpJni.encrypt(handle, salt.getBytesAsChar(), data)) {
      EncryptErrorCode.JNI_OP_ERROR -> throw LdtJniException("Error during jni encrypt operation: error code $res");
      EncryptErrorCode.DATA_LEN_ERROR -> check(false) // this will never happen, lengths checked above
      else -> check(res == SUCCESS)
    }
  }

  /**
   * Releases native resources.
   *
   * <p>Once closed, a Ldt instance cannot be used further.
   */
  override fun close() {
    if (!closed) {
      closed = true
      LdtNpJni.closeEncryptCipher(handle)
    }
  }
}

/**
 * A LdtDecryptionCipher instance which uses LDT-XTS-AES128 with the "swap" mix function.
 *
 * @constructor Creates a new instance from the provided keySeed and hmacTag
 * @param keySeed is the key material from the Nearby Presence credential from which
 *     the LDT key will be derived.
 * @param hmacTag is the hmac auth tag calculated on the metadata key used to verify
 *     decryption was successful.
 * @return an instance configured with the supplied key seed
 * @throws IllegalArgumentException if the keySeed is the wrong size
 * @throws IllegalArgumentException if the hmacTag is the wrong size
 * @throws LdtJniException if creating the instance fails
 */
class LdtDecryptionCipher @Throws(
  LdtJniException::class,
  IllegalArgumentException::class
) constructor(keySeed: ByteArray, hmacTag: ByteArray) : AutoCloseable {
  @Volatile
  private var closed = false
  private val handle: Long

  init {
    require(keySeed.size == KEY_SEED_SIZE)
    require(hmacTag.size == TAG_SIZE)
    handle = LdtNpJni.createDecryptionCipher(keySeed, hmacTag)
    if (handle == CREATE_HANDLE_ERROR) {
      throw LdtJniException("Creating ldt decryption cipher native resources failed")
    }
  }

  /** Error codes which map to return values on the native side.  */
  @IntDef(DecryptAndVerifyResultCode.SUCCESS, DecryptAndVerifyResultCode.MAC_MISMATCH)
  annotation class DecryptAndVerifyResultCode {
    companion object {
      const val SUCCESS = 0
      const val MAC_MISMATCH = -1
    }
  }

  /**
   *  Decrypt a 16-31 byte buffer in-place and verify the plaintext metadata key matches
   *  this item's MAC, if not the buffer will not be decrypted.
   *
   * @param salt the salt extracted from the advertisement that contained this payload.
   * @param data ciphertext to decrypt in place: the metadata key followed by the data elements to
   * be decrypted. The length must be in [16, 31).
   * @return a [DecryptAndVerifyResultCode] indicating of the decrypt operation failed or succeeded.
   * In the case of a failed decrypt, the provided plaintext will not change.
   * @throws IllegalStateException if this instance has already been closed
   * @throws IllegalArgumentException if data is the wrong length
   * @throws LdtJniException if decryption fails
   */
  @Throws(
    IllegalStateException::class,
    IllegalArgumentException::class,
    LdtJniException::class
  )
  @DecryptAndVerifyResultCode
  fun decryptAndVerify(salt: Salt, data: ByteArray): Int {
    check(!closed) { "Double free! Close should only ever be called once" }
    requireValidSizeData(data.size)

    when (val res = LdtNpJni.decryptAndVerify(handle, salt.getBytesAsChar(), data)) {
      DecryptErrorCode.MAC_MISMATCH -> return DecryptAndVerifyResultCode.MAC_MISMATCH
      DecryptErrorCode.DATA_LEN_ERROR -> check(false); // This condition is impossible, we validated data length above
      DecryptErrorCode.JNI_OP_ERROR -> throw LdtJniException("Error occurred during jni encrypt operation")
      else -> check(res == SUCCESS)
    }
    return DecryptAndVerifyResultCode.SUCCESS
  }

  /**
   * Releases native resources.
   *
   * <p>Once closed, a Ldt instance cannot be used further.
   */
  override fun close() {
    if (!closed) {
      closed = true
      LdtNpJni.closeEncryptCipher(handle)
    }
  }
}

private fun requireValidSizeData(size: Int) {
  require(size >= BLOCK_SIZE && size < BLOCK_SIZE * 2) { "Invalid size data: $size" }
}

