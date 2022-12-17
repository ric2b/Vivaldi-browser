// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_SYNC_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_
#define IOS_SYNC_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"

namespace invalidation {
class InvalidationService;
}

namespace vivaldi {

class VivaldiInvalidationServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  VivaldiInvalidationServiceFactory(const VivaldiInvalidationServiceFactory&) =
      delete;
  VivaldiInvalidationServiceFactory& operator=(
      const VivaldiInvalidationServiceFactory&) = delete;

  // Returned value may be nullptr in case if sync invalidations are disabled or
  // not supported.
  static invalidation::InvalidationService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static VivaldiInvalidationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<VivaldiInvalidationServiceFactory>;

  VivaldiInvalidationServiceFactory();
  ~VivaldiInvalidationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

}  // namespace vivaldi

#endif  // IOS_SYNC_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_
