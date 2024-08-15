/*
 * Copyright 2022 Google LLC
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
package com.google.android.gms.nearby.presence.hazmat;

import androidx.annotation.IntDef;

/** JNI for a Nearby Presence LDT-XTS-AES128 cipher with the "swap" mix function. */
class LdtNpJni {

  /** Error codes which map to return values on the native side. */
  @IntDef({
    DecryptErrorCode.DATA_LEN_ERROR,
    DecryptErrorCode.JNI_OP_ERROR,
    DecryptErrorCode.MAC_MISMATCH
  })
  public @interface DecryptErrorCode {
    int DATA_LEN_ERROR = -1;
    int JNI_OP_ERROR = -2;
    int MAC_MISMATCH = -3;
  }

  /** Error codes which map to return values on the native side. */
  @IntDef({EncryptErrorCode.DATA_LEN_ERROR, EncryptErrorCode.JNI_OP_ERROR})
  public @interface EncryptErrorCode {
    int DATA_LEN_ERROR = -1;
    int JNI_OP_ERROR = -2;
  }

  static {
    System.loadLibrary("ldt_np_jni");
  }

  /**
   * Create a new LDT-XTS-AES128 Encryption cipher using the "swap" mix function.
   *
   * @param keySeed is the 32-byte key material from the Nearby Presence credential from which the
   *     LDT key will be derived.
   * @return 0 on error, or a non-zero handle on success.
   */
  static native long createEncryptionCipher(byte[] keySeed);

  /**
   * Create a new LDT-XTS-AES128 Decryption cipher using the "swap" mix function.
   *
   * @param keySeed is the 32-byte key material from the Nearby Presence credential from which the
   *     LDT key will be derived.
   * @param hmacTag is the hmac auth tag calculated on the metadata key used to verify decryption
   *     was successful.
   * @return 0 on error, or a non-zero handle on success.
   */
  static native long createDecryptionCipher(byte[] keySeed, byte[] hmacTag);

  /**
   * Close the native resources for an LdtEncryptCipher instance.
   *
   * @param ldtEncryptHandle a ldt handle returned from {@link LdtNpJni#createEncryptionCipher}.
   */
  static native void closeEncryptCipher(long ldtEncryptHandle);

  /**
   * Close the native resources for an LdtDecryptCipher instance.
   *
   * @param ldtDecryptHandle a ldt handle returned from {@link LdtNpJni#createDecryptionCipher}.
   */
  static native void closeDecryptCipher(long ldtDecryptHandle);

  /**
   * Encrypt a 16-31 byte buffer in-place.
   *
   * @param ldtEncryptHandle a ldt encryption handle returned from {@link
   *     LdtNpJni#createEncryptionCipher}.
   * @param salt is the big-endian 2 byte salt that will be used in the Nearby Presence
   *     advertisement, which will be incorporated into the tweaks LDT uses while encrypting.
   * @param data 16-31 bytes of plaintext data to be encrypted
   * @return 0 on success, in which case `buffer` will now contain ciphertext or a non-zero an error
   *     code on failure
   */
  @EncryptErrorCode
  static native int encrypt(long ldtEncryptHandle, char salt, byte[] data);

  /**
   * Decrypt a 16-31 byte buffer in-place and verify the plaintext metadata key matches this item's
   * MAC, if not the buffer will not be decrypted.
   *
   * @param ldtDecryptHandle a ldt encryption handle returned from {@link
   *     LdtNpJni#createDecryptionCipher}.
   * @param salt is the big-endian 2 byte salt that will be used in the Nearby Presence
   *     advertisement, which will be incorporated into the tweaks LDT uses while encrypting.
   * @param data 16-31 bytes of ciphertext data to be decrypted
   * @return 0 on success, in which case `buffer` will now contain ciphertext or a non-zero an error
   *     code on failure, in which case data will remain unchanged.
   */
  @DecryptErrorCode
  static native int decryptAndVerify(long ldtDecryptHandle, char salt, byte[] data);
}
