// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef UI_LAZY_LOAD_SERVICE_H_
#define UI_LAZY_LOAD_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/sessions/session_restore.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace vivaldi {

class LazyLoadService
    : public KeyedService,
      public resource_coordinator::LifecycleUnitSourceObserver,
      public SessionRestoreObserver {
 public:
  explicit LazyLoadService(Profile* profile);
  ~LazyLoadService() override = default;
  LazyLoadService(const LazyLoadService&) = delete;
  LazyLoadService& operator=(const LazyLoadService&) = delete;

  static const char kLazyLoadIsSafe[];

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  // LifecycleUnitSourceObserver implementation:
  void OnLifecycleUnitCreated(
      resource_coordinator::LifecycleUnit* lifecycle_unit) override;

  // SessionRestoreObserver implementation:
  void OnWillRestoreTab(content::WebContents* web_contents) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace vivaldi

#endif  // UI_LAZY_LOAD_SERVICE_H_
