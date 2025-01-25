// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter_manager.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/not_fatal_until.h"
#include "chrome/browser/profiles/profile.h"
#include "components/request_filter/request_filter_manager_factory.h"
#include "components/request_filter/request_filter_proxying_url_loader_factory.h"
#include "components/request_filter/request_filter_proxying_websocket.h"
#include "components/request_filter/request_filter_proxying_webtransport.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "net/cookies/site_for_cookies.h"

using content::BrowserThread;
using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;

namespace vivaldi {

namespace {

RequestFilter::ResponseHeader ToLowerCase(
    const RequestFilter::ResponseHeader& header) {
  return RequestFilter::ResponseHeader(base::ToLowerASCII(header.first),
                                       header.second);
}

}  // namespace

RequestFilterManager::ProxySet::ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

RequestFilterManager::ProxySet::~ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RequestFilterManager::ProxySet::AddProxy(std::unique_ptr<Proxy> proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  proxies_.insert(std::move(proxy));
}

void RequestFilterManager::ProxySet::RemoveProxy(Proxy* proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto proxy_it = proxies_.find(proxy);
  CHECK(proxy_it != proxies_.end(), base::NotFatalUntil::M130);
  proxies_.erase(proxy_it);
}

RequestFilterManager::RequestIDGenerator::RequestIDGenerator() = default;
RequestFilterManager::RequestIDGenerator::~RequestIDGenerator() = default;

int64_t RequestFilterManager::RequestIDGenerator::Generate(
    int32_t routing_id,
    int32_t network_service_request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = saved_id_map_.find({routing_id, network_service_request_id});
  if (it != saved_id_map_.end()) {
    int64_t id = it->second;
    saved_id_map_.erase(it);
    return id;
  }
  return ++id_;
}

void RequestFilterManager::RequestIDGenerator::SaveID(
    int32_t routing_id,
    int32_t network_service_request_id,
    uint64_t request_id) {
  // If |network_service_request_id| is 0, we cannot reliably match the
  // generated ID to a future request, so ignore it.
  if (network_service_request_id != 0) {
    saved_id_map_.insert(
        {{routing_id, network_service_request_id}, request_id});
  }
}

RequestFilterManager::RequestFilterManager(content::BrowserContext* context)
    : browser_context_(context),
      proxies_(std::make_unique<ProxySet>()),
      request_handler_(this) {}

RequestFilterManager::~RequestFilterManager() = default;

void RequestFilterManager::Shutdown() {
  proxies_.reset();
}

void RequestFilterManager::AddFilter(
    std::unique_ptr<RequestFilter> new_filter) {
  auto filter_it = request_filters_.begin();
  for (; filter_it != request_filters_.end(); filter_it++) {
    if ((*filter_it)->type() > new_filter->type() ||
        ((*filter_it)->type() == new_filter->type() &&
         (*filter_it)->priority() > new_filter->priority()))
      break;
  }
  request_filters_.insert(filter_it, std::move(new_filter));
  ClearCacheOnNavigation();
}

void RequestFilterManager::RemoveFilter(RequestFilter* filter) {
  request_filters_.erase(std::find_if(request_filters_.begin(),
                                      request_filters_.end(),
                                      [filter](const auto& request_filter) {
                                        return request_filter.get() == filter;
                                      }));
  ClearCacheOnNavigation();
}

void RequestFilterManager::ClearCacheOnNavigation() {
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

bool RequestFilterManager::ProxyURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    std::optional<int64_t> navigation_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        forwarding_header_client,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver;

  if (header_client)
    header_client_receiver = header_client->InitWithNewPipeAndPassReceiver();

  // NOTE: This request may be proxied on behalf of an incognito frame, but
  // |this| will always be bound to a regular profile (see
  // |BrowserContextKeyedAPI::kServiceRedirectedInIncognito|).
  DCHECK(browser_context == browser_context_ ||
         (browser_context->IsOffTheRecord() &&
          static_cast<Profile*>(browser_context)->GetOriginalProfile() ==
              browser_context_));
  RequestFilterProxyingURLLoaderFactory::StartProxying(
      browser_context, frame ? frame->GetProcess()->GetID() : render_process_id,
      frame ? frame->GetRoutingID() : MSG_ROUTING_NONE,
      frame ? frame->GetRenderViewHost()->GetRoutingID() : MSG_ROUTING_NONE,
      &request_handler_, &request_id_generator_, std::move(navigation_id),
      factory_builder, std::move(header_client_receiver),
      std::move(forwarding_header_client), proxies_.get(), type,
      std::move(navigation_response_task_runner));
  return true;
}

/*static*/
void RequestFilterManager::ProxiedProxyWebSocket(
    content::BrowserContext* context,
    int process_id,
    int frame_id,
    const url::Origin& frame_origin,
    content::ContentBrowserClient::WebSocketFactory factory,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
        authentication_handler,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client) {
  auto* request_filter_manager =
      vivaldi::RequestFilterManagerFactory::GetForBrowserContext(context);
  request_filter_manager->ProxyWebSocket(
      process_id, frame_id, frame_origin, std::move(factory), site_for_cookies,
      user_agent, url, std::move(additional_headers),
      std::move(handshake_client), std::move(authentication_handler),
      std::move(header_client));
}

void RequestFilterManager::ProxyWebSocket(
    int process_id,
    int frame_id,
    const url::Origin& frame_origin,
    content::ContentBrowserClient::WebSocketFactory factory,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
        authentication_handler,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const bool has_extra_headers =
      request_handler_.WantsExtraHeadersForAnyRequest();

  RequestFilterProxyingWebSocket::StartProxying(
      std::move(factory), site_for_cookies, user_agent, url,
      std::move(additional_headers), std::move(handshake_client),
      std::move(authentication_handler), std::move(header_client),
      has_extra_headers, process_id, frame_id, &request_id_generator_,
      &request_handler_, frame_origin, browser_context_, proxies_.get());
}

/*static*/
void RequestFilterManager::ProxiedProxyWebTransport(
    int process_id,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    std::optional<network::mojom::WebTransportErrorPtr> error) {
  if (error) {
    std::move(callback).Run(std::move(handshake_client), std::move(error));
    return;
  }
  auto* render_process_host = content::RenderProcessHost::FromID(process_id);
  if (!render_process_host) {
    std::move(callback).Run(std::move(handshake_client), std::nullopt);
    return;
  }
  auto* request_filter_manager =
      vivaldi::RequestFilterManagerFactory::GetForBrowserContext(
          render_process_host->GetBrowserContext());

  request_filter_manager->ProxyWebTransport(
      *render_process_host, frame_routing_id, url, initiator_origin,
      std::move(callback), std::move(handshake_client));
}

void RequestFilterManager::ProxyWebTransport(
    content::RenderProcessHost& render_process_host,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client) {
  StartWebRequestProxyingWebTransport(
      render_process_host, frame_routing_id, url, initiator_origin,
      std::move(handshake_client),
      request_id_generator_.Generate(MSG_ROUTING_NONE, 0), &request_handler_,
      *proxies_.get(), std::move(callback));
}

struct RequestFilterManager::RequestHandler::PendingRequest {
  PendingRequest() = default;

