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

// This file contains utilities for encrypting and decrypting messages to be
// used by confidential compute machines executing federated computations.
//
// The messages are encrypted with AEAD using a per-message generated symmetric
// key and then the symmetric key is encrypted using HPKE with the public key of
// the intended message recipient.
//
// Separating the step of encrypting the symmetric key from encrypting the
// message allows the AEAD symmetric key to be passed off to another party to
// be used to decrypt, without sharing the private key between the two parties.
//
// This is a C++ port of the Rust encryption implementation in
// https://github.com/google-parfait/confidential-federated-compute/tree/main/cfc_crypto.

#ifndef FCP_CONFIDENTIALCOMPUTE_CRYPTO_H_
#define FCP_CONFIDENTIALCOMPUTE_CRYPTO_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "openssl/base.h"
#include "openssl/ec_key.h"  // // IWYU pragma: keep, needed for bssl::UniquePtr<EC_KEY>
#include "openssl/hpke.h"

namespace fcp {
namespace confidential_compute {

struct EncryptMessageResult {
  std::string ciphertext;
  std::string encapped_key;
  std::string encrypted_symmetric_key;
  std::string release_token;
};

// Encrypts messages for particular intended recipients.
//
// Returns an EncryptMessageResult containing the ciphertext, the encrypted
// symmetric key to be decrypted and used to decrypt the message, and the
// encapsulated key, which along with the recipient's private key, can be used
// for decrypting the encrypted symmetric key.
//
// IMPORTANT: This class DOES NOT validate the public key passed to Encrypt. If
// the public key is a CWT, it's the caller's responsibility to verify the
// signature and claims.
//
// This class is thread-safe.
class MessageEncryptor {
 public:
  MessageEncryptor();

  // Encrypts a message with the specified public key, which may be either a
  // serialized CWT or a serialized COSE_Key.
  absl::StatusOr<EncryptMessageResult> Encrypt(
      absl::string_view plaintext, absl::string_view recipient_public_key,
      absl::string_view associated_data) const;

  // Encrypts a message with the specified public key and generates a "release
  // token" that can be passed to the CFC KMS to release the decryption key.
  // Like with `Encrypt`, the public key may be either a serialized CWT or a
  // serialized COSE_Key.
  //
  // The KMS will only reveal the decryption key if the logical pipeline's state
  // can be updated from `src_state` to `dst_state`. See the KMS API docs in
  // ../protos/confidentialcompute/kms.proto.
  absl::StatusOr<EncryptMessageResult> EncryptForRelease(
      absl::string_view plaintext, absl::string_view recipient_public_key,
      absl::string_view associated_data,
      std::optional<absl::string_view> src_state, absl::string_view dst_state,
      absl::FunctionRef<absl::StatusOr<std::string>(absl::string_view)> signer)
      const;

 private:
  // Encrypts a message with the specified public key and optionally generates a
  // release token if `signing_key` is non-null.
  //
  // This function implements the common functionality for `Encrypt` and
  // `EncryptForRelease`.
  absl::StatusOr<EncryptMessageResult> EncryptInternal(
      absl::string_view plaintext, absl::string_view recipient_public_key,
      absl::string_view associated_data,
      std::optional<absl::string_view> src_state, absl::string_view dst_state,
      std::optional<
          absl::FunctionRef<absl::StatusOr<std::string>(absl::string_view)>>
          signer) const;

  const EVP_HPKE_KEM* hpke_kem_;
  const EVP_HPKE_KDF* hpke_kdf_;
  const EVP_HPKE_AEAD* hpke_aead_;
  const EVP_AEAD* aead_;
};

// Decrypts messages intended for this recipient.
//
// This class is thread-safe.
class MessageDecryptor {
 public:
  // Constructs a new MessageDecryptor. If set, the provided config_properties
  // will be included in the public key claims. The MessageDecryptor may be
  // provided with a list of decryption keys to use in addition to the
  // internally generated key; these keys should be encoded as serialized
  // COSE_Keys. Any invalid keys will be ignored.
  explicit MessageDecryptor(
      std::string config_properties = "",
      const std::vector<absl::string_view>& decryption_keys = {});

