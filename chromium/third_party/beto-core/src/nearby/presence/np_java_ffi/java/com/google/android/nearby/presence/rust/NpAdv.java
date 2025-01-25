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
import java.lang.ref.Cleaner;

/**
 * The main entrypoint to the library.
 *
 * <p>On Android call {@link #setCleaner} with a {@code SystemCleaner} instance before any other
 * method to avoid creating a new cleaner thread.
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

  private static @Nullable Cleaner CLEANER = null;

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
    DeserializeResult result = nativeDeserializeAdvertisement(serviceData, credentialBook);
    if (result == null) {
      result = new DeserializeResult(DeserializeResult.Kind.UNKNOWN_ERROR);
    }
    return result;
  }

  /**
   * Get the currently configured cleaner. If a cleaner is not configured, a new one will be created
   * via the {@link Cleaner#create()} factory function.
   */
  public static synchronized Cleaner getCleaner() {
    if (CLEANER == null) {
      CLEANER = Cleaner.create();
    }
    return CLEANER;
  }

  /**
   * Configure a {@link Cleaner} to be used by this library. This cleaner will be used to ensure
   * that {@link OwnedHandle} instances are properly freed. Since each {@code Cleaner} instance
   * requires its own thread; this can be used to share a {@code Cleaner} instance to reduce the
   * number of threads used.
   *
   * <p>On Android the {@code SystemCleaner} should be provided.
   */
  @Nullable
  public static synchronized Cleaner setCleaner(Cleaner cleaner) {
    Cleaner old = CLEANER;
    CLEANER = cleaner;
    return old;
  }

  @Nullable
  private static native DeserializeResult nativeDeserializeAdvertisement(
      byte[] serviceData, CredentialBook credentialBook);
}
