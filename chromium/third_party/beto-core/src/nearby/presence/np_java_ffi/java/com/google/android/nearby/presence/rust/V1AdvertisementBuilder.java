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

import androidx.annotation.VisibleForTesting;
import com.google.android.nearby.presence.rust.SerializationException.InsufficientSpaceException;
import com.google.android.nearby.presence.rust.credential.V1BroadcastCredential;

/**
 * A builder for V1 advertisements. Create a new instance with {@link
 * V1AdvertisementBuilder#newPublic()} for a public advertisement or {@link
 * V1AdvertisementBuilder#newEncrypted()} for an encrypted advertisement.
 *
 * <p>Public advertisements may only contain public sections. Encrypted advertisements may only
 * contain encrypted sections. Use {@link V1AdvertisementBuilder#addPublicSection()} and {@link
 * V1AdvertisementBuilder#addEncryptedSection()} to add sections to the advertisement. Only one
 * section may be built at a time.
 */
public final class V1AdvertisementBuilder implements AutoCloseable {

  /** Create a builder for a public advertisement. */
  public static V1AdvertisementBuilder newPublic() {
    return newPublic(NpAdv.getCleaner());
  }

  /** Create a builder for a public advertisement with a specific cleaner. */
  private static V1AdvertisementBuilder newPublic(CooperativeCleaner cleaner) {
    return new V1AdvertisementBuilder(new V1BuilderHandle(cleaner, false));
  }

  /** Create a builder for an encrypted advertisement. */
  public static V1AdvertisementBuilder newEncrypted() {
    return newEncrypted(NpAdv.getCleaner());
  }

  /** Create a builder for an encrypted advertisement with a specific cleaner. */
  private static V1AdvertisementBuilder newEncrypted(CooperativeCleaner cleaner) {
    return new V1AdvertisementBuilder(new V1BuilderHandle(cleaner, true));
  }

  @VisibleForTesting final V1BuilderHandle builder;

  private V1AdvertisementBuilder(V1BuilderHandle builder) {
    this.builder = builder;
  }

  /**
   * Adds a public section to this advertisement. This is only valid on public advertisements.
   *
   * @throws UnclosedActiveSectionException when there is already an active section builder.
   * @throws InvalidHandleException if this builder has already been closed.
   * @throws InvalidSectionKindException if this advertisement builder is not a public advertisement
   *     builder.
   * @throws InsufficientSpaceException if this advertisement does not have enough space for a new
   *     section
   */
  public V1SectionBuilder addPublicSection() throws InsufficientSpaceException {
    return builder.addPublicSection();
  }

  /**
   * Adds an encrypted section to this advertisement. This is only valid on encrypted
   * advertisements.
   *
   * @throws UnclosedActiveSectionException when there is already an active section builder.
   * @throws InvalidHandleException if this builder has already been closed.
   * @throws InvalidSectionKindException if this advertisement builder is not an encrypted
   *     advertisement builder.
   * @throws InsufficientSpaceException if this advertisement does not have enough space for a new
   *     section
   */
  public V1SectionBuilder addEncryptedSection(
      V1BroadcastCredential credential, @VerificationMode int verificationMode)
      throws InsufficientSpaceException {
    return builder.addEncryptedSection(credential, verificationMode);
  }

  /**
   * Build this advertisement into a byte buffer. This will consume the builder when called.
   *
   * @throws UnclosedActiveSectionException when there is an active section builder.
   */
  public byte[] build() {
    return builder.build();
  }

  /** Deallocates this section builder. */
  @Override
  public void close() {
    builder.close();
  }

  /** Internal builder handle object. */
  static final class V1BuilderHandle extends OwnedHandle {
    static {
      System.loadLibrary(NpAdv.LIBRARY_NAME);
    }

    public V1BuilderHandle(CooperativeCleaner cleaner, boolean encrypted) {
      super(allocate(encrypted), cleaner, V1BuilderHandle::deallocate);
    }

    public V1SectionBuilder addPublicSection() throws InsufficientSpaceException {
      return nativeAddPublicSection();
    }

    public V1SectionBuilder addEncryptedSection(
        V1BroadcastCredential credential, @VerificationMode int verificationMode)
        throws InsufficientSpaceException {
      return nativeAddEncryptedSection(credential, verificationMode);
    }

    private class AddDataElementVisitor implements V1DataElement.Visitor {
      private final int section;

      private AddDataElementVisitor(int section) {
        this.section = section;
      }

      @Override
      public void visitGeneric(V1DataElement.Generic generic) {
        try {
          nativeAddGenericDataElement(section, generic);
        } catch (InsufficientSpaceException ise) {
          throw new SmuggledInsufficientSpaceException(ise);
        }
      }
    }

    /**
     * Helper to smuggle {@link InsufficientSpaceException} (a checked exception) through APIs that
     * don't support checked exceptions.
     */
    private static class SmuggledInsufficientSpaceException extends RuntimeException {
      private final InsufficientSpaceException ise;

      public SmuggledInsufficientSpaceException(InsufficientSpaceException ise) {
        this.ise = ise;
      }

      public void throwChecked() throws InsufficientSpaceException {
        throw ise;
      }
    }

    public void addDataElement(int section, V1DataElement dataElement)
        throws InsufficientSpaceException {
      try {
        dataElement.visit(new AddDataElementVisitor(section));
      } catch (SmuggledInsufficientSpaceException ex) {
        ex.throwChecked();
      }
    }

    public void finishSection(int section) {
      nativeFinishSection(section);
    }

    public byte[] build() {
      // `nativeBuild` takes ownership so we leak the Java side here.
      leak();
      return nativeBuild();
    }

    private static native long allocate(boolean encrypted);

    private native V1SectionBuilder nativeAddPublicSection() throws InsufficientSpaceException;

    private native V1SectionBuilder nativeAddEncryptedSection(
        V1BroadcastCredential credential, @VerificationMode int verificationMode)
        throws InsufficientSpaceException;

    private native void nativeAddGenericDataElement(int section, V1DataElement.Generic generic)
        throws InsufficientSpaceException;

    private native void nativeFinishSection(int section);

    private native byte[] nativeBuild();

    private static native void deallocate(long handleId);
  }
}