  // Information about the request that is being blocked. Not owned.
  raw_ptr<const FilteredRequestInfo> request = nullptr;

  // The number of event handlers that we are awaiting a response from.
  int num_filters_blocking = 0;

  // The callback to call when we get a response from all event handlers.
  net::CompletionOnceCallback callback;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest and OnHeadersReceived.
  raw_ptr<GURL> new_url = nullptr;

  // Priority of the filter which set the new URL. Only filters with a higher
  // priority can change it again.
  size_t new_url_priority = 0;

  RequestFilter::CancelDecision cancel;

  raw_ptr<bool> collapse;

  // The request headers that will be issued along with this request. Only valid
  // for OnBeforeSendHeaders.
  raw_ptr<net::HttpRequestHeaders> request_headers = nullptr;

  // The list of headers that have been modified/changed while handling
  // OnBeforeSendHeaders
  raw_ptr<std::set<std::string>> set_request_headers = nullptr;
  raw_ptr<std::set<std::string>> removed_request_headers = nullptr;

  // The response headers that were received from the server and subsequently
  // filtered by the Declarative Net Request API. Only valid for
  // OnHeadersReceived.
  raw_ptr<const net::HttpResponseHeaders> original_response_headers;

  // Location where to override response headers. Only valid for
  // OnHeadersReceived.
  raw_ptr<scoped_refptr<net::HttpResponseHeaders>> override_response_headers =
      nullptr;

  // The request headers to be modified for each filter. Used during
  // OnBeforeSendHeaders.
  std::vector<RequestFilter::RequestHeaderChanges> all_request_header_changes;

