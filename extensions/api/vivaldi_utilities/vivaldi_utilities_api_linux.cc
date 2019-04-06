//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"

#include <langinfo.h>
#include <locale>
#include <map>

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/path_service.h"

namespace {
const char vivaldi_uuid_file_name[] = ".vivaldi_user_id";

base::FilePath GetUserIdFilePath() {
  return base::nix::GetXDGDirectory(base::Environment::Create().get(),
                                    "XDG_DATA_HOME", ".local/share")
      .AppendASCII(vivaldi_uuid_file_name);
}
}  // anonymous namespace

namespace extensions {
bool UtilitiesGetUniqueUserIdFunction::ReadUserIdFromOSProfile(
    std::string* user_id) {
  return base::ReadFileToString(GetUserIdFilePath(), user_id);
}

void UtilitiesGetUniqueUserIdFunction::WriteUserIdToOSProfile(
    const std::string& user_id) {
  base::WriteFile(GetUserIdFilePath(), user_id.c_str(), user_id.length());
}

void reduce_spaces(std::string& s) {
  // strip leading spaces.
  unsigned i;
  for (i = 0; s[i] == ' ' && i < s.length(); i++);
  s.erase(0, i);

  // reduce double spaces
  for (i = 0; i < s.length(); i++) {
    if(s[i] == ' ' && s[i+1] == ' ') {
      s.erase(i, 1);
      i--;
    }
  }

  // strip trailing spaces and commas
  while (s.back() == ' ' || s.back() == ',') {
    s.pop_back();
  }
}

// Turn strftime type format patterns into moment.js compatible ones.
// See: https://github.com/benjaminoakes/moment-strftime for more info.
std::map<char, std::string> dateFormatMods = {
    {'a', "dddd"},     {'A', "dddd"}, {'B', "MMMM"},       {'c', "lll"},
    {'d', "DD"},       {'e', "D"},    {'F', "YYYY-MM-DD"}, {'H', "HH"},
    {'I', "hh"},       {'j', "DDDD"}, {'k', "H"},          {'l', "h"},
    {'m', "MM"},       {'M', "mm"},   {'p', "A"},          {'S', "ss"},
    {'T', "HH:mm:ss"}, {'u', "E"},    {'w', "d"},          {'W', "WW"},
    {'x', "ll"},       {'X', "LTS"},  {'y', "YY"},         {'Y', "YYYY"},
    {'z', ""},         {'Z', ""},     {'f', "SSS"},        {'r', "HH:mm:ss A"}};

std::string getMomentJsFormatString(const char* fmt, bool short_date) {
  std::string s = "";
  std::map<char,std::string>::iterator it;
  char c;

  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      c = fmt[i+1];
      if (c == '-') {
        i++;
        c = fmt[i+1];
      }

      // Long form sometimes uses %b (month number) for the month where we
      // really want month name instead
      if (c == 'b') {
        s += (short_date) ? "MM" : "MMMM";
        i++;
        continue;
      }

      // Seems to be a quirk of the Icelandic language to include the day twice
      // for the short date (Of dozens of languages tested, only South Africa
      // also had this problem)
      if ((c == 'a' || c == 'A') && short_date) {
        i += 2;
        continue;
      }

      it = dateFormatMods.find(c);
      if (it != dateFormatMods.end()) {
        s += it->second;
      }
      i++;
    } else {
      s += fmt[i];
    }
  }
  reduce_spaces(s);
  return s;
}

static int first_weekday(void) {
  const char *const s = nl_langinfo(_NL_TIME_FIRST_WEEKDAY);

  if (s && *s >= 1 && *s <= 7)
      return (int)*s;

  /* Default to Sunday, 1. */
  return 1;
}

bool UtilitiesGetSystemDateFormatFunction::ReadDateFormats(
    vivaldi::utilities::DateFormats* date_formats) {

  // This initializes the system locale for LC_TIME. Not all locale categories
  // are necessarily equal, but LC_TIME is what is relevant to us.
  setlocale(LC_TIME, "");

  // Linux weekdays start on 1, moment.js starts on 0.
  date_formats->first_day_of_week = first_weekday() - 1;
  date_formats->short_date_format =
      getMomentJsFormatString(nl_langinfo(D_FMT), true);
  date_formats->long_date_format =
      getMomentJsFormatString(nl_langinfo(D_T_FMT), false);
  date_formats->time_format = getMomentJsFormatString(nl_langinfo(T_FMT), false);

  return true;
}
}  // namespace extensions
