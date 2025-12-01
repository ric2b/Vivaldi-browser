// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "fcp/confidentialcompute/crypto.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/confidentialcompute/cose.h"
#include "fcp/testing/testing.h"
#include "openssl/base.h"
#include "openssl/hpke.h"

namespace fcp {
namespace confidential_compute {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SaveArg;

// Helper function to generate a new public and private key pair (HPKE_KEY).
// This function uses output parameters because ScopedEVP_HPKE_KEY is not
// movable in older versions of BoringSSL.
void GenerateKeyPair(const EVP_HPKE_KEM& kem, std::string& public_key,
                     bssl::ScopedEVP_HPKE_KEY& private_key) {
  CHECK_EQ(EVP_HPKE_KEY_generate(private_key.get(), &kem), 1);
  size_t public_key_len;
  public_key.resize(EVP_HPKE_MAX_PUBLIC_KEY_LENGTH, '\0');
  CHECK_EQ(EVP_HPKE_KEY_public_key(
               private_key.get(), reinterpret_cast<uint8_t*>(public_key.data()),
               &public_key_len, public_key.size()),
           1);
  public_key.resize(public_key_len);
}

// Helper function to generate a new public and private key pair (raw strings).
std::pair<std::string, std::string> GenerateKeyPair(const EVP_HPKE_KEM& kem) {
  bssl::ScopedEVP_HPKE_KEY key;
  CHECK_EQ(EVP_HPKE_KEY_generate(key.get(), &kem), 1);
  size_t key_len;
  std::string public_key(EVP_HPKE_MAX_PUBLIC_KEY_LENGTH, '\0');
  CHECK_EQ(EVP_HPKE_KEY_public_key(
               key.get(), reinterpret_cast<uint8_t*>(public_key.data()),
               &key_len, public_key.size()),
           1);
  public_key.resize(key_len);
  std::string private_key(EVP_HPKE_MAX_PRIVATE_KEY_LENGTH, '\0');
  CHECK_EQ(EVP_HPKE_KEY_private_key(
               key.get(), reinterpret_cast<uint8_t*>(private_key.data()),
               &key_len, private_key.size()),
           1);
  private_key.resize(key_len);
  return {public_key, private_key};
}

TEST(CryptoTest, GetPublicKey) {
  testing::MockFunction<absl::StatusOr<std::string>(absl::string_view)> signer;
  std::string sig_structure;
  EXPECT_CALL(signer, Call(_))
      .WillOnce(DoAll(SaveArg<0>(&sig_structure), Return("signature")));

  google::protobuf::Struct config_properties;
  (*config_properties.mutable_fields())["key"].set_string_value("value");

  MessageDecryptor decryptor(config_properties.SerializeAsString());
  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey(signer.AsStdFunction(), 7);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(cwt);
  EXPECT_EQ(cwt->algorithm, 7);
  ASSERT_NE(cwt->public_key, std::nullopt);
  EXPECT_EQ(cwt->public_key->algorithm,
            crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm);
  EXPECT_THAT(cwt->public_key->key_ops, ElementsAre(kCoseKeyOpEncrypt));
  EXPECT_EQ(cwt->public_key->curve, crypto_internal::kX25519);
  EXPECT_NE(cwt->public_key->x, "");
  EXPECT_EQ(cwt->config_properties, config_properties.SerializeAsString());
  EXPECT_EQ(cwt->signature, "signature");

  // The signature structure is a COSE implementation detail, but it should at
  // least contain the public key.
  EXPECT_THAT(sig_structure, HasSubstr(cwt->public_key->x));
}

TEST(CryptoTest, GetPublicKeyCwtSigningError) {
  testing::MockFunction<absl::StatusOr<std::string>(absl::string_view)> signer;
  EXPECT_CALL(signer, Call(_))
      .WillOnce(Return(absl::FailedPreconditionError("")));

  MessageDecryptor decryptor;
  EXPECT_THAT(decryptor.GetPublicKey(signer.AsStdFunction(), 0),
              IsCode(FAILED_PRECONDITION));
}

TEST(CryptoTest, EncryptAndDecrypt) {
  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, associated_data,
                        encrypt_result->encrypted_symmetric_key,
                        associated_data, encrypt_result->encapped_key);
  ASSERT_OK(decrypt_result);
  EXPECT_EQ(*decrypt_result, message);
}

TEST(CryptoTest, EncryptRewrapKeyAndDecrypt) {
  std::string message = "some plaintext message";
  std::string message_associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);
  absl::StatusOr<OkpCwt> recipient_cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(recipient_cwt);
  ASSERT_TRUE(recipient_cwt->public_key.has_value());