  // The reponse headers to be modified for each filter. Used during
  // OnHeadersReceived.
  std::vector<RequestFilter::ResponseHeaderChanges> all_response_header_changes;
};

RequestFilterManager::RequestHandler::RequestHandler(
    RequestFilterManager* filter_manager)
    : filter_manager_(filter_manager) {}

RequestFilterManager::RequestHandler::~RequestHandler() = default;

bool RequestFilterManager::RequestHandler::WantsExtraHeadersForAnyRequest() {
  for (const auto& filter : filter_manager_->request_filters_) {
    if (filter->WantsExtraHeadersForAnyRequest())
      return true;
  }
  return false;
}

bool RequestFilterManager::RequestHandler::WantsExtraHeadersForRequest(
    FilteredRequestInfo* request) {
  for (const auto& filter : filter_manager_->request_filters_) {
    if (filter->WantsExtraHeadersForRequest(request))
      return true;
  }
  return false;
}

int RequestFilterManager::RequestHandler::OnBeforeRequest(
    content::BrowserContext* browser_context,
    FilteredRequestInfo* request,
    net::CompletionOnceCallback callback,
    GURL* new_url,
    bool* collapse) {
  if (GetAndSetSignaled(request->id, kOnBeforeRequest))
    return net::OK;

  PendingRequest& pending_request = pending_requests_[request->id];
  pending_request.request = request;
  pending_request.callback = std::move(callback);
  pending_request.new_url = new_url;
  pending_request.collapse = collapse;

  int num_filters_handling = 0;
  size_t priority = 0;
  for (const auto& filter : filter_manager_->request_filters_) {
    if (filter->OnBeforeRequest(
            browser_context, request,
            base::BindOnce(
                &RequestFilterManager::RequestHandler::OnBeforeRequestHandled,
                base::Unretained(this), request->id, priority++))) {
      num_filters_handling++;
    }
  }

  // Some filters may have run the callback synchronously, bringing
  // num_filters_blocking to a negative number. We get the final tally here by
  // adding the amount of filters that are handling the request.
  pending_request.num_filters_blocking += num_filters_handling;
  DCHECK_GE(pending_request.num_filters_blocking, 0);

  if (pending_request.num_filters_blocking != 0) {
    return net::ERR_IO_PENDING;
  }

  bool cancelled = pending_request.cancel == RequestFilter::kCancel;
  pending_requests_.erase(request->id);
  return cancelled ? net::ERR_BLOCKED_BY_CLIENT : net::OK;
}

void RequestFilterManager::RequestHandler::OnBeforeRequestHandled(
    int64_t request_id,
    size_t filter_priority,
    RequestFilter::CancelDecision cancel,
    bool collapse,
    const GURL& new_url) {
  auto pending_request_it = pending_requests_.find(request_id);
  if (pending_request_it == pending_requests_.end())
    return;
  PendingRequest& pending_request = pending_request_it->second;

  DCHECK(!collapse || cancel == RequestFilter::kCancel);

  if (cancel > pending_request.cancel) {
    pending_request.cancel = cancel;
    if (cancel == RequestFilter::kCancel && collapse &&
        pending_request.collapse) {
      *pending_request.collapse = true;
    } else if (cancel == RequestFilter::kPreventCancel) {
      *pending_request.collapse = false;
      *pending_request.new_url = GURL();
    }
  }

  if (pending_request.cancel != RequestFilter::kPreventCancel) {
    // All filters have different priorities and the callbacks only runs once,
    // so the value here would normally never be equal. However, the initial
    // value is 0 and we want filter 0 to be able to have a say.
    if (filter_priority >= pending_request.new_url_priority &&
        new_url.is_valid()) {
      *(pending_request.new_url) = new_url;
      pending_request.new_url_priority = filter_priority;
    }
  }

  // This will make the number of blocking filters negative if the callback was
  // called synchronously. This is accounted for in OnBeforeRequest
  pending_request.num_filters_blocking--;
  if (pending_request.num_filters_blocking != 0)
    return;

  // No more blocking filters. We can continue handling the request.
  DCHECK(!pending_request.callback.is_null());

  bool cancelled = pending_request.cancel == RequestFilter::kCancel;
  auto callback = std::move(pending_request.callback);
  pending_requests_.erase(request_id);
  std::move(callback).Run(cancelled ? net::ERR_BLOCKED_BY_CLIENT : net::OK);
}

int RequestFilterManager::RequestHandler::OnBeforeSendHeaders(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    net::CompletionOnceCallback callback,
    net::HttpRequestHeaders* headers,
    std::set<std::string>* set_headers,
    std::set<std::string>* removed_headers) {
  if (GetAndSetSignaled(request->id, kOnBeforeSendHeaders))
    return net::OK;

  PendingRequest& pending_request = pending_requests_[request->id];
  pending_request.request = request;
  pending_request.callback = std::move(callback);
  pending_request.request_headers = headers;
  pending_request.set_request_headers = set_headers;
  pending_request.removed_request_headers = removed_headers;
  pending_request.all_request_header_changes.clear();
  pending_request.all_request_header_changes.resize(
      filter_manager_->request_filters_.size());

  int num_filters_handling = 0;
  size_t priority = 0;
  for (const auto& filter : filter_manager_->request_filters_) {
    if (filter->OnBeforeSendHeaders(
            browser_context, request, headers,
            base::BindOnce(&RequestFilterManager::RequestHandler::
                               OnBeforeSendHeadersHandled,
                           base::Unretained(this), request->id, priority++))) {
      num_filters_handling++;
    }
  }

  // Some filters may have run the callback synchronously, bringing
  // num_filters_blocking to a negative number. We get the final tally here by
  // adding the amount of filters that are handling the request.
  pending_request.num_filters_blocking += num_filters_handling;
  DCHECK_GE(pending_request.num_filters_blocking, 0);

  if (pending_request.num_filters_blocking != 0) {
    return net::ERR_IO_PENDING;
  }

  MergeRequestHeaderChanges(&pending_request);

  bool cancelled = pending_request.cancel == RequestFilter::kCancel;
  pending_requests_.erase(request->id);
  return cancelled ? net::ERR_BLOCKED_BY_CLIENT : net::OK;
}

void RequestFilterManager::RequestHandler::OnBeforeSendHeadersHandled(
    int64_t request_id,
    size_t filter_priority,
    RequestFilter::CancelDecision cancel,
    RequestFilter::RequestHeaderChanges header_changes) {
  auto pending_request_it = pending_requests_.find(request_id);
  if (pending_request_it == pending_requests_.end())
    return;
  PendingRequest& pending_request = pending_request_it->second;

  if (cancel > pending_request.cancel) {
    pending_request.cancel = cancel;
  }

  pending_request.all_request_header_changes[filter_priority] =
      std::move(header_changes);

  // This will make the number of blocking filters negative if the callback was
  // called synchronously. This is accounted for in OnBeforeRequest
  pending_request.num_filters_blocking--;
  if (pending_request.num_filters_blocking != 0)
    return;

  // No more blocking filters. We can continue handling the request.
  MergeRequestHeaderChanges(&pending_request);

  DCHECK(!pending_request.callback.is_null());

  bool cancelled = pending_request.cancel == RequestFilter::kCancel;
  auto callback = std::move(pending_request.callback);
  pending_requests_.erase(request_id);
  std::move(callback).Run(cancelled ? net::ERR_BLOCKED_BY_CLIENT : net::OK);
}

void RequestFilterManager::RequestHandler::MergeRequestHeaderChanges(
    PendingRequest* pending_request) {
  // No point doing this if we're going to cancel anyway.
  if (pending_request->cancel == RequestFilter::kCancel)
    return;

  // No point doing this if the proxying side doesn't care.
  if (!pending_request->set_request_headers &&
      !pending_request->removed_request_headers)
    return;

  // It's neither or both.
  DCHECK(pending_request->set_request_headers &&
         pending_request->removed_request_headers);

  net::HttpRequestHeaders* request_headers = pending_request->request_headers;
  net::HttpRequestHeaders original_headers = *request_headers;

  for (const auto& header_changes :
       pending_request->all_request_header_changes) {
    if (header_changes.headers_to_modify.IsEmpty() &&
        header_changes.headers_to_remove.empty())
      continue;

    request_headers->MergeFrom(header_changes.headers_to_modify);

    for (auto header_to_remove : header_changes.headers_to_remove) {
      request_headers->RemoveHeader(header_to_remove);
    }
  }

  net::HttpRequestHeaders::Iterator original_headers_it(original_headers);

  while (original_headers_it.GetNext()) {
    if (!request_headers->HasHeader(original_headers_it.name()))
      pending_request->removed_request_headers->insert(
          original_headers_it.name());
  }

  net::HttpRequestHeaders::Iterator new_headers_it(*request_headers);

  while (new_headers_it.GetNext()) {
    std::optional<std::string> old_value =
        original_headers.GetHeader(new_headers_it.name());
    if (!old_value || old_value != new_headers_it.value())
      pending_request->set_request_headers->insert(new_headers_it.name());
  }
}

void RequestFilterManager::RequestHandler::OnSendHeaders(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    const net::HttpRequestHeaders& headers) {
  if (GetAndSetSignaled(request->id, kOnSendHeaders))
    return;

  ClearSignaled(request->id, kOnBeforeRedirect);

  for (const auto& filter : filter_manager_->request_filters_) {
    filter->OnSendHeaders(browser_context, request, headers);
  }
}

int RequestFilterManager::RequestHandler::OnHeadersReceived(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* preserve_fragment_on_redirect_url,
    bool* collapse) {
  if (GetAndSetSignaled(request->id, kOnHeadersReceived))
    return net::OK;

  PendingRequest& pending_request = pending_requests_[request->id];
  pending_request.request = request;
  pending_request.callback = std::move(callback);
  pending_request.original_response_headers = original_response_headers;
  pending_request.override_response_headers = override_response_headers;
  pending_request.all_response_header_changes.clear();
  pending_request.all_response_header_changes.resize(
      filter_manager_->request_filters_.size());
  pending_request.new_url = preserve_fragment_on_redirect_url;
  pending_request.collapse = collapse;

  int num_filters_handling = 0;
  size_t priority = 0;
  for (const auto& filter : filter_manager_->request_filters_) {
    if (filter->OnHeadersReceived(
            browser_context, request, original_response_headers,
            base::BindOnce(
                &RequestFilterManager::RequestHandler::OnHeadersReceivedHandled,
                base::Unretained(this), request->id, priority++))) {
      num_filters_handling++;
    }
  }

  // Some filters may have run the callback synchronously, bringing
  // num_filters_blocking to a negative number. We get the final tally here by
  // adding the amount of filters that are handling the request.
  pending_request.num_filters_blocking += num_filters_handling;
  DCHECK_GE(pending_request.num_filters_blocking, 0);

  if (pending_request.num_filters_blocking != 0) {
    // We do not allow collapsing asynchronously
    pending_request.collapse = nullptr;
    return net::ERR_IO_PENDING;
  }

  MergeResponseHeaderChanges(&pending_request);

  bool cancelled = pending_request.cancel == RequestFilter::kCancel;
  pending_requests_.erase(request->id);
  return cancelled ? net::ERR_BLOCKED_BY_CLIENT : net::OK;
}

void RequestFilterManager::RequestHandler::OnHeadersReceivedHandled(
    int64_t request_id,
    size_t filter_priority,
    RequestFilter::CancelDecision cancel,
    bool collapse,
    const GURL& new_url,
    RequestFilter::ResponseHeaderChanges header_changes) {
  auto pending_request_it = pending_requests_.find(request_id);
  if (pending_request_it == pending_requests_.end())
    return;
  PendingRequest& pending_request = pending_request_it->second;

  DCHECK(!collapse || cancel == RequestFilter::kCancel);

  if (cancel > pending_request.cancel) {
    pending_request.cancel = cancel;
    if (cancel == RequestFilter::kCancel && collapse &&
        pending_request.collapse) {
      *pending_request.collapse = true;
    } else if (cancel == RequestFilter::kPreventCancel &&
               pending_request.collapse) {
      *pending_request.collapse = false;
    }
  }

  // All filters have different priorities and the callbacks only runs once, so
  // the value here would normally never be equal. However, the initial value is
  // 0 and we want filter 0 to be able to have a say.
  if (filter_priority >= pending_request.new_url_priority &&
      new_url.is_valid()) {
    *(pending_request.new_url) = new_url;
    pending_request.new_url_priority = filter_priority;
  }

  pending_request.all_response_header_changes[filter_priority] =
      std::move(header_changes);

  // This will make the number of blocking filters negative if the callback was
  // called synchronously. This is accounted for in OnBeforeRequest
  pending_request.num_filters_blocking--;
  if (pending_request.num_filters_blocking != 0)
    return;

  // No more blocking filters. We can continue handling the request.
  MergeResponseHeaderChanges(&pending_request);

  DCHECK(!pending_request.callback.is_null());

  bool cancelled = pending_request.cancel == RequestFilter::kCancel;
  auto callback = std::move(pending_request.callback);
  pending_requests_.erase(request_id);
  std::move(callback).Run(cancelled ? net::ERR_BLOCKED_BY_CLIENT : net::OK);
}

void RequestFilterManager::RequestHandler::MergeResponseHeaderChanges(
    PendingRequest* pending_request) {
  // No point doing this if we're going to cancel anyway.
  if (pending_request->cancel == RequestFilter::kCancel)
    return;

  scoped_refptr<net::HttpResponseHeaders>* response_headers =
      pending_request->override_response_headers;

  for (const auto& header_changes :
       pending_request->all_response_header_changes) {
    if (header_changes.headers_to_add.empty() &&
        header_changes.headers_to_remove.empty())
      continue;

    if (response_headers->get() == nullptr) {
      *response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          pending_request->original_response_headers->raw_headers());
    }

    // Delete headers
    for (const auto& header : header_changes.headers_to_remove) {
      (*response_headers)->RemoveHeaderLine(header.first, header.second);
    }

    // Add headers.
    for (const auto& header : header_changes.headers_to_add) {
      RequestFilter::ResponseHeader lowercase_header(ToLowerCase(header));
      (*response_headers)
          ->AddHeader(lowercase_header.first, lowercase_header.second);
    }
  }

