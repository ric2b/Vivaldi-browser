// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.

#include "components/search_engines/default_search_engine_observer.h"

#include "components/search_engines/search_engines_managers_factory.h"
#include "components/search_engines/search_engines_prompt_manager.h"
#include "components/search_engines/template_url_service.h"

namespace vivaldi {

void DefaultSearchEngineObserver::Create(
    TemplateURLService* template_url_service,
    PrefService* prefs) {
  if (template_url_service && prefs) {
    new DefaultSearchEngineObserver(template_url_service, prefs);
  }
}

DefaultSearchEngineObserver::~DefaultSearchEngineObserver() = default;

DefaultSearchEngineObserver::DefaultSearchEngineObserver(
    TemplateURLService* new_search_service,
    PrefService* prefs)
    : service_(new_search_service), prefs_(prefs) {
  DCHECK(service_);

  observation_.Observe(service_.get());

  const TemplateURL* default_search_provider =
      service_->GetDefaultSearchProvider();
  if (default_search_provider)
    previous_default_search_provider_data_ = default_search_provider->data();
}

void DefaultSearchEngineObserver::OnTemplateURLServiceChanged() {
  DCHECK(service_);

  // Check whether the default search provider was changed.
  const TemplateURL* new_search = service_->GetDefaultSearchProvider();
  if (!new_search) {
    return;
  }
  // Ignore change if there was no previous default search provider (we just
  // loaded it)
  if (!previous_default_search_provider_data_) {
    previous_default_search_provider_data_ = new_search->data();
    return;
  }
  const TemplateURLData* old_search_data =
      &previous_default_search_provider_data_.value();
  if (service_->VivaldiIsDefaultOverridden() ||
      TemplateURL::MatchesData(new_search, old_search_data,
                               service_->search_terms_data())) {
    // Search is temporary overridden OR provider did NOT change.
    return;
  }

  previous_default_search_provider_data_ = new_search->data();

  const auto* prompt_manager = SearchEnginesManagersFactory::GetInstance()
                                   ->GetSearchEnginesPromptManager();

  const SearchEngineType current_search_type =
      new_search->GetEngineType(service_->search_terms_data());
  if (prompt_manager->ShouldPromptForTypeOrURL(
          current_search_type,
          new_search->GenerateSearchURL(service_->search_terms_data()))) {
    prompt_manager->PutProfileToQuarantine(prefs_);
  }
}

void DefaultSearchEngineObserver::OnTemplateURLServiceShuttingDown() {
  DCHECK(observation_.IsObservingSource(service_.get()));
  observation_.Reset();
  delete this;
}

}  // namespace vivaldi
