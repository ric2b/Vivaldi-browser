// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_

#import <memory>

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

namespace syncer {
class UserEventService;
}  // namespace syncer

// Singleton that associates UserEventServices to ChromeBrowserStates.
class IOSUserEventServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  // TODO(crbug.com/358301380): remove this method.
  static syncer::UserEventService* GetForBrowserState(ProfileIOS* profile);

  static syncer::UserEventService* GetForProfile(ProfileIOS* profile);
  static IOSUserEventServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSUserEventServiceFactory>;

  IOSUserEventServiceFactory();
  ~IOSUserEventServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_IOS_USER_EVENT_SERVICE_FACTORY_H_
