// Copyright 2025 Vivaldi Technologies. All rights reserved.

#include "components/os_crypt/sync/os_crypt.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/os_crypt/sync/keychain_password_mac.h"
#include "components/os_crypt/sync/os_crypt_metrics.h"
#include "crypto/aes_cbc.h"
#include "crypto/apple_keychain.h"
#include "crypto/kdf.h"

namespace {

constexpr char kObfuscationPrefixV10[] = "v10";

constexpr std::array<uint8_t, crypto::aes_cbc::kBlockSize> kIv{
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};

bool import_key_is_cached = false;

base::LazyInstance<base::Lock>::Leaky importLock = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Same as DeriveKey(), but used by vivaldi during import of passwords.
// See chromium/components/os_crypt/sync/os_crypt_mac.mm
bool OSCryptImpl::DeriveImportEncryptionKey(const std::string& service_name,
                                            const std::string& account_name) {
  base::AutoLock auto_lock(importLock.Get());

  // Fast fail if this object is pretending to have a locked keychain.
  // TODO(https://crbug.com/389737048): Replace this with a setter on the mock
  // keychain once it's possible to inject a mock keychain.
  if (use_mock_keychain_ && use_locked_mock_keychain_) {
    return false;
  }

  if (import_key_is_cached)
    return true;

  std::string password;
  crypto::AppleKeychain keychain;
  KeychainPassword encryptor_password(keychain);
  password = encryptor_password.GetPassword(service_name, account_name);

  if (password.empty()) {
    return false;
  }

  // Subsequent code must guarantee that the correct key is cached before
  // returning.
  import_key_is_cached = true;

  static constexpr auto kSalt =
      std::to_array<uint8_t>({'s', 'a', 'l', 't', 'y', 's', 'a', 'l', 't'});
  static constexpr size_t kIterations = 1003;

  std::array<uint8_t, kDerivedKeySize> import_key;
  crypto::kdf::DeriveKeyPbkdf2HmacSha1({.iterations = kIterations},
                                       base::as_byte_span(password), kSalt,
                                       import_key, crypto::SubtlePassKey{});
  import_cached_encryption_key_ = import_key;
  DCHECK(import_cached_encryption_key_);
  return true;
}

// Same as DecryptString(...), but used by vivaldi during import of passwords.
// See chromium/components/os_crypt/sync/os_crypt_mac.mm
bool OSCryptImpl::DecryptImportedString16(const std::string& ciphertext,
                                          std::u16string* plaintext,
                                          const std::string& service_name,
                                          const std::string& account_name) {
  std::string utf8;
  if (ciphertext.empty()) {
    utf8 = std::string();
    *plaintext = base::UTF8ToUTF16(utf8);
    return true;
  }

  // Check that the incoming cyphertext was indeed encrypted with the expected
  // version.  If the prefix is not found then we'll assume we're dealing with
  // old data saved as clear text and we'll return it directly.
  // Credit card numbers are current legacy data, so false match with prefix
  // won't happen.
  const os_crypt::EncryptionPrefixVersion encryption_version =
      ciphertext.find(kObfuscationPrefixV10) == 0
          ? os_crypt::EncryptionPrefixVersion::kVersion10
          : os_crypt::EncryptionPrefixVersion::kNoVersion;

  if (encryption_version == os_crypt::EncryptionPrefixVersion::kNoVersion) {
    return false;
  }

  if (!DeriveImportEncryptionKey(service_name, account_name)) {
    // Can not read import encryption key, will not be able to decrypt
    VLOG(1) << "Decryption failed";
    return false;
  }

  // Strip off the versioning prefix before decrypting.
  base::span<const uint8_t> raw_ciphertext =
      base::as_byte_span(ciphertext).subspan(strlen(kObfuscationPrefixV10));

  std::optional<std::vector<uint8_t>> maybe_plain = crypto::aes_cbc::Decrypt(
      *import_cached_encryption_key_, kIv, base::as_byte_span(raw_ciphertext));

  if (!maybe_plain) {
    VLOG(1) << "Decryption failed";
    return false;
  }

  *plaintext = base::UTF8ToUTF16(base::as_string_view(*maybe_plain));
  return true;
}

void OSCryptImpl::ResetImportCache() {
  import_key_is_cached = false;
}

// End Vivaldi
