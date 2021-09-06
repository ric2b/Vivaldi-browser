// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_
#define SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include "base/no_destructor.h"

class Profile;
namespace invalidation {
class InvalidationService;
}

namespace vivaldi {

class VivaldiInvalidationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  VivaldiInvalidationServiceFactory(const VivaldiInvalidationServiceFactory&) =
      delete;
  VivaldiInvalidationServiceFactory& operator=(
      const VivaldiInvalidationServiceFactory&) = delete;

  // Returned value may be nullptr in case if sync invalidations are disabled or
  // not supported.
  static invalidation::InvalidationService* GetForProfile(Profile* profile);

  static VivaldiInvalidationServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<VivaldiInvalidationServiceFactory>;

  VivaldiInvalidationServiceFactory();
  ~VivaldiInvalidationServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace vivaldi

#endif  // SYNC_INVALIDATION_VIVALDI_INVALIDATION_SERVICE_FACTORY_H_
