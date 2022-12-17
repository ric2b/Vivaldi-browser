// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter.h"

namespace vivaldi {

class PingBlockerFilter : public vivaldi::RequestFilter {
 public:
  PingBlockerFilter();
  ~PingBlockerFilter() override = default;
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
};

}  // namespace vivaldi
