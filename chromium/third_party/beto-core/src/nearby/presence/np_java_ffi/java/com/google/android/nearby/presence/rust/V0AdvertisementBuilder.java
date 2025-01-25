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
import com.google.android.nearby.presence.rust.credential.V0BroadcastCredential;
import java.lang.ref.Cleaner;

/**
 * A builder for V0 advertisements. Create a new instance with {@link #newPublic()} for a public
 * advertisement or {@link #newEncrypted()} for an encrypted advertisement.
 */
public final class V0AdvertisementBuilder implements AutoCloseable {

  /** Create a builder for a public advertisement. */
  public static V0AdvertisementBuilder newPublic() {
    return newPublic(NpAdv.getCleaner());
  }

  /** Create a builder for an encrypted advertisement. */
  public static V0AdvertisementBuilder newEncrypted(V0BroadcastCredential credential, byte[] salt) {
    return newEncrypted(NpAdv.getCleaner(), credential, salt);
  }

  /** Create a builder for a public advertisement with a specific Cleaner. */
  public static V0AdvertisementBuilder newPublic(Cleaner cleaner) {
    return new V0AdvertisementBuilder(new V0BuilderHandle(cleaner));
  }

  /** Create a builder for an encrypted advertisement with a specific Cleaner. */
  public static V0AdvertisementBuilder newEncrypted(
      Cleaner cleaner, V0BroadcastCredential credential, byte[] salt) {
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
  public void addDataElement(V0DataElement dataElement) throws InsufficientSpaceException {
    builder.addDataElement(dataElement);
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

    public V0BuilderHandle(Cleaner cleaner) {
      super(allocatePublic(), cleaner, V0BuilderHandle::deallocate);
    }

    public V0BuilderHandle(Cleaner cleaner, V0BroadcastCredential credential, byte[] salt) {
      super(allocatePrivate(credential, salt), cleaner, V0BuilderHandle::deallocate);
    }

    public void addDataElement(V0DataElement dataElement) {
      // Call the appropriate native add call based on the data element type.
      dataElement.visit(
          new V0DataElement.Visitor() {
            public void visitTxPower(V0DataElement.TxPower txPower) {
              nativeAddTxPowerDataElement(txPower);
            }

            public void visitV0Actions(V0DataElement.V0Actions v0Actions) {
              nativeAddV0ActionsDataElement(v0Actions);
            }
          });
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

    private native void nativeAddTxPowerDataElement(V0DataElement.TxPower txPower);

    private native void nativeAddV0ActionsDataElement(V0DataElement.V0Actions v0Actions);

    private native byte[] nativeBuild()
        throws SerializationException.LdtEncryptionException,
            SerializationException.UnencryptedSizeException;

    private static native void deallocate(long handleId);
  }
}
