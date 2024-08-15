// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_CONTENT_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_CONTENT_H_

#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/ad_blocker/adblock_types.h"
#include "components/ad_blocker/adblock_rule_service.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

namespace adblock_filter {
class RuleServiceContent : public RuleService {
 public:
  ~RuleServiceContent() override;

  // Check if a given document |url| is blocked to determine whether to show
  // an interstiatial in the given |frame|.
  virtual bool IsDocumentBlocked(RuleGroup group,
                                 content::RenderFrameHost* frame,
                                 const GURL& url) const = 0;

  // Helper method for setting up a new cosmetic filter with the needed indexes.
  virtual void InitializeCosmeticFilter(CosmeticFilter* filter) = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULE_SERVICE_CONTENT_H_
