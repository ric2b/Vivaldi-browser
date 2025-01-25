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
import java.util.Iterator;
import org.checkerframework.checker.nullness.qual.EnsuresNonNull;

/**
 * A deserialized V0 advertisement. This class is backed by native data behind the {@link V0Payload}
 * handle. If this class is closed then the underlying handle will be closed too. Methods on this
 * class should not be called if {@link #close()} has already been called.
 */
public final class DeserializedV0Advertisement<M extends CredentialBook.MatchedMetadata>
    extends DeserializedAdvertisement {
  public static boolean isLegibleIdentity(@IdentityKind int identity) {
    return identity > 0;
  }

  private final int numDataElements;
  @Nullable private final V0Payload payload;
  @IdentityKind private final int identity;
  @Nullable private final CredentialBook<M> credentialBook;

  /** Create an illegible instance with the given error identity. */
  /* package */ DeserializedV0Advertisement(@IdentityKind int illegibleIdentity) {
    if (isLegibleIdentity(illegibleIdentity)) {
      throw new IllegalArgumentException(
          "Cannot create empty DeserializedV0Advertisement with a legible identity");
    }
    this.numDataElements = 0;
    this.payload = null;
    this.identity = illegibleIdentity;
    this.credentialBook = null;
  }

  /** Create a legible instance with the given information. */
  /* package */ DeserializedV0Advertisement(
      int numDataElements,
      V0Payload payload,
      @IdentityKind int identity,
      CredentialBook<M> credentialBook) {
    this.numDataElements = numDataElements;
    this.payload = payload;
    this.identity = identity;
    this.credentialBook = credentialBook;
  }

  /**
   * Create a legible instance with the given information. Payload is specified as a raw handle id.
   * This is a helper to be called from native code to avoid needing to construct {@code V0Payload}
   * on the native side.
   */
  /* package */ DeserializedV0Advertisement(
      int numDataElements,
      long payload,
      @IdentityKind int identity,
      CredentialBook<M> credentialBook) {
    this(numDataElements, new V0Payload(payload), identity, credentialBook);
  }

  /** Check if this advertisement is legible */
  public boolean isLegible() {
    return isLegibleIdentity(this.identity);
  }

  /** Throws {@code IllegalStateException} if this advertisement is not legible. */
  @EnsuresNonNull({"payload", "credentialBook"})
  private void ensureLegible(String action) {
    if (!isLegible() || payload == null || credentialBook == null) {
      throw new IllegalStateException(
          String.format("Cannot %s for non-legible advertisement", action));
    }
  }

  /**
   * Gets the identity for this advertisement.
   *
   * @throws IllegalStateException if the advertisement is not legible ({@link #isLegible()}).
   */
  @IdentityKind
  public int getIdentity() {
    ensureLegible("get identity");
    return this.identity;
  }

  /**
   * Gets the number of data elements in this advertisement.
   *
   * @throws IllegalStateException if the advertisement is not legible ({@link #isLegible()}).
   */
  public int getDataElementCount() {
    ensureLegible("get data element count");
    return this.numDataElements;
  }

  /**
   * Gets the data element at the given {@code index} in this advertisement.
   *
   * @param index The data element's index in the advertisement
   * @throws IllegalStateException if the advertisement is not legible ({@link #isLegible()}).
   * @throws IndexOutOfBoundsException if the index is invalid
   * @return The data element at {@code index}
   */
  public V0DataElement getDataElement(int index) {
    ensureLegible("get data element");
    return payload.getDataElement(index);
  }

  /** Gets all the data elements for iteration. */
  public Iterable<V0DataElement> getDataElements() {
    ensureLegible("get data elements");
    return () -> {
      ensureLegible("get data element iterator");
      return new DataElementIterator(payload, numDataElements);
    };
  }

  /** Visits all the data elements with the given visitor. */
  public void visitDataElements(V0DataElement.Visitor v) {
    for (V0DataElement de : getDataElements()) {
      de.visit(v);
    }
  }

  /**
   * Gets the identity token for the credential this advertisement was deserialized with. This will
   * return {@code null} if this advertisement is not an encrypted advertisement.
   */
  @Nullable
  public byte[] getIdentityToken() {
    ensureLegible("get identity token");

    V0Payload.IdentityDetails details = payload.getIdentityDetails();
    return (details != null) ? details.getIdentityToken() : null;
  }

  /**
   * Gets the salt this advertisement was encrypted with. This will return {@code null} if this
   * advertisement is not an encrypted advertisement.
   */
  @Nullable
  public byte[] getSalt() {
    ensureLegible("get salt");

    V0Payload.IdentityDetails details = payload.getIdentityDetails();
    return (details != null) ? details.getSalt() : null;
  }

  /**
   * Gets the metadata matched to the credential this advertisement was deserialized with. This will
   * return {@code null} if this advertisement is not an encrypted advertisement.
   */
  @Nullable
  public M getMatchedMetadata() {
    ensureLegible("get matched metadata");

    V0Payload.IdentityDetails details = payload.getIdentityDetails();
    if (details == null) {
      return null;
    }

    return credentialBook.getMatchedMetadata(details.getCredentialId());
  }

  /**
   * Gets the metadata matched to the credential this advertisement was deserialized with. This will
   * return {@code null} if this advertisement is not an encrypted advertisement or the metadata was
   * not able to be decrypted.
   */
  @Nullable
  public byte[] getDecryptedMetadata() {
    ensureLegible("get decrypted metadata");
    return payload.getDecryptedMetadata();
  }

  /** Iterator instance for data elements in DeserializedV0Advertisement. */
  private static final class DataElementIterator implements Iterator<V0DataElement> {
    private final V0Payload payload;
    private final int numDataElements;

    private int position = 0;

    public DataElementIterator(V0Payload payload, int numDataElements) {
      this.payload = payload;
      this.numDataElements = numDataElements;
    }

    @Override
    public boolean hasNext() {
      return position < numDataElements;
    }

    @Override
    public V0DataElement next() {
      return payload.getDataElement(position++);
    }
  }

  /** Closes the payload handle if this advertisement is legible. */
  @Override
  public void close() {
    if (this.payload != null) {
      this.payload.close();
    }
  }
}