  // Encrypt the symmetric key with the public key of an intermediary.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  const EVP_HPKE_KDF* kdf = EVP_hpke_hkdf_sha256();
  const EVP_HPKE_AEAD* aead = EVP_hpke_aes_128_gcm();
  std::string intermediary_public_key;
  bssl::ScopedEVP_HPKE_KEY intermediary_key;
  GenerateKeyPair(*kem, intermediary_public_key, intermediary_key);
  absl::StatusOr<std::string> intermediary_cwt_bytes = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .curve = crypto_internal::kX25519,
          .x = intermediary_public_key,
      }}.Encode();
  ASSERT_OK(intermediary_cwt_bytes);
  absl::StatusOr<EncryptMessageResult> encrypt_result = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result);

  // Have the intermediary rewrap the symmetric key with the public key of the
  // final recipient.
  absl::StatusOr<std::string> symmetric_key =
      crypto_internal::UnwrapSymmetricKey(
          intermediary_key.get(), kdf, aead,
          encrypt_result->encrypted_symmetric_key, encrypt_result->encapped_key,
          message_associated_data);
  ASSERT_OK(symmetric_key);

  std::string symmetric_key_associated_data =
      "rewrap symmetric key associated data";
  absl::StatusOr<crypto_internal::WrapSymmetricKeyResult>
      rewrapped_symmetric_key_result = crypto_internal::WrapSymmetricKey(
          kem, kdf, aead, *symmetric_key, recipient_cwt->public_key->x,
          symmetric_key_associated_data);
  ASSERT_OK(rewrapped_symmetric_key_result);

  // The final recipient should be able to decrypt the message using the
  // rewrapped key.
  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, message_associated_data,
                        rewrapped_symmetric_key_result->encrypted_symmetric_key,
                        symmetric_key_associated_data,
                        rewrapped_symmetric_key_result->encapped_key);
  ASSERT_OK(decrypt_result);
  EXPECT_EQ(*decrypt_result, message);

  // The symmetric key should be an encoded CoseKey.
  absl::StatusOr<SymmetricKey> decoded_symmetric_key =
      SymmetricKey::Decode(*symmetric_key);
  ASSERT_OK(decoded_symmetric_key);
  EXPECT_EQ(decoded_symmetric_key->algorithm,
            crypto_internal::kAeadAes128GcmSivFixedNonce);
  EXPECT_NE(decoded_symmetric_key->k, "");
}

TEST(CryptoTest, EncryptAndDecryptWithProvidedKey) {
  auto [raw_public_key, raw_private_key] =
      GenerateKeyPair(*EVP_hpke_x25519_hkdf_sha256());
  absl::StatusOr<std::string> public_cwt = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = raw_public_key,
      }}.Encode();
  ASSERT_OK(public_cwt);
  absl::StatusOr<std::string> private_key =
      OkpKey{
          .key_id = "key-id",
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpDecrypt},
          .curve = crypto_internal::kX25519,
          .d = raw_private_key,
      }
          .Encode();
  ASSERT_OK(private_key);

  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor({}, {*private_key});

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *public_cwt, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, associated_data,
      encrypt_result->encrypted_symmetric_key, associated_data,
      encrypt_result->encapped_key, "key-id");
  ASSERT_OK(decrypt_result);
  EXPECT_EQ(*decrypt_result, message);
}

TEST(CryptoTest, EncryptAndDecryptCanIgnoreProvidedKey) {
  auto [raw_public_key, raw_private_key] =
      GenerateKeyPair(*EVP_hpke_x25519_hkdf_sha256());
  absl::StatusOr<std::string> private_key =
      OkpKey{
          .key_id = "key-id",
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpDecrypt},
          .curve = crypto_internal::kX25519,
          .d = raw_private_key,
      }
          .Encode();
  ASSERT_OK(private_key);

  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor({}, {*private_key});

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  // Even though decryption keys were provided, it should still be possible to
  // decrypt using the MessageDecryptor's internal key.
  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, associated_data,
      encrypt_result->encrypted_symmetric_key, associated_data,
      encrypt_result->encapped_key, "key-id");
  ASSERT_OK(decrypt_result);
  EXPECT_EQ(*decrypt_result, message);
}

