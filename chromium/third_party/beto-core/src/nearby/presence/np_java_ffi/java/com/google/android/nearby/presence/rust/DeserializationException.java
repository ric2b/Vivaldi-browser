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

/** Base class for exceptions that can occur during deserialization. */
public abstract class DeserializationException extends Exception {

  private DeserializationException(String message) {
    super(message);
  }

  public static final class InvalidHeaderException extends DeserializationException {
    public InvalidHeaderException() {
      super("Invalid advertisement header");
    }
  }

  public static final class InvalidFormatException extends DeserializationException {
    public InvalidFormatException() {
      super("Invalid advertisement format");
    }
  }
}
