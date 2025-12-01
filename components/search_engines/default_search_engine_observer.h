// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_ENGINE_OBSERVER_H_
#define COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_ENGINE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service_observer.h"

class PrefService;
class TemplateURLService;

namespace vivaldi {

class DefaultSearchEngineObserver : public TemplateURLServiceObserver {
 public:
  static void Create(TemplateURLService* template_url_service,
                     PrefService* prefs);

  // TemplateURLServiceObserver implementation.
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

 private:
  explicit DefaultSearchEngineObserver(TemplateURLService* template_url_service,
                                       PrefService* prefs);

  DefaultSearchEngineObserver(const DefaultSearchEngineObserver&) = delete;
  DefaultSearchEngineObserver& operator=(const DefaultSearchEngineObserver&) =
      delete;

  ~DefaultSearchEngineObserver() override;

  const raw_ptr<TemplateURLService> service_;
  const raw_ptr<PrefService> prefs_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      observation_{this};

  std::optional<TemplateURLData> previous_default_search_provider_data_;

  base::WeakPtrFactory<DefaultSearchEngineObserver> weak_factory_{this};
};

}  // namespace vivaldi
#endif  // COMPONENTS_SEARCH_ENGINES_DEFAULT_SEARCH_ENGINE_OBSERVER_H_
