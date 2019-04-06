// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"

#include "app/vivaldi_constants.h"
#include "base/lazy_instance.h"
#include "base/macros.h"

namespace vivaldi {

namespace {

const char *vivaldi_extra_locales_array[] = {
  "be",
  "en-AU",
  "en-IN",
  "eo",
  "es-PE",
  "eu",
  "fy",
  "gd",
  "gl",
  "hy",
  "io",
  "is",
  "jbo",
  "ka",
  "ku",
  "mk",
  "nn",
  "sc",
  "sq",
};

base::LazyInstance<std::set<std::string>>::DestructorAtExit
    vivaldi_extra_locales = LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool IsVivaldiApp(const std::string &extension_id) {
  return extension_id == kVivaldiAppIdHex || extension_id == kVivaldiAppId;
}

const std::set<std::string> &GetVivaldiExtraLocales() {
  auto *extra_locales = vivaldi_extra_locales.Pointer();
  DCHECK(extra_locales);

  if (extra_locales->empty()) {
    for (size_t i = 0; i < arraysize(vivaldi_extra_locales_array); i++)
      extra_locales->insert(vivaldi_extra_locales_array[i]);
  }
  return *extra_locales;
}

bool IsVivaldiExtraLocale(const std::string &locale) {
  auto &extra_locales = GetVivaldiExtraLocales();
  return extra_locales.find(locale) != extra_locales.end();
}

}  // namespace vivaldi
