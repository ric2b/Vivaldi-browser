// Copyright 2019 Vivaldi Technologies. All rights reserved.

#include "components/locale/locale_kit.h"

#include "base/environment.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/country_codes/country_codes.h"

namespace locale_kit {

const char kVivaldiCountry[] = "VIVALDI_COUNTRY";

std::string GetUserCountry() {
  std::string country;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar(kVivaldiCountry, &country)) {
    if (country.empty()) {
      // This allows testing of code paths that deals with an unknown country.
      return country;
    }
    if (country.length() == 2)
      return base::ToUpperASCII(country);
    LOG(ERROR) << kVivaldiCountry << " must be two-letter country ISO code - "
               << country;
  }
  int code = country_codes::GetCurrentCountryID();
  if (code == country_codes::kCountryIDUnknown)
    return std::string();
  char buffer[2];
  buffer[0] = static_cast<char>(code >> 8);
  buffer[1] = static_cast<char>(code);
  return std::string(buffer, 2);
}

std::string FindBestMatchingLocale(base::span<const std::string_view> locales,
                                   const std::string& application_locale) {
  // Locale may or may not contain the country part and it may be unrelated to
  // the user location, so always use the country from the system and strip the
  // country from the locale.
  std::string language = application_locale;
  if (language.length() > 2 && (language[2] == '-' || language[2] == '_')) {
    language.resize(2);
  }
  std::string country = GetUserCountry();
  std::string_view locale = FindBestMatchingLocale(language, country, locales);
  return std::string(locale.data(), locale.length());
}

namespace {

bool HasLanguageDashPrefix(std::string_view locale, std::string_view language) {
  size_t n = language.length();
  if (locale.length() <= n)
    return false;
  return locale.substr(0, n) == language && locale[n] == '-';
}

bool HasDashCountrySuffix(std::string_view locale, std::string_view country) {
  if (locale.length() <= country.length())
    return false;
  size_t n = locale.length() - country.length();
  return locale[n - 1] == '-' && locale.substr(n) == country;
}

}  // namespace

std::string_view FindBestMatchingLocale(
    std::string_view language,
    std::string_view country,
    base::span<const std::string_view> locales) {
  if (!country.empty()) {
    auto i = std::find_if(locales.begin(), locales.end(),
                          [&](std::string_view locale) {
                            return HasLanguageDashPrefix(locale, language) &&
                                   HasDashCountrySuffix(locale, country);
                          });
    if (i != locales.end())
      return *i;
    // No exact match, see if we have a file for the country in any language
    i = std::find_if(locales.begin(), locales.end(),
                     [&](std::string_view locale) {
                       return HasDashCountrySuffix(locale, country);
                     });
    if (i != locales.end()) {
      // TODO(igor@vivaldi.com): For Norway we want to prefer nb-NO, not
      // nn-NO, if the language is not Norwegian. Luckily nb should be sorted
      // before nn, so we have the proper locale. But we may be less luckily
      // if we get more cases with multiple languages per country, so a generic
      // way to deal with this may need to be invented.
      return *i;
    }
  } else {
    // Country is not known, guess it based on the language. This assumes that
    // the list is sorted so the first entry for the language with multiple
    // country-specific locales is one that we should use.
    auto i = std::find_if(locales.begin(), locales.end(),
                          [&](std::string_view locale) {
                            return HasLanguageDashPrefix(locale, language);
                          });
    if (i != locales.end())
      return *i;
  }

  // Check if we have a generic language file for the language.
  auto i = std::find(locales.begin(), locales.end(), language);
  if (i != locales.end())
    return *i;

  // Try generic English as the last resort.
  i = std::find(locales.begin(), locales.end(), "en");
  if (i != locales.end())
    return *i;

  return std::string_view();
}

}  // namespace locale_kit
