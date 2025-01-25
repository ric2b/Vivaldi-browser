// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_DIRECT_MATCH_DIRECT_MATCH_SERVICE_FACTORY_H_
#define IOS_DIRECT_MATCH_DIRECT_MATCH_SERVICE_FACTORY_H_

#import "base/no_destructor.h"
#import "components/keyed_service/ios/browser_state_keyed_service_factory.h"

class ProfileIOS;

namespace direct_match {

class DirectMatchService;

class DirectMatchServiceFactory : public BrowserStateKeyedServiceFactory {
public:
  static DirectMatchService* GetForProfile(ProfileIOS* profile);
  static DirectMatchService* GetForProfileIfExists(
      ProfileIOS* profile);
  static DirectMatchServiceFactory* GetInstance();

private:
  friend class base::NoDestructor<DirectMatchServiceFactory>;

  DirectMatchServiceFactory();
  ~DirectMatchServiceFactory() override;
  DirectMatchServiceFactory(const DirectMatchServiceFactory&) = delete;
  DirectMatchServiceFactory& operator=(const DirectMatchServiceFactory&) = delete;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
       web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
       web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace direct_match

#endif  // IOS_DIRECT_MATCH_DIRECT_MATCH_SERVICE_FACTORY_H_
