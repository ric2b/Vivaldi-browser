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

import static java.lang.annotation.RetentionPolicy.SOURCE;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import com.google.android.nearby.presence.rust.credential.CredentialBook;
import java.lang.annotation.Retention;

/**
 * A result of calling {@link NpAdv#deserializeAdvertisement}. This represents either an error
 * condition or successfully deserialized advertisement.
 *
 * <p>Note: A successfully deserialized advertisement may still not be legible for cryptographic
 * reasons. That condition is reported on the advertisement object itself since a V1 advertisement
 * may be partially legible (some sections are legible, not all).
 */
public final class DeserializeResult<M extends CredentialBook.MatchedMetadata>
    implements AutoCloseable {

  /** The kind of result. Negative values are errors and do not contain advertisement instances. */
  @IntDef({
    Kind.UNKNOWN_ERROR,
    Kind.V0_ADVERTISEMENT,
    Kind.V1_ADVERTISEMENT,
  })
  @Retention(SOURCE)
  public @interface Kind {
    public static final int UNKNOWN_ERROR = -1;

    public static final int V0_ADVERTISEMENT = 1;
    public static final int V1_ADVERTISEMENT = 2;
  }

  /** Checks if a {@link Kind} represents and error or not. */
  public static boolean isErrorKind(@Kind int kind) {
    return kind <= 0;
  }

  @Kind private final int kind;
  @Nullable private final DeserializedAdvertisement advertisement;

  /** Create a DeserializeResult containing an error code */
  /* package */ DeserializeResult(@Kind int errorKind) {
    if (!isErrorKind(errorKind)) {
      throw new IllegalArgumentException(
          "Cannot create empty DeserializeResult with non-error kind");
    }
    this.kind = errorKind;
    this.advertisement = null;
  }

  /** Create a DeserializeResult containing a V0 advertisement */
  /* package */ DeserializeResult(DeserializedV0Advertisement<M> advertisement) {
    this.kind = Kind.V0_ADVERTISEMENT;
    this.advertisement = advertisement;
  }

  /** Create a DeserializeResult containing a V1 advertisement */
  /* package */ DeserializeResult(DeserializedV1Advertisement<M> advertisement) {
    this.kind = Kind.V1_ADVERTISEMENT;
    this.advertisement = advertisement;
  }

  /** Gets the kind of this result. */
  @Kind
  public int getKind() {
    return kind;
  }

  /** Check if this result is an error result. */
  public boolean isError() {
    return isErrorKind(kind);
  }

  /**
   * Gets the contained V0 advertisement if present.
   *
   * @return the contained V0 advertisement or {@code null} if not present
   */
  @SuppressWarnings("unchecked")
  @Nullable
  public DeserializedV0Advertisement<M> getAsV0() {
    if (this.kind != Kind.V0_ADVERTISEMENT) {
      return null;
    }
    return (DeserializedV0Advertisement<M>) this.advertisement;
  }

  /**
   * Gets the contained V1 advertisement if present.
   *
   * @return the contained V1 advertisement or {@code null} if not present
   */
  @SuppressWarnings("unchecked")
  @Nullable
  public DeserializedV1Advertisement<M> getAsV1() {
    if (this.kind != Kind.V1_ADVERTISEMENT) {
      return null;
    }
    return (DeserializedV1Advertisement<M>) this.advertisement;
  }

  /** Closes the contained advertisement if it exists. */
  @Override
  public void close() {
    if (this.advertisement != null) {
      this.advertisement.close();
    }
  }
}
