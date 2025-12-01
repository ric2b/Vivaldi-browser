// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_REQUEST_FILTER_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_REQUEST_FILTER_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "components/request_filter/request_filter.h"

namespace content {
class RenderFrameHost;
}

namespace adblock_filter {
class RuleServiceImpl;

class AdBlockRequestFilter : public vivaldi::RequestFilter {
 public:
  AdBlockRequestFilter(base::WeakPtr<RuleServiceImpl> rule_service,
                       RuleGroup group);
  ~AdBlockRequestFilter() override;
  AdBlockRequestFilter(const AdBlockRequestFilter&) = delete;
  AdBlockRequestFilter& operator=(const AdBlockRequestFilter&) = delete;

  void set_allow_blocking_documents(bool allow) {
    allow_blocking_documents_ = allow;
  }

  void set_block_pings(bool block_pings) { block_pings_ = block_pings; }

  void OnIndexLoaded();

  // Implementing vivaldi::RequestFilter
  bool WantsExtraHeadersForAnyRequest() const override;
  bool WantsExtraHeadersForRequest(
      vivaldi::FilteredRequestInfo* request) const override;
  bool OnBeforeRequest(content::BrowserContext* browser_context,
                       const vivaldi::FilteredRequestInfo* request,
                       BeforeRequestCallback callback) override;
  bool OnBeforeSendHeaders(content::BrowserContext* browser_context,
                           const vivaldi::FilteredRequestInfo* request,
                           const net::HttpRequestHeaders* headers,
                           BeforeSendHeadersCallback callback) override;
  void OnSendHeaders(content::BrowserContext* browser_context,
                     const vivaldi::FilteredRequestInfo* request,
                     const net::HttpRequestHeaders& headers) override;
  bool OnHeadersReceived(content::BrowserContext* browser_context,
                         const vivaldi::FilteredRequestInfo* request,
                         const net::HttpResponseHeaders* headers,
                         HeadersReceivedCallback callback) override;
  void OnBeforeRedirect(content::BrowserContext* browser_context,
                        const vivaldi::FilteredRequestInfo* request,
                        const GURL& redirect_url) override;
  void OnResponseStarted(content::BrowserContext* browser_context,
                         const vivaldi::FilteredRequestInfo* request) override;
  void OnCompleted(content::BrowserContext* browser_context,
                   const vivaldi::FilteredRequestInfo* request) override;
  void OnErrorOccured(content::BrowserContext* browser_context,
                      const vivaldi::FilteredRequestInfo* request,
                      int net_error) override;

 private:
  bool DoesAdAttributionMatch(content::RenderFrameHost* frame,
                              std::string_view tracker_url_spec,
                              std::string_view ad_domain_and_query_trigger);
  base::WeakPtr<RuleServiceImpl> rule_service_;

  RuleGroup group_;
  bool allow_blocking_documents_ = false;
  bool block_pings_ = false;

  std::vector<base::OnceClosure> pending_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_REQUEST_FILTER_H_