TEST(CryptoTest, EncryptWithCoseKey) {
  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  // Extract the COSE_Key from the recipient's public key. The MessageEncryptor
  // should support that format as well.
  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(cwt);
  ASSERT_TRUE(cwt->public_key);
  absl::StatusOr<std::string> cose_key = cwt->public_key->Encode();
  ASSERT_OK(cose_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cose_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, associated_data,
                        encrypt_result->encrypted_symmetric_key,
                        associated_data, encrypt_result->encapped_key);
  ASSERT_OK(decrypt_result);
  EXPECT_EQ(*decrypt_result, message);
}

TEST(CryptoTest, EncryptForRelease) {
  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);
  // MessageDecryptor::GetPublicKey doesn't set the key_id field, so set it
  // manually.
  absl::StatusOr<OkpCwt> recipient_cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(recipient_cwt);
  ASSERT_TRUE(recipient_cwt->public_key.has_value());
  recipient_cwt->public_key->key_id = "key-id";
  recipient_public_key = recipient_cwt->Encode();
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.EncryptForRelease(
          message, *recipient_public_key, associated_data, "src-state",
          "dst-state", [](absl::string_view) { return "signature"; });
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, associated_data,
                        encrypt_result->encrypted_symmetric_key,
                        associated_data, encrypt_result->encapped_key);
  ASSERT_OK(decrypt_result);
  ASSERT_EQ(*decrypt_result, message);

  absl::StatusOr<ReleaseToken> release_token =
      ReleaseToken::Decode(encrypt_result->release_token);
  ASSERT_OK(release_token);
  EXPECT_THAT(release_token->signing_algorithm, Optional(Not(0)));
  EXPECT_THAT(release_token->encryption_algorithm, Optional(Not(0)));
  EXPECT_EQ(release_token->encryption_key_id, "key-id");
  EXPECT_EQ(release_token->src_state, "src-state");
  EXPECT_EQ(release_token->dst_state, "dst-state");
  EXPECT_THAT(release_token->encrypted_payload, Not(IsEmpty()));
  EXPECT_THAT(release_token->encapped_key, Optional(Not(IsEmpty())));
  EXPECT_EQ(release_token->signature, "signature");
}

TEST(CryptoTest, EncryptForReleaseWithNullSrcState) {
  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.EncryptForRelease(message, *recipient_public_key,
                                  associated_data, std::nullopt, "dst-state",
                                  [](absl::string_view) { return ""; });
  ASSERT_OK(encrypt_result);

  absl::StatusOr<ReleaseToken> release_token =
      ReleaseToken::Decode(encrypt_result->release_token);
  ASSERT_OK(release_token);
  EXPECT_THAT(release_token->src_state, Optional(std::nullopt));
}

TEST(CryptoTest, EncryptWithInvalidPublicKeyFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, "invalid public key", associated_data);
  ASSERT_THAT(encrypt_result, IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptWithInvalidCwtFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, std::string(50, 'x'), associated_data);
  EXPECT_THAT(encrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptWithMissingCwtPublicKeyFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  absl::StatusOr<std::string> cwt_bytes = OkpCwt().Encode();
  ASSERT_OK(cwt_bytes);

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cwt_bytes, associated_data);
  EXPECT_THAT(encrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptWithInvalidCwtAlgorithmFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  absl::StatusOr<std::string> public_key =
      MessageDecryptor().GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(public_key);
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(*public_key);
  ASSERT_OK(cwt);
  cwt->public_key->algorithm = crypto_internal::kAeadAes128GcmSivFixedNonce;
  absl::StatusOr<std::string> cwt_bytes = cwt->Encode();
  ASSERT_OK(cwt_bytes);

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cwt_bytes, associated_data);
  EXPECT_THAT(encrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptWithIncorrectKeyOpFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  absl::StatusOr<std::string> public_key =
      MessageDecryptor().GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(public_key);
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(*public_key);
  ASSERT_OK(cwt);
  cwt->public_key->key_ops = {kCoseKeyOpDecrypt};
  absl::StatusOr<std::string> cwt_bytes = cwt->Encode();
  ASSERT_OK(cwt_bytes);

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cwt_bytes, associated_data);
  EXPECT_THAT(encrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptWithoutKeyOpsSucceeds) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  absl::StatusOr<std::string> public_key =
      MessageDecryptor().GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(public_key);
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(*public_key);
  ASSERT_OK(cwt);
  cwt->public_key->key_ops.clear();
  absl::StatusOr<std::string> cwt_bytes = cwt->Encode();
  ASSERT_OK(cwt_bytes);

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cwt_bytes, associated_data);
  EXPECT_OK(encrypt_result);
}

