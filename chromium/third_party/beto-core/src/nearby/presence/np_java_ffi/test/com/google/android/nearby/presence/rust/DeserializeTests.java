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

import static com.google.android.nearby.presence.rust.TestData.V0_CRED;
import static com.google.android.nearby.presence.rust.TestData.V0_IDENTITY_TOKEN;
import static com.google.android.nearby.presence.rust.TestData.V0_PRIVATE;
import static com.google.android.nearby.presence.rust.TestData.V0_PUBLIC;
import static com.google.android.nearby.presence.rust.TestData.V1_CRED;
import static com.google.android.nearby.presence.rust.TestData.V1_IDENTITY_TOKEN;
import static com.google.android.nearby.presence.rust.TestData.V1_PRIVATE;
import static com.google.android.nearby.presence.rust.TestData.V1_PUBLIC;
import static com.google.common.truth.Truth.assertThat;

import com.google.android.nearby.presence.rust.credential.CredentialBook;
import com.google.android.nearby.presence.rust.credential.CredentialBook.NoMetadata;
import java.util.ArrayList;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class DeserializeTests {

  DeserializeResult<NoMetadata> parsePublicAdv(byte[] bytes) {
    // Call parse with an empty CredentialBook
    try (CredentialBook<NoMetadata> book = CredentialBook.empty()) {
      return NpAdv.deserializeAdvertisement(bytes, book);
    }
  }

  DeserializeResult<NoMetadata> parsePrivateAdv(byte[] bytes) {
    try (CredentialBook<NoMetadata> book =
        CredentialBook.<NoMetadata>builder()
            .addDiscoveryCredential(V0_CRED, NoMetadata.INSTANCE)
            .addDiscoveryCredential(V1_CRED, NoMetadata.INSTANCE)
            .build()) {
      return NpAdv.deserializeAdvertisement(bytes, book);
    }
  }

  @Test
  public void deserializeAdvertisement_v0_canParsePublic() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V0_PUBLIC)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V0_ADVERTISEMENT);

      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();

      assertThat(adv).isNotNull();
      assertThat(adv.isLegible()).isTrue();
      assertThat(adv.getIdentity()).isEqualTo(IdentityKind.PLAINTEXT);
      assertThat(adv.getDataElementCount()).isEqualTo(2);
      assertThat(adv.getMatchedMetadata()).isNull();
      assertThat(adv.getDecryptedMetadata()).isNull();
    }
  }

  @Test
  public void deserializeAdvertisement_v0_canParsePublicWithCreds() {
    try (DeserializeResult<NoMetadata> result = parsePrivateAdv(V0_PUBLIC)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V0_ADVERTISEMENT);

      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();

      assertThat(adv).isNotNull();
      assertThat(adv.isLegible()).isTrue();
      assertThat(adv.getIdentity()).isEqualTo(IdentityKind.PLAINTEXT);
      assertThat(adv.getDataElementCount()).isEqualTo(2);
      assertThat(adv.getMatchedMetadata()).isNull();
      assertThat(adv.getDecryptedMetadata()).isNull();
    }
  }

  @Test
  public void deserializeAdvertisement_v0_canParsePrivate() {
    try (DeserializeResult<NoMetadata> result = parsePrivateAdv(V0_PRIVATE)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V0_ADVERTISEMENT);

      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();

      assertThat(adv).isNotNull();
      assertThat(adv.isLegible()).isTrue();
      assertThat(adv.getIdentity()).isEqualTo(IdentityKind.DECRYPTED);
      assertThat(adv.getDataElementCount()).isEqualTo(1);
      assertThat(adv.getDataElement(0)).isInstanceOf(V0DataElement.TxPower.class);
      assertThat(adv.getIdentityToken()).isEqualTo(V0_IDENTITY_TOKEN);
      assertThat(adv.getSalt()).isEqualTo(new byte[] {(byte) 0x22, (byte) 0x22});
      assertThat(adv.getMatchedMetadata()).isSameInstanceAs(NoMetadata.INSTANCE);
      assertThat(adv.getDecryptedMetadata()).isNull();
    }
  }

  @Test
  public void deserializeAdvertisement_v0_cannotParsePrivateWithoutCreds() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V0_PRIVATE)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V0_ADVERTISEMENT);

      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();

      assertThat(adv).isNotNull();
      assertThat(adv.isLegible()).isFalse();
    }
  }

  @Test
  public void deserializeAdvertisement_v0_canReadTxPowerDe() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V0_PUBLIC)) {
      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();

      V0DataElement de = adv.getDataElement(0);

      assertThat(de).isInstanceOf(V0DataElement.TxPower.class);
      V0DataElement.TxPower txPower = (V0DataElement.TxPower) de;
      assertThat(txPower.getTxPower()).isEqualTo(20);
    }
  }

  @Test
  public void deserializeAdvertisement_v0_canIterateDataElements() throws Exception {
    final int numDes = 2;
    byte[] advBytes;

    try (V0AdvertisementBuilder builder = V0AdvertisementBuilder.newPublic()) {
      builder.addDataElement(
          new V0DataElement.V0Actions(
              IdentityKind.PLAINTEXT, V0DataElement.V0ActionType.NEARBY_SHARE));
      builder.addDataElement(new V0DataElement.TxPower(10));
      advBytes = builder.build();
    }

    try (DeserializeResult<NoMetadata> result = parsePublicAdv(advBytes)) {
      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();
      ArrayList<V0DataElement> deList = new ArrayList<>();
      for (V0DataElement de : adv.getDataElements()) {
        deList.add(de);
      }

      // Validate de order
      assertThat(deList.get(0)).isInstanceOf(V0DataElement.V0Actions.class);
      assertThat(deList.get(1)).isInstanceOf(V0DataElement.TxPower.class);
      // Validate de count
      assertThat(deList.size()).isEqualTo(numDes);
    }
  }

  @Test
  public void deserializeAdvertisement_v0_canReadActionsDe() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V0_PUBLIC)) {
      DeserializedV0Advertisement<NoMetadata> adv = result.getAsV0();

      V0DataElement de = adv.getDataElement(1);

      assertThat(de).isInstanceOf(V0DataElement.V0Actions.class);
      V0DataElement.V0Actions actions = (V0DataElement.V0Actions) de;
      assertThat(actions.getIdentityKind()).isEqualTo(IdentityKind.PLAINTEXT);
      assertThat(actions.hasAction(V0DataElement.V0ActionType.NEARBY_SHARE)).isTrue();
      assertThat(actions.hasAction(V0DataElement.V0ActionType.CROSS_DEV_SDK)).isFalse();
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canParsePublic() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V1_PUBLIC)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V1_ADVERTISEMENT);

      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();

      assertThat(adv).isNotNull();
      assertThat(adv.getNumLegibleSections()).isEqualTo(1);
      assertThat(adv.getNumUndecryptableSections()).isEqualTo(0);
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canParsePrivate() {
    try (DeserializeResult<NoMetadata> result = parsePrivateAdv(V1_PRIVATE)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V1_ADVERTISEMENT);

      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();

      assertThat(adv).isNotNull();
      assertThat(adv.getNumLegibleSections()).isEqualTo(1);
      assertThat(adv.getNumUndecryptableSections()).isEqualTo(0);

      DeserializedV1Section<NoMetadata> section = adv.getSection(0);
      assertThat(section.getIdentityKind()).isEqualTo(IdentityKind.DECRYPTED);
      assertThat(section.getDataElementCount()).isEqualTo(1);
      assertThat(section.getIdentityToken()).isEqualTo(V1_IDENTITY_TOKEN);
      assertThat(section.getVerificationMode()).isEqualTo(VerificationMode.SIGNATURE);
      assertThat(section.getMatchedMetadata()).isSameInstanceAs(NoMetadata.INSTANCE);
      assertThat(section.getDecryptedMetadata()).isNull();
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canParsePrivateWithoutCreds() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V1_PRIVATE)) {
      assertThat(result.getKind()).isEqualTo(DeserializeResult.Kind.V1_ADVERTISEMENT);

      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();

      assertThat(adv).isNotNull();
      assertThat(adv.getNumLegibleSections()).isEqualTo(0);
      assertThat(adv.getNumUndecryptableSections()).isEqualTo(1);
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canParsePublicSection() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V1_PUBLIC)) {
      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();

      DeserializedV1Section<NoMetadata> section = adv.getSection(0);

      assertThat(section).isNotNull();
      assertThat(section.getIdentityKind()).isEqualTo(IdentityKind.PLAINTEXT);
      assertThat(section.getDataElementCount()).isEqualTo(1);
      assertThat(section.getMatchedMetadata()).isNull();
      assertThat(section.getDecryptedMetadata()).isNull();
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canReadGenericDe() {
    try (DeserializeResult<NoMetadata> result = parsePublicAdv(V1_PUBLIC)) {
      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();
      DeserializedV1Section<NoMetadata> section = adv.getSection(0);

      V1DataElement de = section.getDataElement(0);

      assertThat(de).isNotNull();
      assertThat(de).isInstanceOf(V1DataElement.Generic.class);
      V1DataElement.Generic generic = (V1DataElement.Generic) de;
      assertThat(generic.getType()).isEqualTo(0x05 /* V1 TxPower */);
      assertThat(generic.getData()).isEqualTo(new byte[] {(byte) 6});
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canIterateSections() throws Exception {
    final int NUM_SECTIONS = 5;
    byte[] advBytes;

    try (V1AdvertisementBuilder builder = V1AdvertisementBuilder.newPublic()) {
      for (int i = 0; i < NUM_SECTIONS; i++) {
        builder
            .addPublicSection()
            .addDataElement(new V1DataElement.Generic(123, new byte[] {(byte) i}))
            .finishSection();
      }
      advBytes = builder.build();
    }

    try (DeserializeResult<NoMetadata> result = parsePublicAdv(advBytes)) {
      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();

      byte i = 0;
      for (DeserializedV1Section<NoMetadata> section : adv.getSections()) {
        V1DataElement.Generic de = (V1DataElement.Generic) section.getDataElement(0);
        assertThat(de.getType()).isEqualTo(123);
        // Validate section order
        assertThat(de.getData()).isEqualTo(new byte[] {i++});
      }
      // Validate section count
      assertThat(i).isEqualTo(NUM_SECTIONS);
    }
  }

  @Test
  public void deserializeAdvertisement_v1_canIterateDataElements() throws Exception {
    final int numDes = 5;
    byte[] advBytes;

    try (V1AdvertisementBuilder builder = V1AdvertisementBuilder.newPublic()) {
      try (V1SectionBuilder section = builder.addPublicSection()) {
        for (int i = 0; i < numDes; i++) {
          section.addDataElement(new V1DataElement.Generic(123, new byte[] {(byte) i}));
        }
      }
      advBytes = builder.build();
    }

    try (DeserializeResult<NoMetadata> result = parsePublicAdv(advBytes)) {
      DeserializedV1Advertisement<NoMetadata> adv = result.getAsV1();
      DeserializedV1Section<NoMetadata> section = adv.getSection(0);

      byte i = 0;
      for (V1DataElement de : section.getDataElements()) {
        V1DataElement.Generic generic = (V1DataElement.Generic) de;
        assertThat(generic.getType()).isEqualTo(123);
        // Validate de order
        assertThat(generic.getData()).isEqualTo(new byte[] {i++});
      }
      // Validate de count
      assertThat(i).isEqualTo(numDes);
    }
  }
}
