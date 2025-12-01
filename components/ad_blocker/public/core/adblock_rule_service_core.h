// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_CORE_ADBLOCK_RULE_SERVICE_CORE_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_CORE_ADBLOCK_RULE_SERVICE_CORE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace web {
class BrowserState;
}

namespace adblock_filter {
class RuleManager;
class KnownRuleSourcesHandler;

class RuleServiceCore : public KeyedService {
 public:
  ~RuleServiceCore() override;

  virtual bool IsLoaded() const = 0;

  virtual RuleManager* GetRuleManager() = 0;
  virtual KnownRuleSourcesHandler* GetKnownSourcesHandler() = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_CORE_ADBLOCK_RULE_SERVICE_CORE_H_
