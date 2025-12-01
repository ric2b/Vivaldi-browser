// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_AD_BLOCKER_ADBLOCK_RULE_SERVICE_CLIENT_H_
#define BROWSER_AD_BLOCKER_ADBLOCK_RULE_SERVICE_CLIENT_H_

#include <optional>
#include <string_view>

#include "components/ad_blocker/public/content/adblock_rule_service.h"

namespace vivaldi {

class AdblockRuleServiceClient : public adblock_filter::RuleService::Client {
 public:
  AdblockRuleServiceClient();
  ~AdblockRuleServiceClient() override;

  AdblockRuleServiceClient(const AdblockRuleServiceClient&) = delete;
  AdblockRuleServiceClient& operator=(const AdblockRuleServiceClient&) = delete;

  const std::optional<std::string_view> GetBrowserOwnedFrameUrlPrefix()
      override;
};

}  // namespace vivaldi

#endif  //  BROWSER_AD_BLOCKER_ADBLOCK_RULE_SERVICE_CLIENT_H_
