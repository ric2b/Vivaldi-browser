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

bool UtilitiesGetSystemDateFormatFunction::ReadDateFormats(
    vivaldi::utilities::DateFormats* date_formats) {
  // According to MSDN documentation max len is 80
  // https://msdn.microsoft.com/en-us/library/windows/desktop/
  //   dd373896(v=vs.85).aspx
  wchar_t result_buffer[80];
  int len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT,
                            result_buffer, arraysize(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string timeformat = base::UTF16ToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE,
                        result_buffer, arraysize(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string shortformat = base::UTF16ToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SLONGDATE,
                        result_buffer, arraysize(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string longdateformat = base::UTF16ToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK,
                        result_buffer, arraysize(result_buffer));

  if (len == 0) {
    return false;
  }
  int first_day_of_week = _wtoi(result_buffer);

  // convert Win API MSDN to js standard 0-6 (Sunday to Saturday)
  // https://msdn.microsoft.com/en-us/library/windows/desktop/
  //   dd373771(v=vs.85).aspx
  date_formats->first_day_of_week =
      first_day_of_week < 6 ? ++first_day_of_week : 0;

  date_formats->short_date_format = shortformat;
  date_formats->long_date_format = longdateformat;
  date_formats->time_format = timeformat;
  return true;
}

}  // namespace extensions
