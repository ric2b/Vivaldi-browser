// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/search_engines/template_url_service.h"

#include <numeric>

#include "base/functional/bind.h"

#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_service_observer.h"

#include "components/search_engines/vivaldi_pref_names.h"
#include "components/search_engines/vivaldi_template_url_service.h"

namespace {

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copied from chromium/components/search_engines/template_url_service.cc
bool IsCreatedByExtension(const TemplateURL* template_url) {
  return template_url->type() == TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION ||
         template_url->type() == TemplateURL::OMNIBOX_API_EXTENSION;
}

}

namespace vivaldi {

syncer::UniquePosition::Suffix VivaldiGetPositionSuffix(
    const TemplateURL* template_url) {
  return syncer::UniquePosition::GenerateSuffix(
      syncer::ClientTagHash::FromUnhashed(syncer::SEARCH_ENGINES,
                                          template_url->sync_guid()));
}

const char* VivaldiGetDefaultProviderGuidPrefForType(
    TemplateURLService::DefaultSearchType type) {
  switch (type) {
    case TemplateURLService::kDefaultSearchMain:
      return prefs::kSyncedDefaultSearchProviderGUID;
    case TemplateURLService::kDefaultSearchPrivate:
      return prefs::kSyncedDefaultPrivateSearchProviderGUID;
    case TemplateURLService::kDefaultSearchField:
      return prefs::kSyncedDefaultSearchFieldProviderGUID;
    case TemplateURLService::kDefaultSearchFieldPrivate:
      return prefs::kSyncedDefaultPrivateSearchFieldProviderGUID;
    case TemplateURLService::kDefaultSearchSpeeddials:
      return prefs::kSyncedDefaultSpeedDialsSearchProviderGUID;
    case TemplateURLService::kDefaultSearchSpeeddialsPrivate:
      return prefs::kSyncedDefaultSpeedDialsPrivateSearchProviderGUID;
    case TemplateURLService::kDefaultSearchImage:
      return prefs::kSyncedDefaultImageSearchProviderGUID;
  }
  return nullptr;
}

}  // namespace vivaldi

void TemplateURLService::VivaldiMoveTemplateURL(TemplateURL* url,
                                                const TemplateURL* successor) {
  DCHECK(!IsCreatedByExtension(url));
  DCHECK(url != successor);
  syncer::UniquePosition invalid;
  syncer::UniquePosition after;
  if (successor)
    after = successor->data().vivaldi_position;
  const syncer::UniquePosition before = std::accumulate(
      template_urls_.begin(), template_urls_.end(), invalid,
      [after](const auto& position, const auto& template_url) {
        if (!template_url->data().vivaldi_position.IsValid())
          return position;
        if (position.IsValid() &&
            template_url->data().vivaldi_position.LessThan(position))
          return position;
        if (!after.IsValid() ||
            template_url->data().vivaldi_position.LessThan(after))
          return template_url->data().vivaldi_position;
        return position;
      });
  TemplateURLData data(url->data());
  if (after.IsValid() && before.IsValid())
    data.vivaldi_position = syncer::UniquePosition::Between(
        before, after, vivaldi::VivaldiGetPositionSuffix(url));
  else if (after.IsValid())
    data.vivaldi_position = syncer::UniquePosition::Before(
        after, vivaldi::VivaldiGetPositionSuffix(url));
  else if (before.IsValid())
    data.vivaldi_position = syncer::UniquePosition::After(
        before, vivaldi::VivaldiGetPositionSuffix(url));
  else  // We don't need to reorder if there is only one orderable item.
    return;
  Update(url, TemplateURL(data));
}

