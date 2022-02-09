// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_MANAGER_H_
#define COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/request_filter/request_filter.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/completion_once_callback.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/websocket.mojom-forward.h"

class GURL;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace vivaldi {

struct FilteredRequestInfo;

// Manages filtering of requests. Lives on the UI thread. There is one instance
// per BrowserContext which is shared with incognito.
class RequestFilterManager : public KeyedService {
 public:
  using RequestFilterList = std::vector<std::unique_ptr<RequestFilter>>;

  // An interface which is held by ProxySet defined below.
  class Proxy {
   public:
    virtual ~Proxy() {}
  };

  // A ProxySet is a set of proxies used by the filter manager: It holds Proxy
  // instances, and removes all proxies when it is destroyed.
  class ProxySet {
   public:
    ProxySet();
    ~ProxySet();

    // Add a Proxy.
    void AddProxy(std::unique_ptr<Proxy> proxy);
    // Remove a Proxy. The removed proxy is deleted upon this call.
    void RemoveProxy(Proxy* proxy);

   private:
    std::set<std::unique_ptr<Proxy>, base::UniquePtrComparator> proxies_;

    DISALLOW_COPY_AND_ASSIGN(ProxySet);
  };

  class RequestIDGenerator {
   public:
    RequestIDGenerator();
    ~RequestIDGenerator();

    // Generates a WebRequest ID. If the same (routing_id,
    // network_service_request_id) pair is passed to this as was previously
    // passed to SaveID(), the |request_id| passed to SaveID() will be returned.
    int64_t Generate(int32_t routing_id, int32_t network_service_request_id);

    // This saves a WebRequest ID mapped to the (routing_id,
    // network_service_request_id) pair. Clients must call Generate() with the
    // same ID pair to retrieve the |request_id|, or else there may be a memory
    // leak.
    void SaveID(int32_t routing_id,
                int32_t network_service_request_id,
                uint64_t request_id);

   private:
    int64_t id_ = 0;
    std::map<std::pair<int32_t, int32_t>, uint64_t> saved_id_map_;
    DISALLOW_COPY_AND_ASSIGN(RequestIDGenerator);
  };

  class RequestHandler {
   public:
    // The events denoting the lifecycle of a given network request.
    enum EventTypes {
      kInvalidEvent = 0,
      kOnBeforeRequest = 1 << 0,
      kOnBeforeSendHeaders = 1 << 1,
      kOnSendHeaders = 1 << 2,
      kOnHeadersReceived = 1 << 3,
      kOnBeforeRedirect = 1 << 4,
      kOnResponseStarted = 1 << 5,
      kOnErrorOccurred = 1 << 6,
      kOnCompleted = 1 << 7,
    };

    explicit RequestHandler(RequestFilterManager* filter_manager);
    ~RequestHandler();

    bool WantsExtraHeadersForAnyRequest();
    bool WantsExtraHeadersForRequest(FilteredRequestInfo* request);

    // Dispatches the OnBeforeRequest event to all filters. Returns
    // net::ERR_IO_PENDING if a filter can't handle the request synchronously
    // and OK if the request should proceed normally. net::ERR_BLOCKED_BY_CLIENT
    // is returned if the request should be blocked.
    int OnBeforeRequest(content::BrowserContext* browser_context,
                        FilteredRequestInfo* request,
                        net::CompletionOnceCallback callback,
                        GURL* new_url,
                        bool* collapse);

    // Dispatches the onBeforeSendHeaders event. This is fired for HTTP(s)
    // requests only, and allows modification of the outgoing request headers.
    // Returns net::ERR_IO_PENDING if a filter can't handle the request
    // synchonously, OK otherwise.
    int OnBeforeSendHeaders(content::BrowserContext* browser_context,
                            const FilteredRequestInfo* request,
                            net::CompletionOnceCallback callback,
                            net::HttpRequestHeaders* headers,
                            std::set<std::string>* set_headers,
                            std::set<std::string>* removed_headers);

    // Dispatches the onSendHeaders event. This is fired for HTTP(s) requests
    // only.
    void OnSendHeaders(content::BrowserContext* browser_context,
                       const FilteredRequestInfo* request,
                       const net::HttpRequestHeaders& headers);

    // Dispatches the onHeadersReceived event. This is fired for HTTP(s)
    // requests only, and allows modification of incoming response headers.
    // Returns net::ERR_IO_PENDING if a filter can't handle the request
    // synchronously, OK otherwise. |original_response_headers| is reference
    // counted. |callback| |override_response_headers| and
    // |preserve_fragment_on_redirect_url| are not owned but are guaranteed to
    // be valid until |callback| is called or OnRequestWillBeDestroyed is called
    // (whatever comes first). Do not modify |original_response_headers|
    // directly but write new ones into |override_response_headers|.
    int OnHeadersReceived(
        content::BrowserContext* browser_context,
        const FilteredRequestInfo* request,
        net::CompletionOnceCallback callback,
        const net::HttpResponseHeaders* original_response_headers,
        scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
        GURL* preserve_fragment_on_redirect_url);