TEST(CryptoTest, EncryptWithInvalidCwtCurveFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  absl::StatusOr<std::string> public_key =
      MessageDecryptor().GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(public_key);
  absl::StatusOr<OkpCwt> cwt = OkpCwt::Decode(*public_key);
  ASSERT_OK(cwt);
  cwt->public_key->curve = 0;  // 0 is a reserved value.
  absl::StatusOr<std::string> cwt_bytes = cwt->Encode();
  ASSERT_OK(cwt_bytes);

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cwt_bytes, associated_data);
  EXPECT_THAT(encrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptWithInvalidCwtPublicKeyFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  OkpCwt cwt{
      .public_key =
          OkpKey{
              .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
              .curve = crypto_internal::kX25519,
              .x = "invalid public key",
          },
  };
  absl::StatusOr<std::string> cwt_bytes = cwt.Encode();
  ASSERT_OK(cwt_bytes);

  MessageEncryptor encryptor;
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *cwt_bytes, associated_data);
  EXPECT_THAT(encrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, EncryptTwiceWithSameKeyUsesDifferentSymmetricKey) {
  std::string message = "some plaintext message";
  std::string message_associated_data = "plaintext associated data";

  MessageEncryptor encryptor;

  // Encrypt the symmetric key with the public key of an intermediary.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  const EVP_HPKE_KDF* kdf = EVP_hpke_hkdf_sha256();
  const EVP_HPKE_AEAD* aead = EVP_hpke_aes_128_gcm();
  std::string intermediary_public_key;
  bssl::ScopedEVP_HPKE_KEY intermediary_key;
  GenerateKeyPair(*kem, intermediary_public_key, intermediary_key);
  absl::StatusOr<std::string> intermediary_cwt_bytes = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .curve = crypto_internal::kX25519,
          .x = intermediary_public_key,
      }}.Encode();
  ASSERT_OK(intermediary_cwt_bytes);
  absl::StatusOr<EncryptMessageResult> encrypt_result_1 = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result_1);

  // Encrypt the same plaintext a second time. This should produce a different
  // ciphertext as a new symmetric key will be generated.
  absl::StatusOr<EncryptMessageResult> encrypt_result_2 = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result_2);
  ASSERT_NE(encrypt_result_1->ciphertext, encrypt_result_2->ciphertext);

  // Unwrap the symmetric key for each EncryptMessageResult.
  absl::StatusOr<std::string> symmetric_key_1 =
      crypto_internal::UnwrapSymmetricKey(
          intermediary_key.get(), kdf, aead,
          encrypt_result_1->encrypted_symmetric_key,
          encrypt_result_1->encapped_key, message_associated_data);
  ASSERT_OK(symmetric_key_1);

  absl::StatusOr<std::string> symmetric_key_2 =
      crypto_internal::UnwrapSymmetricKey(
          intermediary_key.get(), kdf, aead,
          encrypt_result_2->encrypted_symmetric_key,
          encrypt_result_2->encapped_key, message_associated_data);
  ASSERT_OK(symmetric_key_2);

  // The symmetric keys should be different.
  ASSERT_NE(*symmetric_key_1, *symmetric_key_2);
}

