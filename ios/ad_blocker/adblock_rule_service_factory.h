// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_FACTORY_H_
#define IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace adblock_filter {

class RuleService;

class RuleServiceFactory : public BrowserStateKeyedServiceFactory {
 public:
  static RuleService* GetForProfile(ProfileIOS* profile);
  static RuleService* GetForProfileIfExists(
      ProfileIOS* profile);
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
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* browser_state) const override;
};

}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_RULE_SERVICE_FACTORY_H_
