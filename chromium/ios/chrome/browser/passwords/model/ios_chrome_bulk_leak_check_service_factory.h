// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_otr_helper.h"

class ProfileIOS;
enum class ServiceAccessType;

namespace password_manager {
class BulkLeakCheckServiceInterface;
}

// Singleton that owns all BulkLeakCheckServices and associates them with
// profile.
class IOSChromeBulkLeakCheckServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::BulkLeakCheckServiceInterface* GetForProfile(
      ProfileIOS* profile);
  static IOSChromeBulkLeakCheckServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<IOSChromeBulkLeakCheckServiceFactory>;

  IOSChromeBulkLeakCheckServiceFactory();
  ~IOSChromeBulkLeakCheckServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_IOS_CHROME_BULK_LEAK_CHECK_SERVICE_FACTORY_H_
