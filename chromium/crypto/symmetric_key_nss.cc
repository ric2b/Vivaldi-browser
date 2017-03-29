// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/symmetric_key.h"

#include <nss.h>
#include <pk11pub.h>

#include "base/logging.h"
#include "crypto/nss_util.h"

namespace crypto {

SymmetricKey::~SymmetricKey() {}

// static
SymmetricKey* SymmetricKey::GenerateRandomKey(Algorithm algorithm,
                                              size_t key_size_in_bits) {
  DCHECK_LT(ENC_ALG_START, algorithm);
  DCHECK_GT(ENC_ALG_END, algorithm);

  EnsureNSSInit();

  // Whitelist supported key sizes to avoid accidentaly relying on
  // algorithms available in NSS but not BoringSSL and vice
  // versa. Note that BoringSSL does not support AES-192.
  if (algorithm == AES && key_size_in_bits != 128 && key_size_in_bits != 256)
    return NULL;

  ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (!slot.get())
    return NULL;

  CK_MECHANISM_TYPE alg = CKM_AES_KEY_GEN;
  if (algorithm != AES)
    switch (algorithm)
    {
    case DES_EDE3:
      alg = CKM_DES3_KEY_GEN;
	  break;
    default:
      return NULL;
    }

  PK11SymKey* sym_key = PK11_KeyGen(slot.get(), alg, NULL,
                                    key_size_in_bits / 8, NULL);
  if (!sym_key)
    return NULL;

  return new SymmetricKey(algorithm, sym_key);
}

// static
SymmetricKey* SymmetricKey::DeriveKeyFromPassword(Algorithm algorithm,
                                                  const std::string& password,
                                                  const std::string& salt,
                                                  size_t iterations,
                                                  size_t key_size_in_bits) {
  EnsureNSSInit();
  if (salt.empty() || iterations == 0 || key_size_in_bits == 0)
    return NULL;

  if (algorithm == AES) {
    // Whitelist supported key sizes to avoid accidentaly relying on
    // algorithms available in NSS but not BoringSSL and vice
    // versa. Note that BoringSSL does not support AES-192.
    if (key_size_in_bits != 128 && key_size_in_bits != 256)
      return NULL;
  }

  SECItem password_item;
  password_item.type = siBuffer;
  password_item.data = reinterpret_cast<unsigned char*>(
      const_cast<char *>(password.data()));
  password_item.len = password.size();

  SECItem salt_item;
  salt_item.type = siBuffer;
  salt_item.data = reinterpret_cast<unsigned char*>(
      const_cast<char *>(salt.data()));
  salt_item.len = salt.size();

  SECOidTag cipher_algorithm;
  SECOidTag prf_algorithm;
  switch (algorithm)
  {
  case AES:
    cipher_algorithm =  SEC_OID_AES_256_CBC;
    prf_algorithm = SEC_OID_HMAC_SHA1;
    break;
  case HMAC_SHA1:
    cipher_algorithm = SEC_OID_HMAC_SHA1;
    prf_algorithm = SEC_OID_HMAC_SHA1;
    break;
  case DES_EDE3:
    cipher_algorithm =  SEC_OID_DES_EDE3_CBC;
    prf_algorithm = SEC_OID_HMAC_SHA1;
    break;
  default:
    return NULL;
  }

  ScopedSECAlgorithmID alg_id(PK11_CreatePBEV2AlgorithmID(SEC_OID_PKCS5_PBKDF2,
                                                          cipher_algorithm,
                                                          prf_algorithm,
                                                          key_size_in_bits / 8,
                                                          iterations,
                                                          &salt_item));
  if (!alg_id.get())
    return NULL;

  ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (!slot.get())
    return NULL;

  PK11SymKey* sym_key = PK11_PBEKeyGen(slot.get(), alg_id.get(), &password_item,
                                       PR_FALSE, NULL);
  if (!sym_key)
    return NULL;

  return new SymmetricKey(algorithm, sym_key);
}

// static
SymmetricKey* SymmetricKey::Import(Algorithm algorithm,
                                   const std::string& raw_key) {
  return Import(algorithm,
	  reinterpret_cast<unsigned char*>(const_cast<char *>(raw_key.data())),
      raw_key.size());
}

// static
SymmetricKey* SymmetricKey::Import(Algorithm algorithm,
                                   const unsigned char *raw_key, 
                                   unsigned int raw_key_len) {
  EnsureNSSInit();
  CK_MECHANISM_TYPE cipher = CKM_SHA_1_HMAC;
  switch (algorithm)
  {
  case AES:
    // Whitelist supported key sizes to avoid accidentaly relying on
    // algorithms available in NSS but not BoringSSL and vice
    // versa. Note that BoringSSL does not support AES-192.
    if (raw_key_len != 128/8 && raw_key_len != 256/8)
      return NULL;
    cipher =  CKM_AES_CBC;
    break;
  case DES_EDE3:
    cipher =  CKM_DES3_CBC;
    break;
  case HMAC_SHA1:
	// cipher = CKM_SHA_1_HMAC;
    break;
  default:
    return NULL;
  }

  SECItem key_item;
  key_item.type = siBuffer;
  key_item.data = const_cast<unsigned char*>(raw_key);
  key_item.len = raw_key_len;

  ScopedPK11Slot slot(PK11_GetInternalSlot());
  if (!slot.get())
    return NULL;

  // The exact value of the |origin| argument doesn't matter to NSS as long as
  // it's not PK11_OriginFortezzaHack, so we pass PK11_OriginUnwrap as a
  // placeholder.
  PK11SymKey* sym_key = PK11_ImportSymKey(slot.get(), cipher, PK11_OriginUnwrap,
                                          CKA_ENCRYPT, &key_item, NULL);
  if (!sym_key)
    return NULL;

  return new SymmetricKey(algorithm, sym_key);
}

bool SymmetricKey::GetRawKey(std::string* raw_key) {
  SECStatus rv = PK11_ExtractKeyValue(key_.get());
  if (SECSuccess != rv)
    return false;

  SECItem* key_item = PK11_GetKeyData(key_.get());
  if (!key_item)
    return false;

  raw_key->assign(reinterpret_cast<char*>(key_item->data), key_item->len);
  return true;
}

SymmetricKey::SymmetricKey(Algorithm algorithm, PK11SymKey* key) : 
  algorithm_(algorithm), 
  key_(key) 
{
  DCHECK(key);
}

}  // namespace crypto
