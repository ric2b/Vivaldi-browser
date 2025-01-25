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

import com.google.android.nearby.presence.rust.SerializationException.InsufficientSpaceException;
import com.google.android.nearby.presence.rust.V1AdvertisementBuilder.V1BuilderHandle;
import com.google.errorprone.annotations.CanIgnoreReturnValue;

/**
 * A builder for V1 advertisement sections. Create a new instance with {@link
 * V1AdvertisementBuilder#addPublicSection()} for a public section or {@link
 * V1AdvertisementBuilder#addEncryptedSection()} for an encrypted section.
 */
public final class V1SectionBuilder implements AutoCloseable {

  private final V1BuilderHandle builder;
  private final int section;

  /**
   * Whether {@link #finishSection()} has already been called by the client. This is used to avoid
   * double-finishing when used in a try-with-resources block.
   */
  private boolean finishCalled = false;

  // Native used
  @SuppressWarnings("UnusedMethod")
  private V1SectionBuilder(V1BuilderHandle builder, int section) {
    this.builder = builder;
    this.section = section;
  }

  /**
   * Add a data element to the advertisement. If it cannot be added an exception will be thrown. A
   * thrown exception will not invalidate this builder.
   *
   * @throws InvalidDataElementException when the given data element is not valid (e.g. value out of
   *     range, too large, etc.)
   * @throws InsufficientSpaceException when the data element will not fit in the remaining space.
   */
  @CanIgnoreReturnValue
  public V1SectionBuilder addDataElement(V1DataElement dataElement)
      throws InsufficientSpaceException {
    builder.addDataElement(section, dataElement);
    return this;
  }

  /**
   * Finishes writing the current seciton. This will consume the section builder when called.
   *
   * @throws InvalidHandleException if called multiple times or if the parent advertisement has been
   *     closed.
   */
  public void finishSection() {
    builder.finishSection(section);
    finishCalled = true;
  }

  /**
   * Close this section builder. This will {@link #finishSection()} if the builder hasn't been
   * finished yet.
   */
  @Override
  public void close() {
    // Finish the section if the user has not already done so explicitly
    if (!finishCalled) {
      finishSection();
    }
  }
}
