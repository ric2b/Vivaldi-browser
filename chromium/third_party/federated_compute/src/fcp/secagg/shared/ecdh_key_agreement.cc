/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/secagg/shared/ecdh_key_agreement.h"

#include <memory>
#include <string>

#include "fcp/base/monitoring.h"
#include "fcp/secagg/shared/aes_key.h"
#include "fcp/secagg/shared/ecdh_keys.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/evp.h"
#include "openssl/mem.h"
#include "openssl/sha.h"

namespace fcp {
namespace secagg {

EcdhKeyAgreement::EcdhKeyAgreement() : key_(nullptr, EC_KEY_free) {}

EcdhKeyAgreement::EcdhKeyAgreement(EC_KEY* key) : key_(key, EC_KEY_free) {}

StatusOr<std::unique_ptr<EcdhKeyAgreement>>
EcdhKeyAgreement::CreateFromRandomKeys() {
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  EC_KEY_generate_key(key);
  if (EC_KEY_check_key(key)) {
    return std::make_unique<EcdhKeyAgreement>(key);
  } else {
    return FCP_STATUS(INTERNAL);
  }
}

StatusOr<std::unique_ptr<EcdhKeyAgreement>>
EcdhKeyAgreement::CreateFromPrivateKey(const EcdhPrivateKey& private_key) {
  if (private_key.size() != EcdhPrivateKey::kSize) {
    return FCP_STATUS(INVALID_ARGUMENT)
           << "Private key must be of length " << EcdhPrivateKey::kSize;
  }
  // Wrap a raw pointer in a unique_ptr with a deleter to guarantee deletion.
  BIGNUM* private_key_bn_raw =
      BN_bin2bn(private_key.data(), private_key.size(), nullptr);
  std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> private_key_bn(private_key_bn_raw,
                                                            BN_free);
  EC_KEY* key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (!EC_KEY_set_private_key(key, private_key_bn.get())) {
    return FCP_STATUS(INVALID_ARGUMENT) << "Invalid private key.";
  }
  return std::make_unique<EcdhKeyAgreement>(key);
}

StatusOr<std::unique_ptr<EcdhKeyAgreement>> EcdhKeyAgreement::CreateFromKeypair(
    const EcdhPrivateKey& private_key, const EcdhPublicKey& public_key) {
  if (public_key.size() != EcdhPublicKey::kSize &&
      public_key.size() != EcdhPublicKey::kUncompressedSize) {
    return FCP_STATUS(INVALID_ARGUMENT)
           << "Public key must be of length " << EcdhPublicKey::kSize << " or "
           << EcdhPublicKey::kUncompressedSize;
  }

  // Create using a private key only first, then add the public key if it
  // is valid.
  auto ecdh = CreateFromPrivateKey(private_key);
  if (!ecdh.ok()) {
    return ecdh;
  }

  // Copy this raw pointer for simplicity.
  EC_KEY* key_ptr = ecdh.value()->key_.get();

  // Wrap a raw pointer in a unique_ptr with a deleter to guarantee deletion.
  EC_POINT* public_key_point_raw = EC_POINT_new(EC_KEY_get0_group(key_ptr));
  std::unique_ptr<EC_POINT, void (*)(EC_POINT*)> public_key_point(
      public_key_point_raw, EC_POINT_free);

  if (!EC_POINT_oct2point(EC_KEY_get0_group(key_ptr), public_key_point.get(),
                          public_key.data(), public_key.size(), nullptr)) {
    return FCP_STATUS(INVALID_ARGUMENT) << "Invalid public key.";
  }

  // This makes a copy of the public key, so deletion is safe.
  if (!EC_KEY_set_public_key(key_ptr, public_key_point.get())) {
    return FCP_STATUS(INVALID_ARGUMENT) << "Invalid public key.";
  }

  if (EC_KEY_check_key(key_ptr)) {
    return ecdh;
  } else {
    return FCP_STATUS(INVALID_ARGUMENT) << "Invalid keypair.";
  }
}

EcdhPrivateKey EcdhKeyAgreement::PrivateKey() const {
  const BIGNUM* private_key_bn = EC_KEY_get0_private_key(key_.get());
  uint8_t private_key[EcdhPrivateKey::kSize];
  FCP_CHECK(
      BN_bn2bin_padded(private_key, EcdhPrivateKey::kSize, private_key_bn));
  return EcdhPrivateKey(private_key);
}

EcdhPublicKey EcdhKeyAgreement::PublicKey() const {
  const EC_POINT* public_key_point = EC_KEY_get0_public_key(key_.get());
  if (public_key_point == nullptr) {
    return EcdhPublicKey();
  }
  uint8_t public_key[EcdhPublicKey::kSize];
  int public_key_size = EC_POINT_point2oct(
      EC_KEY_get0_group(key_.get()), public_key_point,
      POINT_CONVERSION_COMPRESSED, public_key, EcdhPublicKey::kSize, nullptr);
  FCP_CHECK(public_key_size == EcdhPublicKey::kSize);
  return EcdhPublicKey(public_key);
}

StatusOr<AesKey> EcdhKeyAgreement::ComputeSharedSecret(
    const EcdhPublicKey& other_key) const {
  if (other_key.size() != EcdhPublicKey::kSize &&
      other_key.size() != EcdhPublicKey::kUncompressedSize) {
    return FCP_STATUS(INVALID_ARGUMENT)
           << "Public key must be of length " << EcdhPublicKey::kSize << " or "
           << EcdhPublicKey::kUncompressedSize;
  }
  // Wrap a raw pointer in a unique_ptr with a deleter to guarantee deletion.
  EC_POINT* other_point_raw = EC_POINT_new(EC_KEY_get0_group(key_.get()));
  std::unique_ptr<EC_POINT, void (*)(EC_POINT*)> other_point(other_point_raw,
                                                             EC_POINT_free);
  if (!EC_POINT_oct2point(EC_KEY_get0_group(key_.get()), other_point.get(),
                          other_key.data(), other_key.size(), nullptr)) {
    return FCP_STATUS(INVALID_ARGUMENT) << "Invalid ECDH public key.";
  }
  uint8_t secret[AesKey::kSize];
  ECDH_compute_key(secret, AesKey::kSize, other_point.get(), key_.get(),
                   nullptr);
  return AesKey(secret);
}

}  // namespace secagg
}  // namespace fcp
