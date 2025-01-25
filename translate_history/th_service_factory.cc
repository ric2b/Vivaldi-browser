// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "translate_history/th_service_factory.h"

#include "base/task/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "translate_history/th_model.h"

namespace translate_history {

TH_ServiceFactory::TH_ServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "TH_Service",
          BrowserContextDependencyManager::GetInstance()) {}

TH_ServiceFactory::~TH_ServiceFactory() {}

// static
TH_Model* TH_ServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<TH_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
TH_Model* TH_ServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<TH_Model*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
TH_ServiceFactory* TH_ServiceFactory::GetInstance() {
  return base::Singleton<TH_ServiceFactory>::get();
}

// static
void TH_ServiceFactory::ShutdownForProfile(Profile* profile) {
  GetInstance()->BrowserContextDestroyed(profile);
}

content::BrowserContext* TH_ServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* TH_ServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* pref_service = profile->GetOriginalProfile()->GetPrefs();
  bool session_only =
      pref_service->GetBoolean(prefs::kSavingBrowserHistoryDisabled);
  std::unique_ptr<TH_Model> service(new TH_Model(context, session_only));
  return service.release();
}

bool TH_ServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace translate_history
