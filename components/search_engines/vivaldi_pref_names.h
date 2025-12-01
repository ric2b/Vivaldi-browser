// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_VIVALDI_PREF_NAMES_H_
#define COMPONENTS_SEARCH_ENGINES_VIVALDI_PREF_NAMES_H_

namespace prefs {

constexpr char kLanguageAtInstall[] = "vivaldi.language_at_install";

constexpr char kDefaultPrivateSearchProviderGUID[] =
    "default_search_provider.guid_private";
constexpr char kDefaultSearchFieldProviderGUID[] =
    "default_search_provider.guid_search_field";
constexpr char kDefaultPrivateSearchFieldProviderGUID[] =
    "default_search_provider.guid_search_field_private";
constexpr char kDefaultSpeedDialsSearchProviderGUID[] =
    "default_search_provider.guid_speeddials";
constexpr char kDefaultSpeedDialsPrivateSearchProviderGUID[] =
    "default_search_provider.guid_speeddials_private";
constexpr char kDefaultImageSearchProviderGUID[] =
    "default_search_provider.guid_image";

}  // namespace prefs

#endif  // COMPONENTS_SEARCH_ENGINES_VIVALDI_PREF_NAMES_H_
