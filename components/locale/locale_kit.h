// Copyright 2021 Vivaldi Technologies. All rights reserved.
//

#ifndef COMPONENTS_LOCALE_LOCALE_KIT_H_
#define COMPONENTS_LOCALE_LOCALE_KIT_H_

#include "base/containers/span.h"
#include "base/strings/string_piece.h"

namespace locale_kit {

std::string GetUserCountry();

std::string FindBestMatchingLocale(base::span<const base::StringPiece> locales,
                                   const std::string& application_locale);

base::StringPiece FindBestMatchingLocale(
    base::StringPiece language,
    base::StringPiece country,
    base::span<const base::StringPiece> locales);

}  // namespace locale_kit

#endif  // COMPONENTS_LOCALE_LOCALE_KIT_H_
