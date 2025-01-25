// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INVALIDATION_VIVALDI_SYNC_INVALIDATION_SERVICE_FACTORY_H_
#define SYNC_INVALIDATION_VIVALDI_SYNC_INVALIDATION_SERVICE_FACTORY_H_

#include "chrome/browser/sync/sync_invalidations_service_factory.h"

#include "base/no_destructor.h"

class Profile;

namespace syncer {
class SyncInvalidationsService;
}  // namespace syncer

namespace vivaldi {
class VivaldiSyncInvalidationsServiceFactory
    : public SyncInvalidationsServiceFactory {
 public:
  VivaldiSyncInvalidationsServiceFactory(
      const VivaldiSyncInvalidationsServiceFactory&) = delete;
  VivaldiSyncInvalidationsServiceFactory& operator=(
      const VivaldiSyncInvalidationsServiceFactory&) = delete;

  static syncer::SyncInvalidationsService* GetForProfile(Profile* profile);

  static VivaldiSyncInvalidationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<VivaldiSyncInvalidationsServiceFactory>;

  VivaldiSyncInvalidationsServiceFactory();
  ~VivaldiSyncInvalidationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};
}  // namespace vivaldi

#endif  // SYNC_INVALIDATION_VIVALDI_SYNC_INVALIDATION_SERVICE_FACTORY_H_
