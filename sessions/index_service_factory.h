// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SESSIONS_INDEX_SERVICE_FACTORY_H_
#define SESSIONS_INDEX_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

template <typename T>
struct DefaultSingletonTraits;

namespace sessions {

class Index_Model;

// Singleton that owns all SessionIndex_Model entries and associates them with
// Profiles.
class IndexServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static Index_Model* GetForBrowserContext(content::BrowserContext* context);

  static Index_Model* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  static IndexServiceFactory* GetInstance();

  static void ShutdownForProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<IndexServiceFactory>;

  IndexServiceFactory();
  ~IndexServiceFactory() override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool ServiceIsNULLWhileTesting() const override;
};

}  //  namespace sessions

#endif  // SESSIONS_INDEX_SERVICE_FACTORY_H_