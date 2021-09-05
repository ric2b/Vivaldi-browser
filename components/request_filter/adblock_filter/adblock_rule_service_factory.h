// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_FACTORY_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace adblock_filter {

class RuleService;

class RuleServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RuleService* GetForBrowserContext(content::BrowserContext* context);
  static RuleServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RuleServiceFactory>;

  RuleServiceFactory();
  ~RuleServiceFactory() override;

  // BrowserContextKeyedBaseFactory methods:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(RuleServiceFactory);
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_FACTORY_H_
