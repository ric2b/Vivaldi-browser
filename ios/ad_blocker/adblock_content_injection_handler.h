// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_ADBLOCK_CONTENT_INJECTION_HANDLER_H_
#define IOS_AD_BLOCKER_ADBLOCK_CONTENT_INJECTION_HANDLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/ad_blocker/adblock_resources.h"
#include "components/ad_blocker/adblock_types.h"

namespace web {
class BrowserState;
}

namespace adblock_filter {

// A provider class that handles compiling and configuring Content Blocker
// rules.
class ContentInjectionHandler {
 public:
  static std::unique_ptr<ContentInjectionHandler> Create(
      web::BrowserState* browser_state,
      Resources* resources);
  virtual ~ContentInjectionHandler();

  virtual void SetIncognitoBrowserState(web::BrowserState* browser_state) = 0;
  virtual void SetScriptletInjectionRules(
      RuleGroup group,
      base::Value::Dict injection_rules) = 0;
};

}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_ADBLOCK_CONTENT_INJECTION_HANDLER_H_
