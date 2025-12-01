// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_FACTORY_H_
#define IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "ios/chrome/browser/shared/model/profile/profile_keyed_service_factory_ios.h"

class ProfileIOS;

namespace adblock_filter {

class RuleService;

class RuleServiceFactory : public ProfileKeyedServiceFactoryIOS {
 public:
  static RuleService* GetForProfile(ProfileIOS* profile);
  static RuleService* GetForProfileIfExists(ProfileIOS* profile);
  static RuleServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<RuleServiceFactory>;

  RuleServiceFactory();
  ~RuleServiceFactory() override;
  RuleServiceFactory(const RuleServiceFactory&) = delete;
  RuleServiceFactory& operator=(const RuleServiceFactory&) = delete;

  // BrowserStateKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* browser_state) const override;
};

}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_FACTORY_H_
