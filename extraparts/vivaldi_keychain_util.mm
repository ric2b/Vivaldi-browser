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

} // namespace vivaldi
