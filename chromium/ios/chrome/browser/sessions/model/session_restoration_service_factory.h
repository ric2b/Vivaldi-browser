// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ChromeBrowserState;
class SessionRestorationService;

// Singleton that owns all SessionRestorationService and associates them with
// ChromeBrowserState.
class SessionRestorationServiceFactory final
    : public BrowserStateKeyedServiceFactory {
 public:
  // Represents the storage format that is requested.
  enum StorageFormat {
    kLegacy,
    kOptimized,
  };

  // Returns the instance of the service associated with `service`.
  static SessionRestorationService* GetForBrowserState(
      ChromeBrowserState* browser_state);

  static SessionRestorationServiceFactory* GetInstance();

  // Requests that session storage for `browser_state` is migrated if needed.
  // Invokes `closure` when the migration is complete. If data is already in
  // the correct format, `closure` is called synchronously.
  //
  // Must be called before GetForBrowserState() is called for `browser_state`.
  void MigrateSessionStorageFormat(ChromeBrowserState* browser_state,
                                   StorageFormat requested_format,
                                   base::OnceClosure closure);

 private:
  friend class base::NoDestructor<SessionRestorationServiceFactory>;

  SessionRestorationServiceFactory();
  ~SessionRestorationServiceFactory() final;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const final;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const final;
  void RegisterBrowserStatePrefs(
      user_prefs::PrefRegistrySyncable* registry) final;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_FACTORY_H_
