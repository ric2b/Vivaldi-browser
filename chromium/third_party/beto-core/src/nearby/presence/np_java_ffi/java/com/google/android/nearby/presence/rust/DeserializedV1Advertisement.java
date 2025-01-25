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

import com.google.android.nearby.presence.rust.credential.CredentialBook;
import java.util.Iterator;

/**
 * A deserialized V1 advertisement. This class is backed by native data behind the {@link
 * LegibleV1Sections} handle. If this class is closed then the underlying handle will be closed too.
 * Methods on this class should not be called if {@link #close()} has already been called.
 */
public final class DeserializedV1Advertisement<M extends CredentialBook.MatchedMetadata>
    extends DeserializedAdvertisement {

  private final int numLegibleSections;
  private final int numUndecryptableSections;
  private final LegibleV1Sections legibleSections;
  private final CredentialBook<M> credentialBook;

  /** Create a legible instance with the given information. */
  /* package */ DeserializedV1Advertisement(
      int numLegibleSections,
      int numUndecryptableSections,
      LegibleV1Sections legibleSections,
      CredentialBook<M> credentialBook) {
    this.numLegibleSections = numLegibleSections;
    this.numUndecryptableSections = numUndecryptableSections;
    this.legibleSections = legibleSections;
    this.credentialBook = credentialBook;
  }

  /** Get the number of legible sections in this advertisement */
  public int getNumLegibleSections() {
    return numLegibleSections;
  }

  /** Get the number of undecryptable sections in this advertisement */
  public int getNumUndecryptableSections() {
    return numUndecryptableSections;
  }

  /**
   * Gets the section at the given {@code index} in this advertisement. {@code index} only counts
   * legible sections.
   *
   * @param index The section's index in the advertisement
   * @throws IndexOutOfBoundsException if the index is invalid
   * @return The section at {@code index}
   */
  public DeserializedV1Section<M> getSection(int index) {
    return legibleSections.getSection(index, credentialBook);
  }

  /** Get an iterable of this advertisement's legible sections. */
  public Iterable<DeserializedV1Section<M>> getSections() {
    return () -> new SectionIterator<>(numLegibleSections, legibleSections, credentialBook);
  }

  /** Iterator instance for sections in DeserializedV1Advertisement. */
  private static final class SectionIterator<M extends CredentialBook.MatchedMetadata>
      implements Iterator<DeserializedV1Section<M>> {
    private final LegibleV1Sections legibleSections;
    private final int numSections;
    private final CredentialBook<M> credentialBook;

    private int position = 0;

    public SectionIterator(
        int numSections, LegibleV1Sections legibleSections, CredentialBook<M> credentialBook) {
      this.numSections = numSections;
      this.legibleSections = legibleSections;
      this.credentialBook = credentialBook;
    }

    @Override
    public boolean hasNext() {
      return position < numSections;
    }

    @Override
    public DeserializedV1Section<M> next() {
      return legibleSections.getSection(position++, credentialBook);
    }
  }

  /** Closes the legible sections handle if it exists. */
  @Override
  public void close() {
    if (this.legibleSections != null) {
      this.legibleSections.close();
    }
  }
}
