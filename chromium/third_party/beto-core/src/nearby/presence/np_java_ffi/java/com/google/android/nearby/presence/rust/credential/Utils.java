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

import java.util.Arrays;

/** Util functions used by multiple files. */
final class Utils {

  private Utils() {}

  /**
   * Create a copy of a {@code n}-byte array of data. Will throw {@code IllegalArgumentException} if
   * the array is not exactly {@code n} bytes.
   */
  public static byte[] copyBytes(byte[] key, int n) {
    if (key.length != n) {
      throw new IllegalArgumentException(
          String.format("Expected key length to be %s bytes, got %s bytes", n, key.length));
    }
    return Arrays.copyOf(key, key.length);
  }
}
