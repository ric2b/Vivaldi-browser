// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_REQUEST_FILTER_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_REQUEST_FILTER_H_

#include "base/functional/callback.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/core/adblock_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/request_filter/request_filter.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {
class RenderFrameHost;
}

namespace adblock_filter {
class RuleServiceImpl;

class AdBlockRequestFilter : public vivaldi::RequestFilter,
                             RuleService::Observer {
 public:
  AdBlockRequestFilter(base::WeakPtr<RuleServiceImpl> rule_service,
                       RuleGroup group,
                       PrefService* prefs);
  ~AdBlockRequestFilter() override;
  AdBlockRequestFilter(const AdBlockRequestFilter&) = delete;
  AdBlockRequestFilter& operator=(const AdBlockRequestFilter&) = delete;

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
  void OnRequestWillBeDestroyed(
      content::BrowserContext* browser_context,
      const vivaldi::FilteredRequestInfo* request) override;

 private:
  void OnEnableDocumentBlockingChanged();
  void OnPingBlockingChanged();

  // Implementing RuleService::Observer
  void OnRulesIndexLoaded(RuleGroup group) override;

  base::WeakPtr<RuleServiceImpl> rule_service_;

  PrefChangeRegistrar pref_change_registrar_;
  RuleGroup group_;
  bool allow_blocking_documents_ = false;
  bool block_pings_ = false;

  absl::flat_hash_map<uint64_t, base::OnceClosure> pending_;
  base::WeakPtrFactory<AdBlockRequestFilter> weak_factory_{this};
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_REQUEST_FILTER_H_
