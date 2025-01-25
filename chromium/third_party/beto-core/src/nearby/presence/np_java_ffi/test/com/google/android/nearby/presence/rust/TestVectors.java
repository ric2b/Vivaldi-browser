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

import static com.google.common.truth.Truth.assertThat;

import com.google.android.nearby.presence.rust.credential.V1BroadcastCredential;
import com.google.gson.FieldNamingPolicy;
import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonDeserializationContext;
import com.google.gson.JsonDeserializer;
import com.google.gson.JsonElement;
import com.google.gson.JsonParseException;
import com.google.gson.reflect.TypeToken;
import java.io.BufferedReader;
import java.lang.reflect.Type;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.security.SecureRandom;
import java.util.Arrays;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class TestVectors {

  static final class DataElement {
    byte[] contents;
    int deType;
  }

  static final class TestVector {
    byte[] keySeed;
    byte[] identityToken;
    byte[] advHeaderByte;
    byte[] sectionSalt;
    DataElement[] dataElements;
    byte[] aesKey;
    byte[] sectionMicHmacKey;
    byte[] nonce;
    byte[] encodedSection;
  }

  static final class HexArrayDeserializer implements JsonDeserializer<byte[]> {

    private static final byte byteFromHexChar(char c) {
      if (c >= '0' && c <= '9') {
        return (byte) (c - '0');
      } else if (c >= 'a' && c <= 'f') {
        return (byte) (c - 'a' + 0x0a);
      } else if (c >= 'A' && c <= 'F') {
        return (byte) (c - 'A' + 0x0a);
      } else {
        throw new IllegalArgumentException("Invalid hex char: " + c);
      }
    }

    @Override
    public byte[] deserialize(JsonElement json, Type typeOfT, JsonDeserializationContext context)
        throws JsonParseException {
      String hex = json.getAsString();
      int byteLen = hex.length() / 2;
      byte[] out = new byte[byteLen];

      for (int i = 0; i < byteLen; i++) {
        int offset = i * 2;
        out[i] =
            (byte)
                ((byteFromHexChar(hex.charAt(offset)) << 4)
                    | byteFromHexChar(hex.charAt(offset + 1)));
      }

      return out;
    }
  }

  static Gson createJsonParser() {
    return new GsonBuilder()
        .setFieldNamingPolicy(FieldNamingPolicy.LOWER_CASE_WITH_UNDERSCORES)
        .registerTypeAdapter(byte[].class, new HexArrayDeserializer())
        .create();
  }

  @Test
  public void micExtendedSaltEncryptedTestVectors() throws Exception {
    Gson gson = createJsonParser();
    TestVector[] testVectors = null;
    try (BufferedReader br =
        Files.newBufferedReader(
            Paths.get(
                "..",
                "np_adv",
                "resources",
                "test",
                "mic-extended-salt-encrypted-test-vectors.json"))) {
      testVectors = gson.fromJson(br, new TypeToken<TestVector[]>() {});
    }
    assertThat(testVectors).isNotNull();

    for (TestVector tv : testVectors) {
      byte[] privateKey = new byte[32];
      SecureRandom.getInstanceStrong().nextBytes(privateKey);
      V1BroadcastCredential credential =
          new V1BroadcastCredential(tv.keySeed, tv.identityToken, privateKey);
      try (V1AdvertisementBuilder builder = V1AdvertisementBuilder.newEncrypted();
          V1SectionBuilder section =
              nativeAddSaltedSection(builder.builder, credential, tv.sectionSalt)) {
        for (DataElement de : tv.dataElements) {
          section.addDataElement(new V1DataElement.Generic(de.deType, de.contents));
        }
        section.finishSection();

        byte[] adv = builder.build();
        byte[] sectionBytes = Arrays.copyOfRange(adv, 1, adv.length);

        assertThat(sectionBytes).isEqualTo(tv.encodedSection);
      }
    }
  }

  @Test
  public void createJsonParser_canParseTestVector() {
    Gson gson = createJsonParser();
    String testJson =
        "{ \"adv_header_byte\": \"20\", \"aes_key\": \"F3DB017C70E08EC5178C92F3AEA0C362\","
            + " \"data_elements\": [ { \"contents\": \"CF75D23EDA8F6E4A23\", \"de_type\": 383 }, {"
            + " \"contents\": \"731B76151735869205CC41\", \"de_type\": 73 }, { \"contents\":"
            + " \"7C2A8DE86B2CBB997703\", \"de_type\": 228 }, { \"contents\":"
            + " \"99F5163DCA0BB9BE89755A6C5AB321\", \"de_type\": 446 } ], \"encoded_section\":"
            + " \"6D91100056F596D16E1F87B107EE86102FFC6D5E9002F00C41CDDF533667362CD14AC54A9388FA3D30AA7CA6603071B8B0FA19BF582479F773F1C7D0EADF98E98B9447139F244D571B780475ACD9CB248F33B2C085925213360732D44081C27CA6EB40BD8A626BC776D88C5FDB09\","
            + " \"key_seed\": \"F0ED9126768CE7DC685FF74932AC5A876442C4E42359A43F720A575142A45043\","
            + " \"identity_token\": \"45795EE4C6533A830886E2C5885EB9E5\", \"nonce\":"
            + " \"40E95D525FAEA1C1FEE39A8E\", \"section_mic_hmac_key\":"
            + " \"A22386E85112EF883218A5B75669B7102E017E9AA149F408A079F60B1D14B4F4\","
            + " \"section_salt\": \"56F596D16E1F87B107EE86102FFC6D5E\" }";

    TestVector testVector = gson.fromJson(testJson, TestVector.class);

    assertThat(testVector.aesKey)
        .isEqualTo(
            new byte[] {
              (byte) 0xF3,
              (byte) 0xDB,
              (byte) 0x01,
              (byte) 0x7C,
              (byte) 0x70,
              (byte) 0xE0,
              (byte) 0x8E,
              (byte) 0xC5,
              (byte) 0x17,
              (byte) 0x8C,
              (byte) 0x92,
              (byte) 0xF3,
              (byte) 0xAE,
              (byte) 0xA0,
              (byte) 0xC3,
              (byte) 0x62
            });
  }

  // Expose the test-only salted section API
  private static native V1SectionBuilder nativeAddSaltedSection(
      V1AdvertisementBuilder.V1BuilderHandle builder,
      V1BroadcastCredential credential,
      byte[] salt);
}
