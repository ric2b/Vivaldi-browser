// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef _SYNC_VIVALDI_INVALIDATION_SERVICE_H
#define _SYNC_VIVALDI_INVALIDATION_SERVICE_H

#include "components/invalidation/impl/invalidator_registrar.h"
#include "components/invalidation/public/invalidation_service.h"

class Profile;
class ProfileIdentityProvider;

namespace invalidation {
class InvalidationLogger;
}

namespace vivaldi {

class VivaldiInvalidationService : public invalidation::InvalidationService {
 public:
  VivaldiInvalidationService(Profile *);
  ~VivaldiInvalidationService() override;

  void PerformInvalidation(const syncer::ObjectIdInvalidationMap&);

  // InvalidationService
  void RegisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  bool UpdateRegisteredInvalidationIds(syncer::InvalidationHandler* handler,
                                       const syncer::ObjectIdSet& ids) override;
  void UnregisterInvalidationHandler(
      syncer::InvalidationHandler* handler) override;
  syncer::InvalidatorState GetInvalidatorState() const override;
  std::string GetInvalidatorClientId() const override;
  invalidation::InvalidationLogger* GetInvalidationLogger() override;
  void RequestDetailedStatus(base::Callback<void(const base::DictionaryValue&)>
                                 post_caller) const override;
  IdentityProvider* GetIdentityProvider() override;

 private:
  std::string client_id_;
  syncer::InvalidatorRegistrar invalidator_registrar_;
  std::unique_ptr<ProfileIdentityProvider> identity_provider_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiInvalidationService);
};

}  // namepace vivaldi

#endif  // _SYNC_VIVALDI_INVALIDATION_SERVICE_H
