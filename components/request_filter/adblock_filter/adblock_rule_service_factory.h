// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_FACTORY_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace adblock_filter {

class RuleServiceContent;

class RuleServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RuleServiceContent* GetForBrowserContext(
      content::BrowserContext* context);
  static RuleServiceFactory* GetInstance();

 private:
  friend base::DefaultSingletonTraits<RuleServiceFactory>;

  RuleServiceFactory();
  ~RuleServiceFactory() override;
  RuleServiceFactory(const RuleServiceFactory&) = delete;
  RuleServiceFactory& operator=(const RuleServiceFactory&) = delete;

  // BrowserContextKeyedBaseFactory methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_FACTORY_H_