  // MessageDecryptor is not copyable or moveable due to the use of
  // bssl::ScopedEVP_HPKE_KEY.
  MessageDecryptor(const MessageDecryptor& other) = delete;
  MessageDecryptor& operator=(const MessageDecryptor& other) = delete;

  // Obtain a public key that can be used to encrypt messages that this class
  // can subsequently decrypt. The key will be a CBOR Web Token (CWT) signed by
  // the provided signing function.
  //
  // The key material is generated in the constructor so the same key material
  // will be included in the CWT even if this function is called multiple times.
  // The CWT returned by this function may differ between function calls if
  // different signing functions or algorithm identifiers are used, or if the
  // provided signing function is non-deterministic.
  absl::StatusOr<std::string> GetPublicKey(
      absl::FunctionRef<absl::StatusOr<std::string>(absl::string_view)> signer,
      int64_t signer_algorithm) const;

  // Decrypts `ciphertext` using a symmetric key produced by decrypting
  // `encrypted_symmetric_key` with the `encapped_key` and the private key
  // corresponding to the public key returned by `GetPublicKey`.
  //
  // The ciphertext to decrypt should have been produced by
  // `MessageEncryptor::Encrypt` or an equivalent implementation.
  //
  // `ciphertext_associated_data` and `encrypted_symmetric_key_associated_data`
  // may differ in the case that the symmetric key was rewrapped by an
  // intermediary for decryption by this recipient.
  //
  // Returns the decrypted plaintext, a FAILED_PRECONDITION status if
  // `GetPublicKey` has never been called for this class, or an INVALID_ARGUMENT
  // status if the ciphertext could not be decrypted with the provided
  // arguments.
  absl::StatusOr<std::string> Decrypt(
      absl::string_view ciphertext,
      absl::string_view ciphertext_associated_data,
      absl::string_view encrypted_symmetric_key,
      absl::string_view encrypted_symmetric_key_associated_data,
      absl::string_view encapped_key, absl::string_view key_id = "") const;

  // Decrypts `ciphertext` using a symmetric key returned by
  // `/KeyManagementService.ReleaseResults`.
  //
  // This effectively runs the second half of the `Decrypt` function above:
  // ReleaseResults handles the first half (unwrapping the symmetric key), and
  // this function handles decryption of the ciphertext using that key.
  //
  // The ciphertext to decrypt should have been produced by
  // `MessageEncryptor::Encrypt` or an equivalent implementation.
  absl::StatusOr<std::string> DecryptReleasedResult(
      absl::string_view ciphertext, absl::string_view associated_data,
      absl::string_view symmetric_key) const;

 private:
  // Attempts to unwraps the encrypted symmetric key using the decryption keys
  // provided in the constructor. Returns nullopt if decryption is not
  // successful.
  std::optional<std::string> UnwrapSymmetricKeyWithDecryptionKeys(
      absl::string_view encrypted_symmetric_key,
      absl::string_view encrypted_symmetric_key_associated_data,
      absl::string_view encapped_key, absl::string_view key_id) const;

  const std::string config_properties_;
  const absl::flat_hash_map<std::string, std::vector<bssl::ScopedEVP_HPKE_KEY>>
      decryption_keys_;
  const EVP_HPKE_KEM* hpke_kem_;
  const EVP_HPKE_KDF* hpke_kdf_;
  const EVP_HPKE_AEAD* hpke_aead_;
  bssl::ScopedEVP_HPKE_KEY hpke_key_;
  const EVP_AEAD* aead_;
};

// Calculates ECDSA signatures using the P-256 (a.k.a. secp256r1) curve, using
// SHA-256 digests.
class EcdsaP256R1Signer {
 public:
  // Returns an instance of this class after generating a new public/private key
  // pair, or an INTERNAL error.
  static EcdsaP256R1Signer Create();