TEST(CryptoTest, DecryptWithWrongKeyFails) {
  std::string message = "some plaintext message";
  std::string message_associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  // Encrypt the symmetric key with the public key of an intermediary.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  std::string intermediary_public_key;
  bssl::ScopedEVP_HPKE_KEY intermediary_key;
  GenerateKeyPair(*kem, intermediary_public_key, intermediary_key);
  absl::StatusOr<std::string> intermediary_cwt_bytes = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .curve = crypto_internal::kX25519,
          .x = intermediary_public_key,
      }}.Encode();
  ASSERT_OK(intermediary_cwt_bytes);
  absl::StatusOr<EncryptMessageResult> encrypt_result = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result);

  // Attempting to decrypt without the symmetric key being rewrapped with the
  // public key of the recipient should fail.
  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, message_associated_data,
                        encrypt_result->encrypted_symmetric_key,
                        message_associated_data, encrypt_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithWrongCiphertextAssociatedDataFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, "wrong ciphertext associated data",
      encrypt_result->encrypted_symmetric_key, associated_data,
      encrypt_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithWrongSymmetricKeyAssociatedDataFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, associated_data,
      encrypt_result->encrypted_symmetric_key,
      "wrong symmetric key associated data", encrypt_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithWrongEncappedKeyFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, associated_data,
      encrypt_result->encrypted_symmetric_key,
      "wrong symmetric key associated data", encrypt_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithInvalidCiphertextFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt("invalid ciphertext", associated_data,
                        encrypt_result->encrypted_symmetric_key,
                        associated_data, encrypt_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithInvalidSymmetricKeyFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, associated_data, "invalid symmetric key",
      associated_data, encrypt_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithInvalidEncappedKeyFails) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *recipient_public_key, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, associated_data,
                        encrypt_result->encrypted_symmetric_key,
                        associated_data, "invalid encapped key");
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithInvalidAlgorithmFails) {
  std::string message = "some plaintext message";
  std::string message_associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);
  absl::StatusOr<OkpCwt> recipient_cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(recipient_cwt);
  ASSERT_TRUE(recipient_cwt->public_key.has_value());

  // Encrypt the symmetric key with the public key of an intermediary.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  const EVP_HPKE_KDF* kdf = EVP_hpke_hkdf_sha256();
  const EVP_HPKE_AEAD* aead = EVP_hpke_aes_128_gcm();
  std::string intermediary_public_key;
  bssl::ScopedEVP_HPKE_KEY intermediary_key;
  GenerateKeyPair(*kem, intermediary_public_key, intermediary_key);
  absl::StatusOr<std::string> intermediary_cwt_bytes = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = intermediary_public_key,
      }}.Encode();
  ASSERT_OK(intermediary_cwt_bytes);
  absl::StatusOr<EncryptMessageResult> encrypt_result = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result);

  // Have the intermediary rewrap the symmetric key with the public key of the
  // final recipient -- after changing the key's algorithm.
  absl::StatusOr<std::string> symmetric_key =
      crypto_internal::UnwrapSymmetricKey(
          intermediary_key.get(), kdf, aead,
          encrypt_result->encrypted_symmetric_key, encrypt_result->encapped_key,
          message_associated_data);
  ASSERT_OK(symmetric_key);

  absl::StatusOr<SymmetricKey> decoded_symmetric_key =
      SymmetricKey::Decode(*symmetric_key);
  ASSERT_OK(decoded_symmetric_key);
  decoded_symmetric_key->algorithm =
      crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm;
  symmetric_key = decoded_symmetric_key->Encode();
  ASSERT_OK(symmetric_key);

  std::string symmetric_key_associated_data =
      "rewrap symmetric key associated data";
  absl::StatusOr<crypto_internal::WrapSymmetricKeyResult>
      rewrapped_symmetric_key_result = crypto_internal::WrapSymmetricKey(
          kem, kdf, aead, *symmetric_key, recipient_cwt->public_key->x,
          symmetric_key_associated_data);
  ASSERT_OK(rewrapped_symmetric_key_result);

  // Decryption should fail because the algorithm is unsupported.
  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, message_associated_data,
                        rewrapped_symmetric_key_result->encrypted_symmetric_key,
                        symmetric_key_associated_data,
                        rewrapped_symmetric_key_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithIncorrectKeyOpFails) {
  std::string message = "some plaintext message";
  std::string message_associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);
  absl::StatusOr<OkpCwt> recipient_cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(recipient_cwt);
  ASSERT_TRUE(recipient_cwt->public_key.has_value());

  // Encrypt the symmetric key with the public key of an intermediary.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  const EVP_HPKE_KDF* kdf = EVP_hpke_hkdf_sha256();
  const EVP_HPKE_AEAD* aead = EVP_hpke_aes_128_gcm();
  std::string intermediary_public_key;
  bssl::ScopedEVP_HPKE_KEY intermediary_key;
  GenerateKeyPair(*kem, intermediary_public_key, intermediary_key);
  absl::StatusOr<std::string> intermediary_cwt_bytes = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = intermediary_public_key,
      }}.Encode();
  ASSERT_OK(intermediary_cwt_bytes);
  absl::StatusOr<EncryptMessageResult> encrypt_result = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result);

  // Have the intermediary rewrap the symmetric key with the public key of the
  // final recipient -- after changing the key_ops.
  absl::StatusOr<std::string> symmetric_key =
      crypto_internal::UnwrapSymmetricKey(
          intermediary_key.get(), kdf, aead,
          encrypt_result->encrypted_symmetric_key, encrypt_result->encapped_key,
          message_associated_data);
  ASSERT_OK(symmetric_key);

  absl::StatusOr<SymmetricKey> decoded_symmetric_key =
      SymmetricKey::Decode(*symmetric_key);
  ASSERT_OK(decoded_symmetric_key);
  decoded_symmetric_key->key_ops = {kCoseKeyOpEncrypt};
  symmetric_key = decoded_symmetric_key->Encode();
  ASSERT_OK(symmetric_key);

  std::string symmetric_key_associated_data =
      "rewrap symmetric key associated data";
  absl::StatusOr<crypto_internal::WrapSymmetricKeyResult>
      rewrapped_symmetric_key_result = crypto_internal::WrapSymmetricKey(
          kem, kdf, aead, *symmetric_key, recipient_cwt->public_key->x,
          symmetric_key_associated_data);
  ASSERT_OK(rewrapped_symmetric_key_result);

  // Decryption should fail because the "decrypt" key_op isn't present.
  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, message_associated_data,
                        rewrapped_symmetric_key_result->encrypted_symmetric_key,
                        symmetric_key_associated_data,
                        rewrapped_symmetric_key_result->encapped_key);
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptWithoutKeyOpsSucceeds) {
  std::string message = "some plaintext message";
  std::string message_associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor;

  absl::StatusOr<std::string> recipient_public_key =
      decryptor.GetPublicKey([](absl::string_view) { return ""; }, 0);
  ASSERT_OK(recipient_public_key);
  absl::StatusOr<OkpCwt> recipient_cwt = OkpCwt::Decode(*recipient_public_key);
  ASSERT_OK(recipient_cwt);
  ASSERT_TRUE(recipient_cwt->public_key.has_value());

  // Encrypt the symmetric key with the public key of an intermediary.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  const EVP_HPKE_KDF* kdf = EVP_hpke_hkdf_sha256();
  const EVP_HPKE_AEAD* aead = EVP_hpke_aes_128_gcm();
  std::string intermediary_public_key;
  bssl::ScopedEVP_HPKE_KEY intermediary_key;
  GenerateKeyPair(*kem, intermediary_public_key, intermediary_key);
  absl::StatusOr<std::string> intermediary_cwt_bytes = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = intermediary_public_key,
      }}.Encode();
  ASSERT_OK(intermediary_cwt_bytes);
  absl::StatusOr<EncryptMessageResult> encrypt_result = encryptor.Encrypt(
      message, *intermediary_cwt_bytes, message_associated_data);
  ASSERT_OK(encrypt_result);

  // Have the intermediary rewrap the symmetric key with the public key of the
  // final recipient -- after clearing the key_ops.
  absl::StatusOr<std::string> symmetric_key =
      crypto_internal::UnwrapSymmetricKey(
          intermediary_key.get(), kdf, aead,
          encrypt_result->encrypted_symmetric_key, encrypt_result->encapped_key,
          message_associated_data);
  ASSERT_OK(symmetric_key);

  absl::StatusOr<SymmetricKey> decoded_symmetric_key =
      SymmetricKey::Decode(*symmetric_key);
  ASSERT_OK(decoded_symmetric_key);
  decoded_symmetric_key->key_ops.clear();
  symmetric_key = decoded_symmetric_key->Encode();
  ASSERT_OK(symmetric_key);

  std::string symmetric_key_associated_data =
      "rewrap symmetric key associated data";
  absl::StatusOr<crypto_internal::WrapSymmetricKeyResult>
      rewrapped_symmetric_key_result = crypto_internal::WrapSymmetricKey(
          kem, kdf, aead, *symmetric_key, recipient_cwt->public_key->x,
          symmetric_key_associated_data);
  ASSERT_OK(rewrapped_symmetric_key_result);

  absl::StatusOr<std::string> decrypt_result =
      decryptor.Decrypt(encrypt_result->ciphertext, message_associated_data,
                        rewrapped_symmetric_key_result->encrypted_symmetric_key,
                        symmetric_key_associated_data,
                        rewrapped_symmetric_key_result->encapped_key);
  ASSERT_OK(decrypt_result);
}

