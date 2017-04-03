//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace {
const wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
const wchar_t kUniqueUserValue[] = L"unique_user_id";

}  // anonymous namespace

namespace extensions {
bool UtilitiesGetUniqueUserIdFunction::ReadUserIdFromOSProfile(
    std::string* user_id) {
  base::win::RegKey key(HKEY_CURRENT_USER, kVivaldiKey, KEY_READ);

  base::string16 reg_user_id;
  if (!key.Valid() ||
      key.ReadValue(kUniqueUserValue, &reg_user_id) != ERROR_SUCCESS)
    return false;
  user_id->assign(base::UTF16ToUTF8(reg_user_id));
  return true;
}

void UtilitiesGetUniqueUserIdFunction::WriteUserIdToOSProfile(
    const std::string& user_id) {
  base::win::RegKey key(HKEY_CURRENT_USER, kVivaldiKey, KEY_WRITE);

  if (!key.Valid())
    return;
  key.WriteValue(kUniqueUserValue, base::UTF8ToUTF16(user_id).c_str());
}
}  // namespace extensions
