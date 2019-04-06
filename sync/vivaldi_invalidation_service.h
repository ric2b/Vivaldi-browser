// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_VIVALDI_INVALIDATION_SERVICE_H_
#define SYNC_VIVALDI_INVALIDATION_SERVICE_H_

#include <memory>
#include <string>

#include "components/invalidation/impl/invalidator_registrar.h"
#include "components/invalidation/public/invalidation_service.h"

class Profile;

namespace invalidation {
class InvalidationLogger;
}

namespace vivaldi {

class VivaldiInvalidationService : public invalidation::InvalidationService {
 public:
  explicit VivaldiInvalidationService(Profile*);
  ~VivaldiInvalidationService() override;

  void PerformInvalidation(const syncer::ObjectIdInvalidationMap&);
  void UpdateInvalidatorState(syncer::InvalidatorState state);

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

 private:
  std::string client_id_;
  syncer::InvalidatorRegistrar invalidator_registrar_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiInvalidationService);
};

}  // namespace vivaldi

#endif  // SYNC_VIVALDI_INVALIDATION_SERVICE_H_
