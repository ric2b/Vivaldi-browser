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

package com.google.android.nearby.presence.rust;

import androidx.annotation.Nullable;
import java.util.Arrays;

/**
 * Internal handle type for deserialized V0 advertisements. It provides access to the native data
 * and allows that data to be deallocated.
 */
public final class V0Payload extends OwnedHandle {

  static {
    System.loadLibrary(NpAdv.LIBRARY_NAME);
  }

  /** Internal data class to pass identity data back from native code. */
  public static final class IdentityDetails {
    /**
     * @see com.google.android.nearby.presence.rust.credential.CredentialBook.Builder
     */
    private final int credentialId;

    private final byte[] identityToken;
    private final byte[] salt;

    public IdentityDetails(int credentialId, byte[] identityToken, byte[] salt) {
      this.credentialId = credentialId;
      this.identityToken = Arrays.copyOf(identityToken, identityToken.length);
      this.salt = Arrays.copyOf(salt, salt.length);
    }

    public int getCredentialId() {
      return credentialId;
    }

    public byte[] getIdentityToken() {
      return identityToken;
    }

    public byte[] getSalt() {
      return salt;
    }
  }

  /**
   * Create a V0Payload handle from the raw handle id. This will use the default cleaner form {@code
   * NpAdv#getCleaner()}. This is expected to be called from native code.
   */
  /* package-visible */ V0Payload(long handleId) {
    this(handleId, NpAdv.getCleaner());
  }

  /** Create a V0Payload handle from the raw handle id. */
  private V0Payload(long handleId, CooperativeCleaner cleaner) {
    super(handleId, cleaner, V0Payload::deallocate);
  }

  /**
   * Get the data element at the given index.
   *
   * @param index The data element's index in the advertisement
   * @throws IndexOutOfBoundsException if the given index is out of range for this advertisement
   * @return The data element at that index
   */
  public V0DataElement getDataElement(int index) {
    V0DataElement de = nativeGetDataElement(this.handleId, index);
    if (de == null) {
      throw new IndexOutOfBoundsException();
    }
    return de;
  }

  @Nullable
  public IdentityDetails getIdentityDetails() {
    return nativeGetIdentityDetails(this.handleId);
  }

  @Nullable
  public byte[] getDecryptedMetadata() {
    return nativeGetDecryptedMetadata(this.handleId);
  }

  @Nullable
  private static native V0DataElement nativeGetDataElement(long handleId, int index);

  @Nullable
  private static native IdentityDetails nativeGetIdentityDetails(long handleId);

  @Nullable
  private static native byte[] nativeGetDecryptedMetadata(long handleId);

  private static native void deallocate(long handleId);
}
