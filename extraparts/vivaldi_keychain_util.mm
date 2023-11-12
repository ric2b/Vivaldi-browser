// Copyright (c) 2018-2020 Vivaldi Technologies AS. All rights reserved

#include "extraparts/vivaldi_keychain_util.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "components/os_crypt/sync/keychain_password_mac.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "crypto/apple_keychain.h"

namespace vivaldi {

namespace {
base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;
}

const std::string vivaldi_service_name = "Vivaldi Safe Storage";
const std::string vivaldi_account_name = "Vivaldi";
const crypto::AppleKeychain keychain_;

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251 but deprecation
// warnings are disabled in the meanwhile.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

OSStatus GetVivaldiKeychainStatus() {
  // Turn off the keychain user interaction
  SecKeychainSetUserInteractionAllowed(false);

  UInt32 viv_password_length = 0;
  void* viv_password_data = NULL;
  OSStatus error = keychain_.FindGenericPassword(vivaldi_service_name.size(),
                                                 vivaldi_service_name.data(),
                                                 vivaldi_account_name.size(),
                                                 vivaldi_account_name.data(),
                                                 &viv_password_length,
                                                 &viv_password_data,
                                                 NULL);

  // Turn keychain user interaction back on
  SecKeychainSetUserInteractionAllowed(true);

  return error;
}

#pragma clang diagnostic pop

bool HasKeychainAccess() {
  return GetVivaldiKeychainStatus() == errSecSuccess;
}

void MaybeSetupVivaldiKeychain() {
  if (first_run::IsChromeFirstRun()) {
    // if this is the first run of the browser we just let the normal keychain
    // path create the vivaldi keychain by requesting the password
    LOG(INFO) << "First run, create vivaldi keychain with random key if needed";
    base::AutoLock auto_lock(g_lock.Get());
    KeychainPassword encryptor_password(keychain_);
    encryptor_password.GetPassword();
    return;
  }

  OSStatus viv_error = GetVivaldiKeychainStatus();

  if (viv_error == errSecSuccess ||
      viv_error == errSecAuthFailed ||
      viv_error == errSecInteractionRequired) {
    LOG(INFO) << "Vivaldi keychain found, error code: " << viv_error;
    return;
  } else {
    LOG(INFO) << "Vivaldi keychain not found, error code: " << viv_error;
  }

  // check if chromium keychain exists
  std::string old_service_name = "Chrome Safe Storage";
  std::string old_account_name = "Chrome";
  UInt32 chr_password_length = 0;
  void* chr_password_data = NULL;
  OSStatus chr_error = keychain_.FindGenericPassword(old_service_name.size(),
                                                     old_service_name.data(),
                                                     old_account_name.size(),
                                                     old_account_name.data(),
                                                     &chr_password_length,
                                                     &chr_password_data,
                                                     NULL);

  LOG(INFO) << "Finished checking Chrome keychain";

  if (chr_error == errSecSuccess && viv_error == errSecItemNotFound) {
    // Chromium keychain exists and we have access but no Vivaldi keychain found
    // Create vivaldi keychain with password from chromium keychain
    LOG(INFO) << "Chrome keychain exists, adding Vivaldi with Chrome key";

    OSStatus error = keychain_.AddGenericPassword(vivaldi_service_name.size(),
                                                  vivaldi_service_name.data(),
                                                  vivaldi_account_name.size(),
                                                  vivaldi_account_name.data(),
                                                  chr_password_length,
                                                  chr_password_data,
                                                  NULL);

    if (error != noErr) {
      LOG(INFO) << "Failed adding vivaldi keychain";
      return;
    }
    LOG(INFO) << "Successfully added vivaldi keychain";
  }
  return;
}

} // namespace vivaldi
