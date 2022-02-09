// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_NOTE_SYNC_SERVICE_FACTORY_H_
#define SYNC_NOTE_SYNC_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace sync_notes {
class NoteSyncService;
}

namespace vivaldi {

// Singleton that owns the note sync service.
class NoteSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of NoteSyncService associated with this profile
  // (creating one if none exists).
  static sync_notes::NoteSyncService* GetForProfile(Profile* profile);

  // Returns an instance of the NoteSyncServiceFactory singleton.
  static NoteSyncServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<NoteSyncServiceFactory>;

  NoteSyncServiceFactory();
  ~NoteSyncServiceFactory() override;
  NoteSyncServiceFactory(const NoteSyncServiceFactory&) = delete;
  NoteSyncServiceFactory& operator=(const NoteSyncServiceFactory&) = delete;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // SYNC_NOTE_SYNC_SERVICE_FACTORY_H_
