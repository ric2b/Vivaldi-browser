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

import com.google.android.nearby.presence.rust.SerializationException.InsufficientSpaceException;
import com.google.android.nearby.presence.rust.SerializationException.InvalidDataElementException;
import com.google.android.nearby.presence.rust.V0DataElement.TxPower;
import com.google.android.nearby.presence.rust.V0DataElement.V0Actions;
import com.google.android.nearby.presence.rust.credential.V0BroadcastCredential;
import com.google.errorprone.annotations.CanIgnoreReturnValue;

/**
 * A builder for V0 advertisements. Create a new instance with {@link
 * V0AdvertisementBuilder#newPublic()} for a public advertisement or {@link
 * V0AdvertisementBuilder#newEncrypted()} for an encrypted advertisement.
 */
public final class V0AdvertisementBuilder implements AutoCloseable {

  /** Create a builder for a public advertisement. */
  public static V0AdvertisementBuilder newPublic() {
    return newPublic(NpAdv.getCleaner());
  }

  /** Create a builder for a public advertisement with a specific cleaner. */
  private static V0AdvertisementBuilder newPublic(CooperativeCleaner cleaner) {
    return new V0AdvertisementBuilder(new V0BuilderHandle(cleaner));
  }

  /** Create a builder for an encrypted advertisement. */
  public static V0AdvertisementBuilder newEncrypted(V0BroadcastCredential credential, byte[] salt) {
    return newEncrypted(NpAdv.getCleaner(), credential, salt);
  }

  /** Create a builder for an encrypted advertisement with a specific cleaner. */
  private static V0AdvertisementBuilder newEncrypted(
      CooperativeCleaner cleaner, V0BroadcastCredential credential, byte[] salt) {
    return new V0AdvertisementBuilder(new V0BuilderHandle(cleaner, credential, salt));
  }

  private final V0BuilderHandle builder;

  private V0AdvertisementBuilder(V0BuilderHandle builder) {
    this.builder = builder;
  }

  /**
   * Add a data element to the advertisement. If it cannot be added an exception will be thrown. A
   * thrown exception will not invalidate this builder.
   *
   * @throws InvalidDataElementException when the given data element is not valid (e.g. TX power out
   *     of range)
   * @throws InsufficientSpaceException when the data element will not fit in the remaining space.
   */
  @CanIgnoreReturnValue
  public V0AdvertisementBuilder addDataElement(V0DataElement dataElement)
      throws InsufficientSpaceException {
    builder.addDataElement(dataElement);
    return this;
  }

  /**
   * Build this advertisement into a byte buffer. This will consume the builder when called.
   *
   * @throws LdtEncryptionException when the advertisement cannot be encrypted.
   * @throws UnencryptedSizeException when the advertisement is empty.
   */
  public byte[] build()
      throws SerializationException.LdtEncryptionException,
          SerializationException.UnencryptedSizeException {
    return builder.build();
  }

  @Override
  public void close() {
    builder.close();
  }

  /** Internal builder handle object. */
  private static final class V0BuilderHandle extends OwnedHandle {
    static {
      System.loadLibrary(NpAdv.LIBRARY_NAME);
    }

    /** Create a public builder. */
    public V0BuilderHandle(CooperativeCleaner cleaner) {
      super(allocatePublic(), cleaner, V0BuilderHandle::deallocate);
    }

    /** Create an encrypted builder. */
    public V0BuilderHandle(
        CooperativeCleaner cleaner, V0BroadcastCredential credential, byte[] salt) {
      super(allocatePrivate(credential, salt), cleaner, V0BuilderHandle::deallocate);
    }

    private class AddDataElementVisitor implements V0DataElement.Visitor {
      @Override
      public void visitTxPower(TxPower txPower) {
        try {
          nativeAddTxPowerDataElement(txPower);
        } catch (InsufficientSpaceException ise) {
          throw new SmuggledInsufficientSpaceException(ise);
        }
      }

      @Override
      public void visitV0Actions(V0Actions actions) {
        try {
          nativeAddV0ActionsDataElement(actions);
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

    public void addDataElement(V0DataElement dataElement) throws InsufficientSpaceException {
      try {
        dataElement.visit(new AddDataElementVisitor());
      } catch (SmuggledInsufficientSpaceException ex) {
        ex.throwChecked();
      }
    }

    public byte[] build()
        throws SerializationException.LdtEncryptionException,
            SerializationException.UnencryptedSizeException {
      // `nativeBuild` takes ownership so we leak the Java side here.
      leak();
      return nativeBuild();
    }

    private static native long allocatePublic();

    private static native long allocatePrivate(V0BroadcastCredential credential, byte[] salt);

    private native void nativeAddTxPowerDataElement(TxPower txPower)
        throws InsufficientSpaceException;

    private native void nativeAddV0ActionsDataElement(V0Actions v0Actions)
        throws InsufficientSpaceException;

    private native byte[] nativeBuild()
        throws SerializationException.LdtEncryptionException,
            SerializationException.UnencryptedSizeException;

    private static native void deallocate(long handleId);
  }
}
