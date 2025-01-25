//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <windows.h>
#include <winnls.h>
#include <optional>
#include <string>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace extensions {

bool UtilitiesGetSystemDateFormatFunction::ReadDateFormats(
    vivaldi::utilities::DateFormats* date_formats) {
  // According to MSDN documentation max len is 80
  // https://msdn.microsoft.com/en-us/library/windows/desktop/
  //   dd373896(v=vs.85).aspx
  wchar_t result_buffer[80];
  int len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_STIMEFORMAT,
                            result_buffer, std::size(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string timeformat = base::WideToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTDATE,
                        result_buffer, std::size(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string shortformat = base::WideToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SLONGDATE,
                        result_buffer, std::size(result_buffer));

  if (len == 0) {
    return false;
  }

  std::string longdateformat = base::WideToUTF8(result_buffer);
  len = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_IFIRSTDAYOFWEEK,
                        result_buffer, std::size(result_buffer));

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

std::optional<bool> UtilitiesIsVivaldiPinnedToLaunchBarFunction::CheckIsPinned() {
  return std::nullopt;
}

bool UtilitiesPinVivaldiToLaunchBarFunction::PinToLaunchBar() {
  return false;
}

}  // namespace extensions