  // Not copyable due to bssl::UniquePtr<EC_KEY> member field.
  EcdsaP256R1Signer(const EcdsaP256R1Signer& other) = delete;
  EcdsaP256R1Signer(EcdsaP256R1Signer&& other) = default;

  // Returns the public key that can be used to verify signatures generated with
  // the private key for this instance.
  //
  // The key will be an X9.62 (a.k.a. SEC1) encoded public key, in uncompressed
  // format.
  std::string GetPublicKey() const;

  // Signs `data` with this instance's private key. Note that `data` must be the
  // actual data, and not just a digest of the data. The function will calculate
  // the correct digest over the data.
  //
  // Returns a signature encoded as per RFC 8152 section 8.1, which can be
  // verified by `EcdsaP256R1SignatureVerifier::Verify` below.
  std::string Sign(absl::string_view data) const;

 private:
  EcdsaP256R1Signer(bssl::UniquePtr<EC_KEY> key,
                    std::string encoded_public_key);
  bssl::UniquePtr<EC_KEY> key_;
  std::string encoded_public_key_;
};

// Verifies ECDSA signatures using the P-256 (a.k.a. secp256r1) curve, using
// SHA-256 digests.
class EcdsaP256R1SignatureVerifier {
 public:
  // Creates a new verifier for the given X9.62 (a.k.a. SEC1) octet-encoded
  // public key.
  //
  // Returns an INVALID_ARGUMENT error if the key is invalid.
  static absl::StatusOr<EcdsaP256R1SignatureVerifier> Create(
      absl::string_view public_key);

  // Not copyable due to bssl::UniquePtr<EC_KEY> member field.
  EcdsaP256R1SignatureVerifier(const EcdsaP256R1SignatureVerifier& other) =
      delete;
  EcdsaP256R1SignatureVerifier(EcdsaP256R1SignatureVerifier&& other) = default;

  // Verifies whether that `signature` constitutes a valid signature `data` by
  // this instance's public key.
  //
  // - `data` should be the actual data to sign, not a digest of the data, since
  //   the function will calculate the digest over the data itself.
  // - `signature` should contain a signature encoded as per RFC 8152
  //   section 8.1. I.e. a 64 byte long signature, consisting of the
  //   concatenation of the R and S components encoded as big endian integers of
  //   32 bytes long (with zero padding as needed).
  absl::Status Verify(absl::string_view data,
                      absl::string_view signature) const;

 private:
  EcdsaP256R1SignatureVerifier(bssl::UniquePtr<EC_KEY> public_key);
  bssl::UniquePtr<EC_KEY> public_key_;
};

// Helper functions exposed for testing purposes.
namespace crypto_internal {

// Supported COSE Algorithms; see ../protos/confidentialcompute/cbor_ids.md.
enum CoseAlgorithm {
  kEs256 = -7,
  kHpkeBaseX25519Sha256Aes128Gcm = -65537,
  kAeadAes128GcmSivFixedNonce = -65538,
};

// Supported COSE Elliptic Curves; see
// https://www.iana.org/assignments/cose/cose.xhtml#elliptic-curves.
enum CoseEllipticCurve {
  kX25519 = 4,
};

struct WrapSymmetricKeyResult {
  std::string encapped_key;
  std::string encrypted_symmetric_key;
};

// Wraps a symmetric encryption key using HPKE.
absl::StatusOr<WrapSymmetricKeyResult> WrapSymmetricKey(
    const EVP_HPKE_KEM* hpke_kem, const EVP_HPKE_KDF* hpke_kdf,
    const EVP_HPKE_AEAD* hpke_aead, absl::string_view symmetric_key,
    absl::string_view recipient_public_key, absl::string_view associated_data);

// Unwraps a symmetric encryption key using HPKE.
absl::StatusOr<std::string> UnwrapSymmetricKey(
    const EVP_HPKE_KEY* hpke_key, const EVP_HPKE_KDF* hpke_kdf,
    const EVP_HPKE_AEAD* hpke_aead, absl::string_view encrypted_symmetric_key,
    absl::string_view encapped_key, absl::string_view associated_data);

}  // namespace crypto_internal

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_CRYPTO_H_
