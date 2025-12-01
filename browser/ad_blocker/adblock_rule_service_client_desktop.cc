// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "browser/ad_blocker/adblock_rule_service_client.h"

#include "app/vivaldi_constants.h"

#include <optional>
#include <string_view>

namespace vivaldi {
AdblockRuleServiceClient::AdblockRuleServiceClient() = default;
AdblockRuleServiceClient::~AdblockRuleServiceClient() = default;

const std::optional<std::string_view>
AdblockRuleServiceClient::GetBrowserOwnedFrameUrlPrefix() {
  return kVivaldiAppURLDomain;
}
}  // namespace vivaldi
