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

#include "launch_bar_gnome_support.h"

namespace extensions {
void reduce_spaces(std::string& s) {
  // strip leading spaces.
  unsigned i;
  for (i = 0; s[i] == ' ' && i < s.length(); i++)
    ;
  s.erase(0, i);

  // reduce double spaces
  for (i = 0; i < s.length(); i++) {
    if (s[i] == ' ' && s[i + 1] == ' ') {
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
    {'z', ""},         {'Z', ""},     {'f', "SSS"},        {'r', "hh:mm:ss A"}};

// TODO: Three known languages that still might need special treatment are
// Tongan, Farsi and Vietnamese, out of 92 tested so far.
std::string getMomentJsFormatString(const char* fmt, bool short_date) {
  std::string s = "";
  std::map<char, std::string>::iterator it;
  char c;

  for (int i = 0; fmt[i] != '\0'; i++) {
    if (fmt[i] == '%') {
      c = fmt[i + 1];
      if (c == '-') {
        i++;
        c = fmt[i + 1];
      }

      // Long form sometimes uses %b (month number) for the month where we
      // really want month name instead
      if (c == 'b') {
        s += (short_date) ? "MM" : "MMMM";
        i++;
        continue;
      }

      // Seems to be a quirk of some locales (like Icelandic) to include
      // the day twice for the short date
      if ((c == 'a' || c == 'A') && short_date) {
        i += 2;
        continue;
      }

      it = dateFormatMods.find(c);
      if (it != dateFormatMods.end()) {
        s += it->second;
      }
      i++;

      // Norway adds 'kl.' in front of its time format. This code might seem
      // like too much special-casing, but Norway really is the only country
      // in the world that does this and 'kl' interferes with moment.js
      // formatting.
    } else if (fmt[i] == 'k' && fmt[i + 1] == 'l' && fmt[i + 2] == '.') {
      i += 2;
    } else {
      s += fmt[i];
    }
  }
  reduce_spaces(s);
  return s;
}

static int first_weekday(void) {
  const char* const s = nl_langinfo(_NL_TIME_FIRST_WEEKDAY);

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
  date_formats->time_format =
      getMomentJsFormatString(nl_langinfo(T_FMT), false);

  return true;
}

std::optional<bool>
UtilitiesIsVivaldiPinnedToLaunchBarFunction::CheckIsPinned() {
  if (dock::GnomeLaunchBar::IsGnomeRunning()) {
    return dock::GnomeLaunchBar::IsVivaldiPinned();
  }

  LOG(INFO) << "Pinning is not suppported by the current linux environment.";
  return std::nullopt;
}

bool UtilitiesPinVivaldiToLaunchBarFunction::PinToLaunchBar() {
  if (dock::GnomeLaunchBar::IsGnomeRunning()) {
    return dock::GnomeLaunchBar::PinVivaldi();
  }

  return false;
}

}  // namespace extensions
