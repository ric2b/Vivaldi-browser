// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/interstitial/document_blocked_throttle.h"

#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
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

  std::optional<RuleGroup> blocking_group;
  if (service->IsDocumentBlocked(RuleGroup::kTrackingRules,
                                 navigation_handle()->GetRenderFrameHost(),
                                 url))
    blocking_group = RuleGroup::kTrackingRules;
  else if (service->IsDocumentBlocked(RuleGroup::kAdBlockingRules,
                                      navigation_handle()->GetRenderFrameHost(),
                                      url))
    blocking_group = RuleGroup::kAdBlockingRules;
  if (!blocking_group)
    return content::NavigationThrottle::PROCEED;

  auto controller =
      std::make_unique<DocumentBlockedControllerClient>(web_contents, url);

  std::unique_ptr<DocumentBlockedInterstitial> blocking_page(
      new DocumentBlockedInterstitial(web_contents, url, blocking_group.value(),
                                      std::move(controller)));

  std::optional<std::string> error_page_contents =
      blocking_page->GetHTMLContents();

  security_interstitials::SecurityInterstitialTabHelper::AssociateBlockingPage(
      navigation_handle(), std::move(blocking_page));

  return content::NavigationThrottle::ThrottleCheckResult(
      CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_contents);
}
}  // namespace adblock_filter