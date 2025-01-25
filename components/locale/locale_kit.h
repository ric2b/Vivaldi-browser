// Copyright 2021 Vivaldi Technologies. All rights reserved.
//

#ifndef COMPONENTS_LOCALE_LOCALE_KIT_H_
#define COMPONENTS_LOCALE_LOCALE_KIT_H_

#include <string_view>

#include "base/containers/span.h"

namespace locale_kit {

std::string GetUserCountry();

std::string FindBestMatchingLocale(base::span<const std::string_view> locales,
                                   const std::string& application_locale);

std::string_view FindBestMatchingLocale(
    std::string_view language,
    std::string_view country,
    base::span<const std::string_view> locales);

}  // namespace locale_kit

#endif  // COMPONENTS_LOCALE_LOCALE_KIT_H_
