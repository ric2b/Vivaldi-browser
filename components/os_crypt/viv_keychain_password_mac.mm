
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

  auto password = keychain_->FindGenericPassword(service_name, account_name);

  if (password.has_value()) {
    return std::string(base::as_string_view(*password));
  }

  if (password.error() != errSecItemNotFound) {
    OSSTATUS_DLOG(ERROR, password.error()) << "Keychain lookup failed";
  }

  // Either error was encountered OR
  // The requested account has no passwords in keychain, we can stop
  return std::string();
}