  if (pending_request->new_url->is_valid()) {
    // Only create a copy if we really want to modify the response headers.
    if (response_headers->get() == nullptr) {
      *response_headers = new net::HttpResponseHeaders(
          pending_request->original_response_headers->raw_headers());
    }
    (*response_headers)->ReplaceStatusLine("HTTP/1.1 302 Found");
    (*response_headers)->RemoveHeader("location");
    (*response_headers)
        ->AddHeader("Location", pending_request->new_url->spec());
  }
}

void RequestFilterManager::RequestHandler::OnBeforeRedirect(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    const GURL& new_location) {
  if (GetAndSetSignaled(request->id, kOnBeforeRedirect))
    return;

  ClearSignaled(request->id, kOnBeforeRequest);
  ClearSignaled(request->id, kOnBeforeSendHeaders);
  ClearSignaled(request->id, kOnSendHeaders);
  ClearSignaled(request->id, kOnHeadersReceived);

  for (const auto& filter : filter_manager_->request_filters_) {
    filter->OnBeforeRedirect(browser_context, request, new_location);
  }
}

void RequestFilterManager::RequestHandler::OnResponseStarted(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    int net_error) {
  // OnResponseStarted is even triggered, when the request was cancelled.
  if (net_error != net::OK)
    return;

  for (const auto& filter : filter_manager_->request_filters_) {
    filter->OnResponseStarted(browser_context, request);
  }
}

