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

import static com.google.android.nearby.presence.rust.credential.Utils.copyBytes;

/** A V1 broadcast credential in a format that is ready to be passed to native code. */
@SuppressWarnings("UnusedVariable")
public final class V1BroadcastCredential {
  private final byte[] keySeed;
  private final byte[] identityToken;
  private final byte[] privateKey;

  /**
   * Create the credential. {@code keySeed} and {@code privateKey} are exactly 32 bytes. {@code
   * identityToken} is exactly 16 bytes
   */
  public V1BroadcastCredential(byte[] keySeed, byte[] identityToken, byte[] privateKey) {
    this.keySeed = copyBytes(keySeed, 32);
    this.identityToken = copyBytes(identityToken, 16);
    this.privateKey = copyBytes(privateKey, 32);
  }
}
