// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_VIVALDI_PREF_NAMES_H_
#define COMPONENTS_SEARCH_ENGINES_VIVALDI_PREF_NAMES_H_

namespace prefs {

constexpr char kLanguageAtInstall[] = "vivaldi.language_at_install";

constexpr char kSyncedDefaultPrivateSearchProviderGUID[] =
    "default_search_provider.synced_guid_private";
constexpr char kSyncedDefaultSearchFieldProviderGUID[] =
    "default_search_provider.synced_guid_search_field";
constexpr char kSyncedDefaultPrivateSearchFieldProviderGUID[] =
    "default_search_provider.synced_guid_search_field_private";
constexpr char kSyncedDefaultSpeedDialsSearchProviderGUID[] =
    "default_search_provider.synced_guid_speeddials";
constexpr char kSyncedDefaultSpeedDialsPrivateSearchProviderGUID[] =
    "default_search_provider.synced_guid_speeddials_private";
constexpr char kSyncedDefaultImageSearchProviderGUID[] =
    "default_search_provider.synced_guid_image";
}  // namespace prefs

#endif  // COMPONENTS_SEARCH_ENGINES_VIVALDI_PREF_NAMES_H_
