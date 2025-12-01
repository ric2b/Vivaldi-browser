// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATE_UPDATE_SERVICE_FACTORY_H_
#define UPDATE_UPDATE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "update/update_service.h"

class Profile;

namespace update {

// Singleton that owns all UpdateService and associates them with
// Profiles.
class UpdateServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static UpdateService* GetForProfile(Profile* profile);

  static UpdateService* GetForProfileIfExists(Profile* profile,
                                              ServiceAccessType sat);

  static UpdateService* GetForProfileWithoutCreating(Profile* profile);

  static UpdateServiceFactory* GetInstance();

  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<UpdateServiceFactory>;

  UpdateServiceFactory();
  ~UpdateServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace update

#endif  // UPDATE_UPDATE_SERVICE_FACTORY_H_