TEST(CryptoTest, DecryptWithWrongKeyIdFails) {
  auto [raw_public_key, raw_private_key] =
      GenerateKeyPair(*EVP_hpke_x25519_hkdf_sha256());
  absl::StatusOr<std::string> public_cwt = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .curve = crypto_internal::kX25519,
          .x = raw_public_key,
      }}.Encode();
  ASSERT_OK(public_cwt);
  absl::StatusOr<std::string> private_key =
      OkpKey{
          .key_id = "key-id",
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .curve = crypto_internal::kX25519,
          .d = raw_private_key,
      }
          .Encode();
  ASSERT_OK(private_key);

  std::string message = "some plaintext message";
  std::string associated_data = "plaintext associated data";

  MessageEncryptor encryptor;
  MessageDecryptor decryptor({}, {*private_key});

  absl::StatusOr<EncryptMessageResult> encrypt_result =
      encryptor.Encrypt(message, *public_cwt, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result = decryptor.Decrypt(
      encrypt_result->ciphertext, associated_data,
      encrypt_result->encrypted_symmetric_key, associated_data,
      encrypt_result->encapped_key, "other-id");
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(CryptoTest, DecryptReleasedResult) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  // Encrypt the symmetric key with a known public key.
  const EVP_HPKE_KEM* kem = EVP_hpke_x25519_hkdf_sha256();
  const EVP_HPKE_KDF* kdf = EVP_hpke_hkdf_sha256();
  const EVP_HPKE_AEAD* aead = EVP_hpke_aes_128_gcm();
  std::string public_key;
  bssl::ScopedEVP_HPKE_KEY private_key;
  GenerateKeyPair(*kem, public_key, private_key);
  absl::StatusOr<std::string> cwt = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = public_key,
      }}.Encode();
  ASSERT_OK(cwt);
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      MessageEncryptor().Encrypt(message, *cwt, associated_data);
  ASSERT_OK(encrypt_result);

  // Unwrap the symmetric key.
  absl::StatusOr<std::string> symmetric_key =
      crypto_internal::UnwrapSymmetricKey(
          private_key.get(), kdf, aead, encrypt_result->encrypted_symmetric_key,
          encrypt_result->encapped_key, associated_data);
  ASSERT_OK(symmetric_key);

  // The message should be decryptable using the symmetric key.
  absl::StatusOr<std::string> decrypt_result =
      MessageDecryptor().DecryptReleasedResult(encrypt_result->ciphertext,
                                               associated_data, *symmetric_key);
  ASSERT_OK(decrypt_result);
  EXPECT_EQ(*decrypt_result, message);
}

