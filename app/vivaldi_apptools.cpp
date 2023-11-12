// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_apptools.h"

#include "app/vivaldi_constants.h"
#include "base/stl_util.h"

namespace vivaldi {

namespace {

// This must be sorted.
const char *const vivaldi_extra_locales_array[] = {
  "af",
  "be",
  "ca@valencia",
  "de-CH",
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
  "ja-KS",
  "jbo",
  "ka",
  "kab",
  "ku",
  "mk",
  "nn",
  "sc",
  "sq",
};

}  // namespace

bool IsVivaldiApp(base::StringPiece extension_id) {
  return extension_id == kVivaldiAppIdHex || extension_id == kVivaldiAppId;
}

bool IsVivaldiExtraLocale(base::StringPiece locale) {
  DCHECK([]() {
    // Verify vivaldi_extra_locales_array is sorted.
    static bool after_first = false;
    if (after_first)
      return true;
    after_first = true;
    return std::is_sorted(
        std::begin(vivaldi_extra_locales_array),
        std::end(vivaldi_extra_locales_array),
        [](const char* a, const char* b) { return strcmp(a, b) < 0; });
  }());
  return std::binary_search(std::begin(vivaldi_extra_locales_array),
                            std::end(vivaldi_extra_locales_array), locale);
}

}  // namespace vivaldi
