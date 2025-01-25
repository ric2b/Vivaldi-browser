/*
 * Copyright 2024 Google LLC
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

/** Base class for exceptions that can occur during serialization. */
public abstract class SerializationException extends Exception {

  private SerializationException(String message) {
    super(message);
  }

  public static final class InvalidDataElementException extends RuntimeException {
    public InvalidDataElementException(String reason) {
      super(String.format("Data element is invalid: %s", reason));
    }
  }

  public static final class UnclosedActiveSectionException extends RuntimeException {
    public UnclosedActiveSectionException() {
      super(
          "There is currently an active section builder which must be finished before continuing");
    }
  }

  public static final class InvalidSectionKindException extends RuntimeException {
    public InvalidSectionKindException() {
      super("A section of this kind is not allowed in this advertisement");
    }
  }

  public static final class InsufficientSpaceException extends SerializationException {
    public InsufficientSpaceException() {
      super("There isn't enough space remaining in the advertisement");
    }
  }

  public static final class LdtEncryptionException extends SerializationException {
    public LdtEncryptionException() {
      super(
          "Serializing the advertisement to bytes failed because the data in the advertisement"
              + " wasn't of an appropriate size for LDT encryption to succeed.");
    }
  }

  /**
   * Advertisement has an invalid length. This means it's empty; the length requirement is {@code
   * length >= 1 byte}.
   */
  public static final class UnencryptedSizeException extends SerializationException {
    public UnencryptedSizeException() {
      super(
          "Serializing an unencrypted adv failed because the adv data didn't meet the length"
              + " requirements.");
    }
  }
}
