// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/search_engines/search_engines_prompt_manager.h"

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/search_engine_utils.h"
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

namespace {
// Copy from components/ad_blocker/adblock_known_sources_handler_impl.cc
// Remove when the IDs for known sources are exposed in the API.
const char kPartnersList[] =
    "https://downloads.vivaldi.com/lists/vivaldi/partners-current.txt";
}  // namespace

SearchEnginesPromptManager::SearchEnginesPromptManager(
    std::unique_ptr<ParsedSearchEnginesPrompt> prompt)
    : prompt_(std::move(prompt)) {
  CHECK(prompt_);
}

SearchEnginesPromptManager::~SearchEnginesPromptManager() = default;

void SearchEnginesPromptManager::MarkCurrentPromptAsSeen(
    PrefService* prefs) const {
  prefs->SetInteger(vivaldiprefs::kStartupLastSeenSearchEnginePromptVersion,
                    GetCurrentVersion());
}

TemplateURL* SearchEnginesPromptManager::GetDefaultSearchEngineToPrompt(
    PrefService* prefs,
    TemplateURLService* template_url_service,
    adblock_filter::RuleService* rules_service) const {
  if (!prefs || !template_url_service || !template_url_service->loaded() ||
      !rules_service->IsLoaded()) {
    return nullptr;
  }

  const TemplateURL* current_search;
  current_search = template_url_service->GetDefaultSearchProvider(
      TemplateURLService::kDefaultSearchMain);

  std::unique_ptr<TemplateURLData> default_search =
      TemplateURLPrepopulateData::GetPrepopulatedFallbackSearch(
          prefs, nullptr, TemplateURLPrepopulateData::SearchType::kMain);

  auto shouldPrompt = [&]() {
    // Do not prompt when:

    // * SearchEnginesPromptManager is invalid (SearchEnginesManager version
    // dependency is greater than current version)
    if (!IsValid()) {
      return false;
    }

    const uint32_t partner_list_id =
        adblock_filter::RuleSourceCore::FromUrl(GURL(kPartnersList))->id();
    // * 'Allow Ads from our partners' adblocking source is disabled
    if (!rules_service->GetKnownSourcesHandler()->IsSourceEnabled(
            adblock_filter::RuleGroup::kAdBlockingRules, partner_list_id)) {
      return false;
    }

    // * last seen version of the prompt has already been seen
    if (prefs->GetInteger(
            vivaldiprefs::kStartupLastSeenSearchEnginePromptVersion) >=
        GetCurrentVersion()) {
      return false;
    }
    // * should not prompt for current search engine
    const SearchEngineType current_search_type = current_search->GetEngineType(
        template_url_service->search_terms_data());
    if (!ShouldPromptForTypeOrURL(
            current_search_type,
            current_search->GenerateSearchURL(
                template_url_service->search_terms_data()))) {
      return false;
    }
    // * should prompt for default search engine for locale
    const SearchEngineType default_search_type =
        SearchEngineUtils::GetEngineType(GURL(default_search->url()));
    if (ShouldPromptForTypeOrURL(default_search_type,
                                 GURL(default_search->url()))) {
      return false;
    }

    return true;
  };

  auto getTemplateURLByPrepopulateId =
      [&template_url_service](const auto& id) -> TemplateURL* {
    TemplateURLService::TemplateURLVector template_urls =
        template_url_service->GetTemplateURLs();
    const auto template_url_iter =
        std::find_if(template_urls.begin(), template_urls.end(),
                     [id](const auto template_url) {
                       return template_url->is_active() !=
                                  TemplateURLData::ActiveStatus::kFalse &&
                              template_url->prepopulate_id() == id;
                     });
    return template_url_iter != template_urls.end() ? *template_url_iter
                                                    : nullptr;
  };
  // The default search engine from GetPrepopulatedFallbackSearch() is not a
  // valid TemplateURL managed by TemplateURLService, we must find a correct
  // TemplateURL by looking for the same prepopulate ID.
  return shouldPrompt()
             ? getTemplateURLByPrepopulateId(default_search->prepopulate_id)
             : nullptr;
}

bool SearchEnginesPromptManager::ShouldPromptForTypeOrURL(
    const SearchEngineType& type,
    const GURL& url) const {
  if (type == SearchEngineType::SEARCH_ENGINE_OTHER) {
    const auto urls = prompt_->prompt_if_domain();
    return std::any_of(urls.begin(), urls.end(),
                       [&url](const auto& it) { return url.DomainIs(it); });
  }
  const auto prompt_if_type = prompt_->prompt_if_type();
  return prompt_if_type.find(type) != prompt_if_type.end();
}

int SearchEnginesPromptManager::GetCurrentVersion() const {
  return prompt_->version();
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
