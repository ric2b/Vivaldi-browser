//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <winnls.h>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace extensions {

namespace {

const wchar_t kVivaldiKey[] = L"Software\\Vivaldi";
const wchar_t kUniqueUserValue[] = L"unique_user_id";

}  // anonymous namespace

bool UtilitiesGetSystemDateFormatFunction::ReadDateFormats(
    vivaldi::utilities::DateFormats* date_formats) {
  // According to MSDN documentation max len is 80
  // https://msdn.microsoft.com/en-us/library/windows/desktop/
  //   dd373896(v=vs.85).aspx
  wchar_t result_buffer[80];
  int len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT,
                            result_buffer, base::size(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string timeformat = base::UTF16ToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE,
                        result_buffer, base::size(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string shortformat = base::UTF16ToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SLONGDATE,
                        result_buffer, base::size(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string longdateformat = base::UTF16ToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK,
                        result_buffer, base::size(result_buffer));

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

std::string UtilitiesGetSystemCountryFunction::ReadCountry() {
  // TODO(igor@vivaldi.com): Consider calling GetUserDefaultGeoName() instead.
  // It is only available in Windows 10 so calling it require looking up the
  // function at runtime.
  GEOID geo_id = GetUserGeoID(GEOCLASS_NATION);
  if (geo_id == GEOID_NOT_AVAILABLE)
    return std::string();
  char buffer[3];
  if (0 == GetGeoInfoA(geo_id, GEO_ISO2, buffer,
                       sizeof buffer / sizeof buffer[0], 0)) {
    LOG(ERROR) << "Windows API error " << GetLastError();
    return std::string();
  }
  return std::string(buffer);
}

}  // namespace extensions
