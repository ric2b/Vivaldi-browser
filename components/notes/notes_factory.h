// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_NOTES_NOTES_FACTORY_H_
#define COMPONENTS_NOTES_NOTES_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

template <typename T>
struct DefaultSingletonTraits;

namespace vivaldi {

class NotesModel;

class NotesModelFactory : public BrowserContextKeyedServiceFactory {
 public:
  static NotesModel* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static NotesModel* GetForBrowserContextIfExists(
      content::BrowserContext* browser_context);

  static NotesModelFactory* GetInstance();

  int64_t get_current_id_max() { return current_max_id_; }

 private:
  friend struct base::DefaultSingletonTraits<NotesModelFactory>;

  NotesModelFactory();
  ~NotesModelFactory() override;
  NotesModelFactory(const NotesModelFactory&) = delete;
  NotesModelFactory& operator=(const NotesModelFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  static const bool kServiceRedirectedInIncognito = true;

  int64_t current_max_id_;
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTES_FACTORY_H_
