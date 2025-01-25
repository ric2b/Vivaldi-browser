// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_H_
#define COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/request_filter/filtered_request_info.h"

namespace content {
class BrowserContext;
}

namespace vivaldi {

class RequestFilter {
 public:
  struct RequestHeaderChanges {
   public:
    RequestHeaderChanges();
    ~RequestHeaderChanges();
    RequestHeaderChanges(RequestHeaderChanges&&);
    RequestHeaderChanges& operator=(RequestHeaderChanges&&);

    net::HttpRequestHeaders headers_to_modify;
    std::set<std::string> headers_to_remove;
  };

  using ResponseHeader = std::pair<std::string, std::string>;
  using ResponseHeaders = std::vector<ResponseHeader>;
  struct ResponseHeaderChanges {
   public:
    ResponseHeaderChanges();
    ~ResponseHeaderChanges();
    ResponseHeaderChanges(ResponseHeaderChanges&&);
    ResponseHeaderChanges& operator=(ResponseHeaderChanges&&);

    ResponseHeaders headers_to_remove;
    ResponseHeaders headers_to_add;
  };

  // Types are sorted by order of priority. Higher value = higher priority;
  enum Type {
    kAdBlock = 0,
    kPingBlock = 1,
  };

  enum CancelDecision { kAllow = 0, kCancel, kPreventCancel };

  RequestFilter(Type type, int priority);
  virtual ~RequestFilter();

  Type type() { return type_; }

  int priority() { return priority_; }

  virtual bool WantsExtraHeadersForAnyRequest() const = 0;
  virtual bool WantsExtraHeadersForRequest(
      FilteredRequestInfo* request) const = 0;

  using BeforeRequestCallback = base::OnceCallback<
      void(CancelDecision cancel, bool collapse, const GURL& new_url)>;
  virtual bool OnBeforeRequest(content::BrowserContext* browser_context,
                               const FilteredRequestInfo* request,
                               BeforeRequestCallback callback) = 0;

  using BeforeSendHeadersCallback =
      base::OnceCallback<void(CancelDecision cancel,
                              RequestHeaderChanges header_changes)>;
  virtual bool OnBeforeSendHeaders(content::BrowserContext* browser_context,
                                   const FilteredRequestInfo* request,
                                   const net::HttpRequestHeaders* headers,
                                   BeforeSendHeadersCallback callback) = 0;

  virtual void OnSendHeaders(content::BrowserContext* browser_context,
                             const FilteredRequestInfo* request,
                             const net::HttpRequestHeaders& headers) = 0;

  using HeadersReceivedCallback =
      base::OnceCallback<void(CancelDecision cancel,
                              // Ignored when responding asynchronously
                              bool collapse,
                              const GURL& new_url,
                              ResponseHeaderChanges header_changes)>;

  virtual bool OnHeadersReceived(content::BrowserContext* browser_context,
                                 const FilteredRequestInfo* request,
                                 const net::HttpResponseHeaders* headers,
                                 HeadersReceivedCallback callback) = 0;

  virtual void OnBeforeRedirect(content::BrowserContext* browser_context,
                                const FilteredRequestInfo* request,
                                const GURL& redirect_url) = 0;

  virtual void OnResponseStarted(content::BrowserContext* browser_context,
                                 const FilteredRequestInfo* request) = 0;

  virtual void OnCompleted(content::BrowserContext* browser_context,
                           const FilteredRequestInfo* request) = 0;

  virtual void OnErrorOccured(content::BrowserContext* browser_context,
                              const FilteredRequestInfo* request,
                              int net_error) = 0;

 private:
  const Type type_;
  const int priority_;
};
}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_H_