void RequestFilterManager::RequestHandler::OnCompleted(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    int net_error) {
  // See comment in OnErrorOccurred regarding net::ERR_WS_UPGRADE.
  DCHECK(net_error == net::OK || net_error == net::ERR_WS_UPGRADE);

  DCHECK(!GetAndSetSignaled(request->id, kOnCompleted));

  pending_requests_.erase(request->id);

  for (const auto& filter : filter_manager_->request_filters_) {
    filter->OnCompleted(browser_context, request);
  }
}

void RequestFilterManager::RequestHandler::OnErrorOccurred(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request,
    bool started,
    int net_error) {
  // When WebSocket handshake request finishes, the request is cancelled with an
  // ERR_WS_UPGRADE code (see WebSocketStreamRequestImpl::PerformUpgrade).
  // We report this as a completed request.
  if (net_error == net::ERR_WS_UPGRADE) {
    OnCompleted(browser_context, request, net_error);
    return;
  }

  DCHECK_NE(net::OK, net_error);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  DCHECK(!GetAndSetSignaled(request->id, kOnErrorOccurred));

  pending_requests_.erase(request->id);

  for (const auto& filter : filter_manager_->request_filters_) {
    filter->OnErrorOccured(browser_context, request, net_error);
  }
}

void RequestFilterManager::RequestHandler::OnRequestWillBeDestroyed(
    content::BrowserContext* browser_context,
    const FilteredRequestInfo* request) {
  pending_requests_.erase(request->id);
  signaled_requests_.erase(request->id);
}

bool RequestFilterManager::RequestHandler::GetAndSetSignaled(
    uint64_t request_id,
    EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = (iter->second & event_type) != 0;
  iter->second |= event_type;
  return was_signaled_before;
}

void RequestFilterManager::RequestHandler::ClearSignaled(
    uint64_t request_id,
    EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end())
    return;
  iter->second &= ~event_type;
}
}  // namespace vivaldi
