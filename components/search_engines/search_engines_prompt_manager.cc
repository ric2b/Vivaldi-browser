// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/search_engines/search_engines_prompt_manager.h"

#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engine_utils.h"
#include "components/search_engines/search_engines_helper.h"
#include "components/search_engines/search_engines_managers_factory.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"

#include "components/ad_blocker/adblock_known_sources_handler.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/parsed_search_engines_prompt.h"
#include "components/search_engines/search_engines_manager.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

inline constexpr int kVivaldiSearchEnginePromptQuarantineInDays = 30;

SearchEnginesPromptManager::SearchEnginesPromptManager(
    std::unique_ptr<ParsedSearchEnginesPrompt> prompt)
    : prompt_(std::move(prompt)) {
  CHECK(prompt_);
}

SearchEnginesPromptManager::~SearchEnginesPromptManager() = default;

void SearchEnginesPromptManager::MarkCurrentPromptAsSeen(
    PrefService* prefs) const {
  if (!IsQuarantined(prefs)) {
    prefs->SetInteger(vivaldiprefs::kStartupLastSeenSearchEnginePromptVersion,
                      GetCurrentVersion());
    prefs->SetDouble(vivaldiprefs::kStartupLastSeenSearchEnginePromptTime,
                     base::Time::Now().InSecondsFSinceUnixEpoch());
  }
}

void SearchEnginesPromptManager::PutProfileToQuarantine(
    PrefService* prefs) const {
  if (!IsQuarantined(prefs)) {
    // Put to quarantine.
    prefs->SetDouble(vivaldiprefs::kStartupLastSeenSearchEnginePromptTime,
                     base::Time::Now().InSecondsFSinceUnixEpoch());
  }
}

bool SearchEnginesPromptManager::ShouldPrompt(
    PrefService* prefs,
    TemplateURLService* template_url_service,
    adblock_filter::RuleService* rule_service) const {
  if (!prefs || !template_url_service || !template_url_service->loaded() ||
      !rule_service->IsLoaded()) {
    return false;
  }

  const TemplateURL* current_search;
  current_search = template_url_service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchMain);

  // Do not prompt when:
  // * SearchEnginesPromptManager is invalid (SearchEnginesManager version
  // dependency is greater than current version)
  if (!IsValid()) {
    return false;
  }

  // * should not show dialog while quarantined
  if (IsQuarantined(prefs)) {
    return false;
  }

  // * 'Allow Ads from our partners' adblocking source is disabled
  if (!rule_service->GetKnownSourcesHandler()->IsPresetEnabled(
          (base::Uuid::ParseLowercase(
              adblock_filter::KnownRuleSourcesHandler::kPartnersListUuid)))) {
    return false;
  }

  // * last seen version of the prompt has already been seen
  if (prefs->GetInteger(
          vivaldiprefs::kStartupLastSeenSearchEnginePromptVersion) >=
      GetCurrentVersion()) {
    return false;
  }

  // * should not prompt for current search engine
  const SearchEngineType current_search_type =
      current_search->GetEngineType(template_url_service->search_terms_data());
  if (!ShouldPromptForTypeOrURL(
          current_search_type,
          current_search->GenerateSearchURL(
              template_url_service->search_terms_data()))) {
    return false;
  }

  return true;
}

std::vector<TemplateURL*>
SearchEnginesPromptManager::GetPartnerSearchEnginesToPrompt(
    country_codes::CountryId country_id,
    const std::string_view application_locale,
    PrefService& prefs,
    TemplateURLService* template_url_service) const {
  std::vector<TemplateURL*> partners;

  if (!template_url_service || !template_url_service->loaded()) {
    return partners;
  }

  ParsedSearchEngines::EnginesListWithDefaults prepopulated_engines =
      TemplateURLPrepopulateData::GetPrepopulatedSearchEngines(
          country_id, prefs, application_locale);

  TemplateURLService::TemplateURLVector template_urls =
      template_url_service->GetTemplateURLs();
  for (const TemplateURLPrepopulateData::PrepopulatedEngine* engine :
       prepopulated_engines.list) {
    // The partner search engines are not valid TemplateURLs managed by
    // TemplateURLService, we must find a correct TemplateURL by looking for
    // the same prepopulate ID.
    const auto template_url_iter =
        std::find_if(template_urls.begin(), template_urls.end(),
                     [&engine](const auto template_url) {
                       return engine->is_partner &&
                              template_url->is_active() !=
                                  TemplateURLData::ActiveStatus::kFalse &&
                              template_url->prepopulate_id() == engine->id;
                     });
    if (template_url_iter == template_urls.end() || !*template_url_iter) {
      continue;
    }

    const SearchEngineType default_search_type = engine->type;
    const GURL default_search_url =
        (*template_url_iter)
            ->GenerateSearchURL(template_url_service->search_terms_data());

    if (IsInExcludeList(default_search_type, default_search_url)) {
      continue;
    }
    if (ShouldPromptForTypeOrURL(default_search_type, default_search_url)) {
      continue;
    }
    partners.push_back(*template_url_iter);
  }

  base::RandomShuffle(partners.begin(), partners.end());

  return partners;
}

bool SearchEnginesPromptManager::ShouldPromptForTypeOrURL(
    const SearchEngineType& type,
    const GURL& url) const {
  if (type == SearchEngineType::SEARCH_ENGINE_OTHER ||
      type == SearchEngineType::SEARCH_ENGINE_UNKNOWN) {
    const auto urls = prompt_->prompt_if_domain();
    return std::any_of(urls.begin(), urls.end(),
                       [&url](const auto& it) { return url.DomainIs(it); });
  }
  const auto prompt_if_type = prompt_->prompt_if_type();
  return prompt_if_type.find(type) != prompt_if_type.end();
}

bool SearchEnginesPromptManager::IsInExcludeList(const SearchEngineType& type,
                                                 const GURL& url) const {
  if (type == SearchEngineType::SEARCH_ENGINE_OTHER ||
      type == SearchEngineType::SEARCH_ENGINE_UNKNOWN) {
    const auto exclude_urls = prompt_->exclude_if_domain();
    return std::any_of(exclude_urls.begin(), exclude_urls.end(),
                       [&url](const auto& it) { return url.DomainIs(it); });
  }
  const auto exclude_if_type = prompt_->exclude_if_type();
  return exclude_if_type.find(type) != exclude_if_type.end();
}

int SearchEnginesPromptManager::GetCurrentVersion() const {
  return prompt_->current_data_version();
}

std::string SearchEnginesPromptManager::GetDialogType() const {
  return prompt_->type();
}

int SearchEnginesPromptManager::GetSearchEnginesDataVersionRequired() const {
  return prompt_->search_engines_data_version_required();
}

bool SearchEnginesPromptManager::IsValid() const {
  const auto search_engines_version =
      SearchEnginesManagersFactory::GetInstance()
          ->GetSearchEnginesManager()
          ->GetCurrentDataVersion();
  return GetSearchEnginesDataVersionRequired() <= search_engines_version;
}

bool SearchEnginesPromptManager::IsQuarantined(PrefService* prefs) const {
  const base::Time last_seen_prompt = base::Time::FromSecondsSinceUnixEpoch(
      prefs->GetDouble(vivaldiprefs::kStartupLastSeenSearchEnginePromptTime));
  const base::Time now = base::Time::Now();
  const int days_since_last_seen_prompt =
      (now - last_seen_prompt).InDaysFloored();
  return days_since_last_seen_prompt <
         kVivaldiSearchEnginePromptQuarantineInDays;
}
