// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/interstitial/document_blocked_throttle.h"

#include <optional>

#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_tab_state_and_logs.h"
#include "components/request_filter/adblock_filter/interstitial/document_blocked_controller_client.h"
#include "components/request_filter/adblock_filter/interstitial/document_blocked_interstitial.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"

namespace adblock_filter {
DocumentBlockedThrottle::DocumentBlockedThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

DocumentBlockedThrottle::~DocumentBlockedThrottle() = default;

const char* DocumentBlockedThrottle::GetNameForLogging() {
  return "DocumentBlockedThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DocumentBlockedThrottle::WillFailRequest() {
  if (navigation_handle()->GetNetErrorCode() != net::ERR_BLOCKED_BY_CLIENT ||
      !navigation_handle()->IsInMainFrame())
    return content::NavigationThrottle::PROCEED;

  const GURL& url = navigation_handle()->GetURL();
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  auto* service = RuleServiceFactory::GetForBrowserContext(
      web_contents->GetBrowserContext());

  std::array<std::optional<TabStateAndLogs::RuleData>, kRuleGroupCount>
      blocking_rules =
          service->IsDocumentBlocked(navigation_handle()->GetRenderFrameHost());
  std::optional<TabStateAndLogs::RuleData> blocking_rule;
  std::optional<RuleGroup> blocking_group;
  for (auto group : {RuleGroup::kAdBlockingRules, RuleGroup::kTrackingRules}) {
    blocking_rule = std::move(blocking_rules[static_cast<size_t>(group)]);
    if (blocking_rule != std::nullopt) {
      blocking_group = group;
      break;
    }
  }
  if (blocking_rule == std::nullopt || blocking_group == std::nullopt)
    return content::NavigationThrottle::PROCEED;

  std::optional<ActiveRuleSource> rule_source =
      service->GetRuleManager()->GetRuleSource(*blocking_group,
                                               blocking_rule->rule_source_id);

  std::string rule_source_name = "Unloaded rule source";
  GURL rule_source_link = GURL("#");

  if (rule_source) {
    rule_source_name = rule_source->unsafe_adblock_metadata.title;
    if (rule_source_name.empty()) {
      rule_source_name = rule_source->core.is_from_url()
                             ? rule_source->core.source_url().spec()
                             : rule_source->core.source_file().AsUTF8Unsafe();
    }

    rule_source_link = rule_source->unsafe_adblock_metadata.homepage;
    if (!rule_source_link.is_valid()) {
      rule_source_link = rule_source->core.is_from_url()
                             ? rule_source->core.source_url()
                             : GURL(std::string("file://") +
                                    rule_source->core.source_file()
                                        .NormalizePathSeparatorsTo('/')
                                        .AsUTF8Unsafe());
    }
  }

  auto controller =
      std::make_unique<DocumentBlockedControllerClient>(web_contents, url);

  std::unique_ptr<DocumentBlockedInterstitial> blocking_page(
      new DocumentBlockedInterstitial(
          web_contents, url, *blocking_group, blocking_rule->rule_text,
          rule_source_name, rule_source_link, std::move(controller)));

  std::optional<std::string> error_page_contents =
      blocking_page->GetHTMLContents();

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle(), std::move(blocking_page));

  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_contents);
}
}  // namespace adblock_filter