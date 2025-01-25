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

package com.google.android.nearby.presence.rust.credential;

import com.google.android.nearby.presence.rust.CooperativeCleaner;
import com.google.android.nearby.presence.rust.NpAdv;
import com.google.android.nearby.presence.rust.OwnedHandle;

/**
 * A {@code CredentialSlab} handle that is used to build the native-side structures for a {@link
 * CredentialBook}. Clients should use {@link CredentialSlab.Builder} instead of this class.
 */
final class CredentialSlab extends OwnedHandle {
  static {
    System.loadLibrary(NpAdv.LIBRARY_NAME);
  }

  /** Create a new {@code CredentialSlab} with the given {@code cleaner}. */
  public CredentialSlab(CooperativeCleaner cleaner) {
    super(allocate(), cleaner, CredentialSlab::deallocate);
  }

  /** Add a V0 discovery credential to this slab. */
  public void addDiscoveryCredential(
      V0DiscoveryCredential credential, int credId, byte[] encryptedMetadataBytes) {
    if (!nativeAddV0DiscoveryCredential(handleId, credential, credId, encryptedMetadataBytes)) {
      throw new IllegalStateException("Failed to add credential to slab");
    }
  }

  /**
   * Add a V1 discovery credential to this slab. May throw {@link
   * CredentialBook.InvalidKeyException} if the key inside {@code credential} is improperly
   * formatted.
   */
  public void addDiscoveryCredential(
      V1DiscoveryCredential credential, int credId, byte[] encryptedMetadataBytes) {
    if (!nativeAddV1DiscoveryCredential(handleId, credential, credId, encryptedMetadataBytes)) {
      throw new IllegalStateException("Failed to add credential to slab");
    }
  }

  /**
   * Mark this slab handle as moved and return the handle id. This will leak the handle. This should
   * be done when the handle is moved to the Rust side to avoid freeing it early.
   */
  public long move() {
    leak();
    return handleId;
  }

  private static native long allocate();

  private static native boolean nativeAddV0DiscoveryCredential(
      long handleId, V0DiscoveryCredential cred, int credId, byte[] encryptedMetadataBytes);

  private static native boolean nativeAddV1DiscoveryCredential(
      long handleId, V1DiscoveryCredential cred, int credId, byte[] encryptedMetadataBytes);

  private static native void deallocate(long handleId);
}
