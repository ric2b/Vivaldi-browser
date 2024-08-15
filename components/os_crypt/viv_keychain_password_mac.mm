
#include "components/os_crypt/sync/keychain_password_mac.h"

#import <Security/Security.h>

#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/base64.h"
#include "base/rand_util.h"
#include "build/branding_buildflags.h"
#include "crypto/apple_keychain.h"

std::string KeychainPassword::GetPassword(
    const std::string& service_name,
    const std::string& account_name) const {
  // Vivaldi function to access the keychain for other apps, not Vivaldi Safe
  // Storage, e.g. for other chromium based browsers

  UInt32 password_length = 0;
  void* password_data = NULL;
  OSStatus error = keychain_->FindGenericPassword(service_name.size(),
                                                 service_name.data(),
                                                 account_name.size(),
                                                 account_name.data(),
                                                 &password_length,
                                                 &password_data,
                                                 NULL);

  if (error == noErr) {
    std::string password =
    std::string(static_cast<char*>(password_data), password_length);
    keychain_->ItemFreeContent(password_data);
    return password;
  } else if (error == errSecItemNotFound) {
    // The requested account has no passwords in keychain, we can stop
    return std::string();
  } else {
    OSSTATUS_DLOG(ERROR, error) << "Keychain lookup failed";
    return std::string();
  }
}
