// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_RULE_SERVICE_H_
#define COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_RULE_SERVICE_H_

#include <optional>
#include <string_view>

#include "base/observer_list_types.h"
#include "base/uuid.h"
#include "components/ad_blocker/public/content/adblock_tab_state_and_logs.h"
#include "components/ad_blocker/public/content/mojom/adblock_cosmetic_filter.mojom.h"
#include "components/ad_blocker/public/core/adblock_rule_service_core.h"
#include "components/ad_blocker/public/core/adblock_types.h"

#if BUILDFLAG(IS_IOS)
namespace web {
class BrowserState;
}
#else
namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content
#endif

namespace adblock_filter {
class RuleManager;
class KnownRuleSourcesHandler;
class StateAndLogs;
class CosmeticFilter;
class StatsStore;

class RuleService : public RuleServiceCore {
 public:
  class Client {
   public:
    virtual ~Client();
    virtual const std::optional<std::string_view>
    GetBrowserOwnedFrameUrlPrefix() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    virtual void OnRuleServiceStateLoaded(RuleService* rule_service) {}
    virtual void OnRulesIndexLoaded(RuleGroup group) {}
  };

  ~RuleService() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual bool HasDocumentActivationForRuleSource(
      adblock_filter::RuleGroup group,
      content::WebContents* web_contents,
      base::Uuid preset_id) = 0;

  // Helper method for building a new cosmetic filter..
  virtual std::unique_ptr<mojom::CosmeticFilter> MakeCosmeticFilter(
      content::RenderFrameHost* frame) = 0;

  virtual StateAndLogs* GetStateAndLogs() = 0;
  virtual StatsStore* GetStatsStore() = 0;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_PUBLIC_CONTENT_ADBLOCK_RULE_SERVICE_H_
