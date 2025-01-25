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
import com.google.android.nearby.presence.rust.credential.CredentialBook;

/**
 * The main entrypoint to the library.
 *
 * <h3>Supported Features:</h3>
 *
 * <ul>
 *   <li>Create credential books: {@link CredentialBook#builder()}
 *   <li>Deserialize advertisements: {@link #deserializeAdvertisement}
 *   <li>Serialize advertisements: {@link
 *       com.google.android.nearby.presence.rust.V0AdvertisementBuilder}
 * </ul>
 */
public final class NpAdv {

  public static final String LIBRARY_NAME = "np_java_ffi";

  static {
    System.loadLibrary(LIBRARY_NAME);
  }

  // This is effectively an injected variable, but without depending on a DI implementation.
  @SuppressWarnings("NonFinalStaticField")
  @Nullable
  private static CooperativeCleaner cleaner = null;

  private NpAdv() {}

  /**
   * Deserialize a Nearby Presence advertisement from its service data bytes.
   *
   * @param serviceData The service data bytes. Must have length&lt;256.
   * @param credentialBook The credential book to use to decrypt.
   * @return A result containing the advertisement if it was able to be deserialized.
   */
  public static <M extends CredentialBook.MatchedMetadata>
      DeserializeResult<M> deserializeAdvertisement(
          byte[] serviceData, CredentialBook<M> credentialBook) {
    DeserializeResult<M> result = nativeDeserializeAdvertisement(serviceData, credentialBook);
    if (result == null) {
      result = new DeserializeResult<M>(DeserializeResult.Kind.UNKNOWN_ERROR);
    }
    return result;
  }

  /**
   * Get the currently configured cleaner. If a cleaner is not configured, a new one will be
   * created.
   */
  public static synchronized CooperativeCleaner getCleaner() {
    if (cleaner == null) {
      cleaner = new CooperativeCleaner();
    }
    return cleaner;
  }

  @Nullable
  private static native <M extends CredentialBook.MatchedMetadata>
      DeserializeResult<M> nativeDeserializeAdvertisement(
          byte[] serviceData, CredentialBook<M> credentialBook);
}
