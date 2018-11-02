// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_SERVICE_FACTORY_H_
#define CONTACT_CONTACT_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "contact/contact_service.h"

class Profile;

namespace contact {

// Singleton that owns all ContactService and associates them with
// Profiles.
class ContactServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ContactService* GetForProfile(Profile* profile);

  static ContactService* GetForProfileIfExists(Profile* profile,
                                               ServiceAccessType sat);

  static ContactService* GetForProfileWithoutCreating(Profile* profile);

  static ContactServiceFactory* GetInstance();

  // In the testing profile, we often clear the contacts before making a new
  // one. This takes care of that work. It should only be used in tests.
  // Note: This does not do any cleanup; it only destroys the service. The
  // calling test is expected to do the cleanup before calling this function.
  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<ContactServiceFactory>;

  ContactServiceFactory();
  ~ContactServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace contact

#endif  // CONTACT_CONTACT_SERVICE_FACTORY_H_
