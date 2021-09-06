// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_SERVICE_FACTORY_H_
#define COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "mail_client_service.h"

class Profile;

namespace mail_client {

// Singleton that owns all MailClientService and associates them with
// Profiles.
class MailClientServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static MailClientService* GetForProfile(Profile* profile);

  static MailClientService* GetForProfileIfExists(Profile* profile,
                                                  ServiceAccessType sat);

  static MailClientService* GetForProfileWithoutCreating(Profile* profile);

  static MailClientServiceFactory* GetInstance();

  // In the testing profile, we often clear the mail_client_service before
  // making a new one. This takes care of that work. It should only be used in
  // tests. Note: This does not do any cleanup; it only destroys the service.
  // The calling test is expected to do the cleanup before calling this
  // function.
  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<MailClientServiceFactory>;

  MailClientServiceFactory();
  ~MailClientServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace mail_client

#endif  // COMPONENTS_DB_MAIL_CLIENT_MAIL_CLIENT_SERVICE_FACTORY_H_
