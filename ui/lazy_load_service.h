// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef UI_LAZY_LOAD_SERVICE_H_
#define UI_LAZY_LOAD_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "components/keyed_service/core/keyed_service.h"
class Profile;

namespace vivaldi {

class LazyLoadService
    : public KeyedService,
      public resource_coordinator::LifecycleUnitSourceObserver {
 public:
  explicit LazyLoadService(Profile* profile);
  ~LazyLoadService() override = default;

  static const char kLazyLoadIsSafe[];

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void OnLifecycleUnitCreated(
      resource_coordinator::LifecycleUnit* lifecycle_unit) override;

 private:
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(LazyLoadService);
};

}  // namespace vivaldi

#endif  // UI_LAZY_LOAD_SERVICE_H_