void TemplateURLService::ResetTemplateURL(
    TemplateURL* url,
    const std::u16string& title,
    const std::u16string& keyword,
    const std::string& search_url,
    const std::string& search_post_params,
    const std::string& suggest_url,
    const std::string& suggest_post_params,
    const std::string& image_url,
    const std::string& image_post_params,
    const GURL& favicon_url) {
  DCHECK(!IsCreatedByExtension(url));
  DCHECK(!keyword.empty());
  DCHECK(!search_url.empty());
  TemplateURLData data(url->data());
  // If we change anything fundamental about a prepopulated engine,
  // it needs to be removed and re-added as a new engine instead of
  // simply updated
  bool reset_prepopulated =
      data.prepopulate_id > 0 &&
      (search_url != data.url() ||
       search_post_params != data.search_url_post_params ||
       suggest_url != data.suggestions_url ||
       suggest_post_params != data.suggestions_url_post_params ||
       image_url != data.image_url ||
       image_post_params != data.image_url_post_params);

  data.SetShortName(title);
  data.SetKeyword(keyword);
  data.SetURL(search_url);
  data.search_url_post_params = search_post_params;
  data.suggestions_url = suggest_url;
  data.suggestions_url_post_params = suggest_post_params;
  data.image_url = image_url;
  data.image_url_post_params = image_post_params;
  data.favicon_url = favicon_url;
  data.safe_for_autoreplace = false;
  data.last_modified = clock_->Now();
  data.is_active = TemplateURLData::ActiveStatus::kTrue;

  if (!reset_prepopulated) {
    Update(url, TemplateURL(data));
    return;
  }

  data.prepopulate_id = 0;
  // Using a new guid will cause sync to add this as a new turl instead of
  // updating the existing one.
  data.GenerateSyncGUID();

  TemplateURLData prepopulate_data(url->data());
  prepopulate_data.is_active = TemplateURLData::ActiveStatus::kFalse;
  prepopulate_data.id = kInvalidTemplateURLID;

  Update(url, TemplateURL(data));

  for (int i = 0; i < kDefaultSearchTypeCount; i++) {
    if (url == default_search_provider_[i])
      prefs_->SetString(vivaldi::VivaldiGetDefaultProviderGuidPrefForType(
                            DefaultSearchType(i)),
          data.sync_guid);
  }

  // Re-add a disabled version of the prepopulated turl. Sync will pick this up
  // and disable it on other clients.
  Add(std::make_unique<TemplateURL>(prepopulate_data));
}

void TemplateURLService::VivaldiSetDefaultOverride(TemplateURL* url) {
  vivaldi_default_override_ = url;
  for (auto& observer : model_observers_) {
    observer.OnTemplateURLServiceChanged();
  }
}

bool TemplateURLService::VivaldiIsDefaultOverridden() {
  return vivaldi_default_override_ != nullptr;
}

void TemplateURLService::VivaldiResetDefaultOverride() {
  vivaldi_default_override_ = nullptr;
  for (auto& observer : model_observers_) {
    observer.OnTemplateURLServiceChanged();
  }
}

std::array<DefaultSearchManager, TemplateURLService::kDefaultSearchTypeCount>
TemplateURLService::VivaldiGetDefaultSearchManagers(
    PrefService* prefs,
    search_engines::SearchEngineChoiceService* search_engine_choice_service) {
  return {
      {{prefs, search_engine_choice_service,
        DefaultSearchManager::kDefaultSearchProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this),
                            TemplateURLService::kDefaultSearchMain)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
       },
       {prefs, search_engine_choice_service,
        DefaultSearchManager::kDefaultPrivateSearchProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this), kDefaultSearchPrivate)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
       },
       {prefs, search_engine_choice_service,
        DefaultSearchManager::kDefaultSearchFieldProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this), kDefaultSearchField)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
       },
       {prefs, search_engine_choice_service,
        DefaultSearchManager::kDefaultPrivateSearchFieldProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this), kDefaultSearchFieldPrivate)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
       },
       {prefs, search_engine_choice_service,
        DefaultSearchManager::kDefaultSpeeddialsSearchProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this), kDefaultSearchSpeeddials)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
       },
       {prefs, search_engine_choice_service,
        DefaultSearchManager::
            kDefaultSpeeddialsPrivateSearchProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this),
                            kDefaultSearchSpeeddialsPrivate)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
            ,
        for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
       },
       {prefs, search_engine_choice_service,
        DefaultSearchManager::kDefaultImageSearchProviderDataPrefName,
        base::BindRepeating(&TemplateURLService::ApplyDefaultSearchChange,
                            base::Unretained(this), kDefaultSearchImage)}
#if BUILDFLAG(IS_CHROMEOS_LACROS)
       ,
       for_lacros_main_profile
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
      },
  };
}