    // Dispatches the onBeforeRedirect event. This is fired for HTTP(s) requests
    // only.
    void OnBeforeRedirect(content::BrowserContext* browser_context,
                          const FilteredRequestInfo* request,
                          const GURL& new_location);

    // Dispatches the onResponseStarted event indicating that the first bytes of
    // the response have arrived.
    void OnResponseStarted(content::BrowserContext* browser_context,
                           const FilteredRequestInfo* request,
                           int net_error);

    // Dispatches the onComplete event.
    void OnCompleted(content::BrowserContext* browser_context,
                     const FilteredRequestInfo* request,
                     int net_error);

    // Dispatches an onErrorOccurred event.
    void OnErrorOccurred(content::BrowserContext* browser_context,
                         const FilteredRequestInfo* request,
                         bool started,
                         int net_error);

    // Notificaties when |request| is no longer being processed, regardless of
    // whether it has gone to completion or merely been cancelled. This is
    // guaranteed to be called eventually for any request observed by this
    // object, and |*request| will be immintently destroyed after this returns.
    void OnRequestWillBeDestroyed(content::BrowserContext* browser_context,
                                  const FilteredRequestInfo* request);

   private:
    struct PendingRequest;

    void OnBeforeRequestHandled(int64_t request_id,
                                size_t filter_priorty,
                                bool cancel,
                                bool collapse,
                                const GURL& new_url);

    void OnBeforeSendHeadersHandled(
        int64_t request_id,
        size_t filter_priorty,
        bool cancel,
        RequestFilter::RequestHeaderChanges header_changes);

    void MergeRequestHeaderChanges(PendingRequest* pending_request);

    void OnHeadersReceivedHandled(
        int64_t request_id,
        size_t filter_priorty,
        bool cancel,
        const GURL& new_url,
        RequestFilter::ResponseHeaderChanges header_changes);

    void MergeResponseHeaderChanges(PendingRequest* pending_request);

    // Sets the flag that |event_type| has been signaled for |request_id|.
    // Returns the value of the flag before setting it.
    bool GetAndSetSignaled(uint64_t request_id, EventTypes event_type);

    // Clears the flag that |event_type| has been signaled for |request_id|.
    void ClearSignaled(uint64_t request_id, EventTypes event_type);

    using PendingRequestMap = std::map<uint64_t, PendingRequest>;
    // Map of request_id -> bit vector of EventTypes already signaled
    using SignaledRequestMap = std::map<uint64_t, int>;

    RequestFilterManager* filter_manager_;

    // A map of network requests that are waiting for at least one filter
    // to respond.
    PendingRequestMap pending_requests_;

    // A map of request ids to a bitvector indicating which events have been
    // signaled and should not be sent again.
    SignaledRequestMap signaled_requests_;
  };

  explicit RequestFilterManager(content::BrowserContext* context);
  ~RequestFilterManager() override;

  // Swaps out |*factory_request| for a new request which proxies through an
  // internal URLLoaderFactory. This supports lifetime observation and control
  // for the purpose of filtering.
  // |frame| and |render_process_id| are the frame and render process id in
  // which the URLLoaderFactory will be used. |frame| can be nullptr for
  // factories proxied for service worker.
  bool ProxyURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      absl::optional<int64_t> navigation_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          forwarding_header_client);

  static void ProxiedProxyWebSocket(
      content::BrowserContext* context,
      int process_id,
      int frame_id,
      const url::Origin& frame_origin,
      content::ContentBrowserClient::WebSocketFactory factory,
      const GURL& site_for_cookies,
      const absl::optional<std::string>& user_agent,
      const GURL& url,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          authentication_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client);

  // Starts proxying the connection with |factory|.
  void ProxyWebSocket(
      int process_id,
      int frame_id,
      const url::Origin& frame_origin,
      content::ContentBrowserClient::WebSocketFactory factory,
      const GURL& site_for_cookies,
      const absl::optional<std::string>& user_agent,
      const GURL& url,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          authentication_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client);

  void AddFilter(std::unique_ptr<RequestFilter> new_filter);
  void RemoveFilter(RequestFilter* filter);

  void ClearCacheOnNavigation();

  void Shutdown() override;

 private:
  content::BrowserContext* const browser_context_;

  RequestIDGenerator request_id_generator_;
  std::unique_ptr<ProxySet> proxies_;

  RequestFilterList request_filters_;

  RequestHandler request_handler_;

  DISALLOW_COPY_AND_ASSIGN(RequestFilterManager);
};
}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_MANAGER_H_