TEST(CryptoTest, DecryptReleasedFailsWithInvalidSymmetricKey) {
  std::string message = "some plaintext message";
  std::string associated_data = "associated data";

  // Encrypt the symmetric key with a known public key.
  std::string public_key;
  bssl::ScopedEVP_HPKE_KEY private_key;
  GenerateKeyPair(*EVP_hpke_x25519_hkdf_sha256(), public_key, private_key);
  absl::StatusOr<std::string> cwt = OkpCwt{
      .public_key = OkpKey{
          .algorithm = crypto_internal::kHpkeBaseX25519Sha256Aes128Gcm,
          .key_ops = {kCoseKeyOpEncrypt},
          .curve = crypto_internal::kX25519,
          .x = public_key,
      }}.Encode();
  ASSERT_OK(cwt);
  absl::StatusOr<EncryptMessageResult> encrypt_result =
      MessageEncryptor().Encrypt(message, *cwt, associated_data);
  ASSERT_OK(encrypt_result);

  absl::StatusOr<std::string> decrypt_result =
      MessageDecryptor().DecryptReleasedResult(encrypt_result->ciphertext,
                                               associated_data, "invalid");
  EXPECT_THAT(decrypt_result, fcp::IsCode(INVALID_ARGUMENT));
}

TEST(EcdsaP256R1SignatureVerifierTest, VerifierWithInvalidPublicKeyFails) {
  // Verify a real signature with a bogus public key, which should fail.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create("not a valid public key");
  EXPECT_THAT(verifier, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(verifier.status().message(),
              HasSubstr("Failed to initialize public key"));
}

TEST(EcdsaP256R1SignatureVerifierTest, VerifierWithEmptyPublicKeyFails) {
  // Attempt to create a verifier with an empty public key, which should fail.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(/*public_key=*/"");
  EXPECT_THAT(verifier, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(verifier.status().message(),
              HasSubstr("Failed to initialize public key"));
}

TEST(EcdsaP256R1SignatureVerifierTest, ValidSignatureVerifiesSuccessfully) {
  absl::string_view data_to_sign = "some string to be signed";

  // Generate a signature for the data.
  EcdsaP256R1Signer signer = EcdsaP256R1Signer::Create();
  std::string signature = signer.Sign(data_to_sign);

  // Verify the signature.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(signer.GetPublicKey());
  ASSERT_OK(verifier);
  auto result = verifier->Verify(data_to_sign, signature);
  ASSERT_OK(result);
}

// Ensures that an empty data string can still be signed successfully, and
// doesn't cause a crash or error.
TEST(EcdsaP256R1SignatureVerifierTest,
     EmptyDataValidSignatureVerifiesSuccessfully) {
  absl::string_view empty_string = "";

  // Generate a signature for the data.
  EcdsaP256R1Signer signer = EcdsaP256R1Signer::Create();
  std::string signature = signer.Sign(empty_string);

  // Verify the signature.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(signer.GetPublicKey());
  ASSERT_OK(verifier);
  auto result = verifier->Verify(empty_string, signature);
  ASSERT_OK(result);
}

TEST(EcdsaP256R1SignatureVerifierTest, InvalidSignatureFailsVerification) {
  absl::string_view data_to_sign = "some string to be signed";

  // Create a signer, so we can get a valid public key.
  EcdsaP256R1Signer signer = EcdsaP256R1Signer::Create();

  // Verify a bogus signature not generated with that public key, which should
  // fail.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(signer.GetPublicKey());
  ASSERT_OK(verifier);
  auto result = verifier->Verify(data_to_sign, "not a valid signature");
  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(result.message(), HasSubstr("Invalid signature"));
}

TEST(EcdsaP256R1SignatureVerifierTest, EmptySignatureFailsVerification) {
  absl::string_view data_to_sign = "some string to be signed";

  // Create a signer, so we can get a valid public key.
  EcdsaP256R1Signer signer = EcdsaP256R1Signer::Create();

  // Verify with an empty signature, which should fail.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(signer.GetPublicKey());
  ASSERT_OK(verifier);
  auto result = verifier->Verify(data_to_sign, "");
  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(result.message(), HasSubstr("Invalid signature"));
}

TEST(EcdsaP256R1SignatureVerifierTest, MismatchingPublicKeyFailsVerification) {
  absl::string_view data_to_sign = "some string to be signed";

  // Generate a signature for the data.
  EcdsaP256R1Signer signer = EcdsaP256R1Signer::Create();
  std::string signature = signer.Sign(data_to_sign);

  // Create a second signer, which will have a different public key.
  EcdsaP256R1Signer second_signer = EcdsaP256R1Signer::Create();

  // Verify a signature from the first signer with the second signer's public
  // key, which should fail.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(second_signer.GetPublicKey());
  ASSERT_OK(verifier);
  auto result = verifier->Verify(data_to_sign, signature);
  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(result.message(), HasSubstr("Invalid signature"));
}

TEST(EcdsaP256R1SignatureVerifierTest,
     EmptyDataAndSignatureAndValidKeyFailsVerification) {
  EcdsaP256R1Signer signer = EcdsaP256R1Signer::Create();
  // Verify an empty data string with empty signature, which should fail.
  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(signer.GetPublicKey());
  ASSERT_OK(verifier);
  auto result = verifier->Verify("", "");
  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
}

// Tests that the signature verifier can successfully verify a known example of
// a valid <public key,signature,data> triple.
TEST(EcdsaP256R1SignatureVerifierTest, VerifyReferenceExample) {
  std::string public_key = absl::HexStringToBytes(
      "0429207056114055f94e46b14e55e6e3f1f088bea3bbc8c7d3cc140161551d42b1397395"
      "1c88e7638d800395191db1fa12515a174235cc291caa189eb2b4b4de19");

  std::string signature = absl::HexStringToBytes(
      "43675a6d2f2c2dfab5ab0497030ac63bafb9b9c6f09bcae8265e49543e8888cddc023453"
      "9a1f54fee3cb0781255c1c8c07c5d769095a3d1bd08d1ab57185b582");

  std::string data = absl::HexStringToBytes(
      "846a5369676e61747572653143a10126405848a3041a65fcb9b6061a65f37f363a000100"
      "005834a501010244b9d87c09033a000100002004215820bf33767a49e111cfcc9c1c8400"
      "a40e8c6205073d76794416ff13066bd08a6568");

  absl::StatusOr<EcdsaP256R1SignatureVerifier> verifier =
      EcdsaP256R1SignatureVerifier::Create(public_key);
  ASSERT_OK(verifier);
  ASSERT_OK(verifier->Verify(data, signature));
}

}  // namespace
}  // namespace confidential_compute
}  // namespace fcp
