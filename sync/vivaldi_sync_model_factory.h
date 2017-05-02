// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_SYNC_MODEL_FACTORY_H_
#define VIVALDI_SYNC_MODEL_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

template <typename T>
struct DefaultSingletonTraits;

class VivaldiSyncModel;
class Profile;

// Singleton that owns all SyncModels and associates them with Profiles.
class SyncModelFactory : public BrowserContextKeyedServiceFactory {
 public:
  static VivaldiSyncModel* GetForProfile(Profile* profile);

  static VivaldiSyncModel* GetForProfileIfExists(Profile* profile);

  static SyncModelFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SyncModelFactory>;

  SyncModelFactory();
  ~SyncModelFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(SyncModelFactory);
};

#endif  // VIVALDI_SYNC_MODEL_FACTORY_H_
