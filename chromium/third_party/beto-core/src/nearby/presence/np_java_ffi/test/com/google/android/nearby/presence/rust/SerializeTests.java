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

import static com.google.android.nearby.presence.rust.TestData.*;
import static com.google.android.nearby.presence.rust.V0DataElement.TxPower;
import static com.google.android.nearby.presence.rust.V0DataElement.V0ActionType;
import static com.google.android.nearby.presence.rust.V0DataElement.V0Actions;
import static com.google.android.nearby.presence.rust.credential.CredentialBook.NoMetadata;
import static com.google.common.truth.Truth.assertThat;
import static org.junit.jupiter.api.Assertions.assertThrows;

import com.google.android.nearby.presence.rust.credential.CredentialBook;
import org.junit.jupiter.api.Disabled;
import org.junit.jupiter.api.Test;

public class SerializeTests {

  public static final TxPower TX_POWER = new TxPower(7);
  public static final TxPower INVALID_TX_POWER = new TxPower(-777);

  public static final V0Actions PUBLIC_ACTIONS =
      new V0Actions(IdentityKind.PLAINTEXT, V0ActionType.NEARBY_SHARE, V0ActionType.CROSS_DEV_SDK);
  public static final V0Actions PRIVATE_ACTIONS =
      new V0Actions(IdentityKind.DECRYPTED, V0ActionType.CALL_TRANSFER, V0ActionType.NEARBY_SHARE);

  public static final byte[] SALT = new byte[] {0x12, 0x34};

  @Test
  void serializeAdvertisement_v0_canSerialize() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      builder.addDataElement(TX_POWER);
      builder.addDataElement(PUBLIC_ACTIONS);
      byte[] adv = builder.build();
      assertThat(adv).isNotNull();
    }
  }

  @Test
  void serializeAdvertisement_v0_canSerializePrivate() throws Exception {
    try (V0AdvertisementBuilder builder =
        V0AdvertisementBuilder.newEncrypted(V0_BROADCAST_CRED, SALT)) {
      builder.addDataElement(TX_POWER);
      builder.addDataElement(PRIVATE_ACTIONS);
      byte[] adv = builder.build();
      assertThat(adv).isNotNull();
    }
  }

  @Test
  void serializeAdvertisement_v0_canRoundtrip() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic();
        CredentialBook book = CredentialBook.empty()) {
      builder.addDataElement(TX_POWER);
      builder.addDataElement(PUBLIC_ACTIONS);
      byte[] advBytes = builder.build();

      DeserializeResult result = NpAdv.deserializeAdvertisement(advBytes, book);
      DeserializedV0Advertisement adv = result.getAsV0();

      assertThat(adv).isNotNull();
      assertThat(adv.getDataElementCount()).isEqualTo(2);
      V0DataElement de0 = adv.getDataElement(0);
      assertThat(de0).isInstanceOf(TxPower.class);
      TxPower txPower = (TxPower) de0;
      assertThat(txPower.getTxPower()).isEqualTo(TX_POWER.getTxPower());
      V0DataElement de1 = adv.getDataElement(1);
      assertThat(de1).isInstanceOf(V0Actions.class);
      V0Actions v0Actions = (V0Actions) de1;
      assertThat(v0Actions.getIdentityKind()).isEqualTo(IdentityKind.PLAINTEXT);
      assertThat(v0Actions.getActionBits()).isEqualTo(PUBLIC_ACTIONS.getActionBits());
    }
  }

  @Test
  void serializeAdvertisement_v0_canRoundtripPrivate() throws Exception {
    try (V0AdvertisementBuilder builder =
            V0AdvertisementBuilder.newEncrypted(V0_BROADCAST_CRED, SALT);
        CredentialBook book =
            CredentialBook.builder().addDiscoveryCredential(V0_CRED, NoMetadata.INSTANCE).build()) {
      builder.addDataElement(TX_POWER);
      builder.addDataElement(PRIVATE_ACTIONS);
      byte[] advBytes = builder.build();

      DeserializeResult result = NpAdv.deserializeAdvertisement(advBytes, book);
      DeserializedV0Advertisement adv = result.getAsV0();

      assertThat(adv).isNotNull();
      assertThat(adv.getDataElementCount()).isEqualTo(2);
      V0DataElement de0 = adv.getDataElement(0);
      assertThat(de0).isInstanceOf(TxPower.class);
      TxPower txPower = (TxPower) de0;
      assertThat(txPower.getTxPower()).isEqualTo(TX_POWER.getTxPower());
      V0DataElement de1 = adv.getDataElement(1);
      assertThat(de1).isInstanceOf(V0Actions.class);
      V0Actions v0Actions = (V0Actions) de1;
      assertThat(v0Actions.getIdentityKind()).isEqualTo(IdentityKind.DECRYPTED);
      assertThat(v0Actions.getActionBits()).isEqualTo(PRIVATE_ACTIONS.getActionBits());
    }
  }

  @Test
  void serializeAdvertisement_v0_emptyIsError() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      assertThrows(SerializationException.UnencryptedSizeException.class, () -> builder.build());
    }
  }

  @Test
  void serializeAdvertisement_v0_fullIsError() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      assertThrows(
          SerializationException.InsufficientSpaceException.class,
          () -> {
            // Overfill the advertisement. Fixing b/311225033 will break this method.
            for (int i = 0; i < 50; i++) {
              builder.addDataElement(TX_POWER);
            }
          });
    }
  }

  @Test
  void serializeAdvertisement_v0_publicAdvPrivateActionsIsError() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      assertThrows(
          SerializationException.InvalidDataElementException.class,
          () -> builder.addDataElement(PRIVATE_ACTIONS));
    }
  }

  @Test
  void serializeAdvertisement_v0_privateAdvPublicActionsIsError() throws Exception {
    try (V0AdvertisementBuilder builder =
        V0AdvertisementBuilder.newEncrypted(V0_BROADCAST_CRED, SALT)) {
      assertThrows(
          SerializationException.InvalidDataElementException.class,
          () -> builder.addDataElement(PUBLIC_ACTIONS));
    }
  }

  @Test
  void serializeAdvertisement_v0_invalidTxPowerIsError() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      assertThrows(
          SerializationException.InvalidDataElementException.class,
          () -> builder.addDataElement(INVALID_TX_POWER));
    }
  }

  @Test
  void serializeAdvertisement_v0_handleIsConsumedByBuild() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      builder.addDataElement(TX_POWER);
      byte[] adv = builder.build();
      assertThrows(Handle.InvalidHandleException.class, () -> builder.addDataElement(TX_POWER));
      assertThrows(Handle.InvalidHandleException.class, () -> builder.build());
    }
  }

  @Test
  @Disabled("b/311225033: Duplicate data element spec change not implemented")
  void serializeAdvertisement_v0_deNotAddedTwice() throws Exception {
    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      builder.addDataElement(TX_POWER);
      assertThrows(Exception.class, () -> builder.addDataElement(TX_POWER));
    }
  }
}
