// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter_proxying_url_loader_factory.h"

#include <optional>
#include <utility>

#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/strings/stringprintf.h"
#include "base/types/optional_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/completion_repeating_callback.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace vivaldi {
namespace {

// This shutdown notifier makes sure the proxy is destroyed if an incognito
// browser context is destroyed. This is needed because RequestFilterManager
// only clears the proxies when the original browser context is destroyed.
class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<ShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "RequestFilterProxyingURLLoaderFactory") {}
  ~ShutdownNotifierFactory() override {}
};

void ForwardOnBeforeSendHeadersCallback(
    network::mojom::TrustedHeaderClient::OnBeforeSendHeadersCallback callback,
    const std::optional<::net::HttpRequestHeaders>& initial_headers,
    int32_t error_code,
    const std::optional<::net::HttpRequestHeaders>& headers) {
  if (headers) {
    std::move(callback).Run(error_code, headers);
  } else {
    std::move(callback).Run(error_code, initial_headers);
  }
}

void ForwardOnHeaderReceivedCallback(
    network::mojom::TrustedHeaderClient::OnHeadersReceivedCallback callback,
    const std::optional<std::string>& initial_headers,
    const std::optional<GURL>& initial_preserve_fragment_on_redirect_url,
    int32_t error_code,
    const std::optional<std::string>& headers,
    const std::optional<GURL>& preserve_fragment_on_redirect_url) {
  std::move(callback).Run(error_code, headers ? headers : initial_headers,
                          preserve_fragment_on_redirect_url
                              ? preserve_fragment_on_redirect_url
                              : initial_preserve_fragment_on_redirect_url);
}

// Creates simulated net::RedirectInfo when a filter redirects a request,
// behaving like a redirect response was actually returned by the remote server.
net::RedirectInfo CreateRedirectInfo(
    const network::ResourceRequest& original_request,
    const GURL& new_url,
    int response_code,
    const std::optional<std::string>& referrer_policy_header) {
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies,
      original_request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      original_request.referrer_policy, original_request.referrer.spec(),
      response_code, new_url, referrer_policy_header,
      /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/false,
      /*is_signed_exchange_fallback_redirect=*/false);
}

}  // namespace

RequestFilterProxyingURLLoaderFactory::InProgressRequest::FollowRedirectParams::
    FollowRedirectParams() = default;
RequestFilterProxyingURLLoaderFactory::InProgressRequest::FollowRedirectParams::
    ~FollowRedirectParams() = default;

RequestFilterProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    RequestFilterProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    int32_t network_service_request_id,
    int32_t view_routing_id,
    int32_t frame_routing_id,
    uint32_t options,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
    : factory_(factory),
      request_(request),
      original_initiator_(request.request_initiator),
      request_id_(request_id),
      network_service_request_id_(network_service_request_id),
      view_routing_id_(view_routing_id),
      frame_routing_id_(frame_routing_id),
      options_(options),
      traffic_annotation_(traffic_annotation),
      proxied_loader_receiver_(this,
                               std::move(loader_receiver),
                               navigation_response_task_runner),
      target_client_(std::move(client)),
      current_response_(network::mojom::URLResponseHead::New()),
      has_any_extra_headers_listeners_(
          network_service_request_id_ != 0 &&
          factory->request_handler_->WantsExtraHeadersForAnyRequest()),
      navigation_response_task_runner_(navigation_response_task_runner) {
  // If there is a client error, clean up the request.
  target_client_.set_disconnect_handler(
      base::BindOnce(&RequestFilterProxyingURLLoaderFactory::InProgressRequest::
                         OnClientDisconnected,
                     weak_factory_.GetWeakPtr()));

  proxied_loader_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&RequestFilterProxyingURLLoaderFactory::InProgressRequest::
                         OnLoaderDisconnected,
                     weak_factory_.GetWeakPtr()));
}

RequestFilterProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    RequestFilterProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    int32_t frame_routing_id,
    const network::ResourceRequest& request)
    : factory_(factory),
      request_(request),
      original_initiator_(request.request_initiator),
      request_id_(request_id),
      frame_routing_id_(frame_routing_id),
      proxied_loader_receiver_(this),
      for_cors_preflight_(true),
      has_any_extra_headers_listeners_(
          factory->request_handler_->WantsExtraHeadersForAnyRequest()) {}

RequestFilterProxyingURLLoaderFactory::InProgressRequest::~InProgressRequest() {
  DCHECK_NE(state_, State::kInvalid);
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  if (info_) {
    factory_->request_handler_->OnRequestWillBeDestroyed(
        factory_->browser_context_, &info_.value());
  }
  if (on_before_send_headers_callback_) {
    std::move(on_before_send_headers_callback_)
        .Run(net::ERR_ABORTED, std::nullopt);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, std::nullopt, std::nullopt);
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::Restart() {
  UpdateRequestInfo();
  RestartInternal();
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    UpdateRequestInfo() {
  // Derive a new FilteredRequestInfo value any time |Restart()| is called,
  // because the details in |request_| may have changed e.g. if we've been
  // redirected. |request_initiator| can be modified on redirects, but we keep
  // the original for |initiator| in the event.
  network::ResourceRequest request_for_info = request_;
  request_for_info.request_initiator = original_initiator_;
  info_.emplace(request_id_, factory_->render_process_id_, frame_routing_id_,
                request_for_info, factory_->loader_factory_type(),
                !(options_ & network::mojom::kURLLoadOptionSynchronous), false,
                factory_->navigation_id_);

  // The value of `has_any_extra_headers_listeners_` is constant for the
  // lifetime of InProgressRequest and determines whether the request is made
  // with the network::mojom::kURLLoadOptionUseHeaderClient option. To prevent
  // the redirected request from getting into a state where
  // `current_request_uses_header_client_` is true but the request is not made
  // with the kURLLoadOptionUseHeaderClient option, also check
  // `has_any_extra_headers_listeners_` here. See http://crbug.com/1074282.
  current_request_uses_header_client_ =
      has_any_extra_headers_listeners_ &&
      factory_->url_loader_header_client_receiver_.is_bound() &&
      (request_.url.SchemeIsHTTPOrHTTPS() ||
       request_.url.SchemeIs(url::kUuidInPackageScheme)) &&
      (for_cors_preflight_ || network_service_request_id_ != 0) &&
      factory_->request_handler_->WantsExtraHeadersForRequest(&info_.value());
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    RestartInternal() {
  DCHECK_EQ(info_->request.url, request_.url)
      << "UpdateRequestInfo must have been called first";
  is_header_client_receiver_paused_ = false;
  // If the header client will be used, we start the request immediately, and
  // OnBeforeSendHeaders and OnSendHeaders will be handled there. Otherwise,
  // send these events before the request starts.
  base::RepeatingCallback<void(int)> continuation;
  const auto state_on_error = State::kRejectedByOnBeforeRequest;
  if (current_request_uses_header_client_) {
    continuation =
        base::BindRepeating(&InProgressRequest::ContinueToStartRequest,
                            weak_factory_.GetWeakPtr(), state_on_error);
  } else if (for_cors_preflight_) {
    // In this case we do nothing because filters should see nothing.
    return;
  } else {
    continuation =
        base::BindRepeating(&InProgressRequest::ContinueToBeforeSendHeaders,
                            weak_factory_.GetWeakPtr(), state_on_error);
  }
  redirect_url_ = GURL();
  collapse_initiator_ = false;
  int result = factory_->request_handler_->OnBeforeRequest(
      factory_->browser_context_, &info_.value(), continuation, &redirect_url_,
      &collapse_initiator_);
  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    network::URLLoaderCompletionStatus status =
        CreateURLLoaderCompletionStatus(result, collapse_initiator_);

    OnRequestError(status, state_on_error);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    // One or more listeners is blocking, so the request must be paused until
    // they respond. |continuation| above will be invoked asynchronously to
    // continue or cancel the request.
    //
    // We pause the receiver here to prevent further client message processing.
    if (proxied_client_receiver_.is_bound()) {
      proxied_client_receiver_.Pause();
    }

    // Pause the header client, since we want to wait until OnBeforeRequest has
    // finished before processing any future events.
    if (header_client_receiver_.is_bound()) {
      header_client_receiver_.Pause();
      is_header_client_receiver_paused_ = true;
    }
    return;
  }
  DCHECK_EQ(net::OK, result);

  continuation.Run(net::OK);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  if (new_url) {
    request_.url = new_url.value();
  }

  for (const std::string& header : removed_headers) {
    request_.headers.RemoveHeader(header);
  }
  request_.headers.MergeFrom(modified_headers);

  // Call this before checking |current_request_uses_header_client_| as it
  // calculates it.
  UpdateRequestInfo();

  if (target_loader_.is_bound()) {
    // If header_client_ is used, then we have to call FollowRedirect now as
    // that's what triggers the network service calling back to
    // OnBeforeSendHeaders(). Otherwise, don't call FollowRedirect now. Wait for
    // the onBeforeSendHeaders callback(s) to run as these may modify request
    // headers and if so we'll pass these modifications to FollowRedirect.
    if (current_request_uses_header_client_) {
      target_loader_->FollowRedirect(removed_headers, modified_headers,
                                     modified_cors_exempt_headers, new_url);
    } else {
      auto params = std::make_unique<FollowRedirectParams>();
      params->removed_headers = removed_headers;
      params->modified_headers = modified_headers;
      params->modified_cors_exempt_headers = modified_cors_exempt_headers;
      params->new_url = new_url;
      pending_follow_redirect_params_ = std::move(params);
    }
  }

  RestartInternal();
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  if (target_loader_.is_bound()) {
    target_loader_->SetPriority(priority, intra_priority_value);
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    PauseReadingBodyFromNet() {
  if (target_loader_.is_bound()) {
    target_loader_->PauseReadingBodyFromNet();
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ResumeReadingBodyFromNet() {
  if (target_loader_.is_bound()) {
    target_loader_->ResumeReadingBodyFromNet();
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) {
  target_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveResponse(network::mojom::URLResponseHeadPtr head,
                      mojo::ScopedDataPipeConsumerHandle body,
                      std::optional<::mojo_base::BigBuffer> cached_metadata) {
  current_body_ = std::move(body);
  current_cached_metadata_ = std::move(cached_metadata);
  if (current_request_uses_header_client_) {
    // Use the cookie headers we got from OnHeadersReceived as that'll contain
    // Set-Cookie if it existed. Re-adding cookie headers here does not
    // duplicate any headers, because the headers we received via Mojo have been
    // stripped of any cookie response headers.
    auto saved_headers = current_response_->headers;
    current_response_ = std::move(head);
    size_t headers_iterator = 0;
    std::string header_name, header_value;
    while (saved_headers != nullptr &&
           saved_headers->EnumerateHeaderLines(&headers_iterator, &header_name,
                                               &header_value)) {
      if (net::HttpResponseHeaders::IsCookieResponseHeader(header_name)) {
        current_response_->headers->AddHeader(header_name, header_value);
      }
    }
    ContinueToResponseStarted();
  } else {
    current_response_ = std::move(head);
    HandleResponseOrRedirectHeaders(base::BindOnce(
        &InProgressRequest::OverwriteHeadersAndContinueToResponseStarted,
        weak_factory_.GetWeakPtr()));
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr head) {
  if (redirect_url_ != redirect_info.new_url &&
      !IsRedirectSafe(request_.url, redirect_info.new_url,
                      info_->loader_factory_type ==
                          content::ContentBrowserClient::URLLoaderFactoryType::
                              kNavigation)) {
    OnNetworkError(CreateURLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT));
    return;
  }

  if (current_request_uses_header_client_) {
    // Use the headers we got from OnHeadersReceived as that'll contain
    // Set-Cookie if it existed.
    auto saved_headers = current_response_->headers;
    current_response_ = std::move(head);
    // If this redirect is from an HSTS upgrade, OnHeadersReceived will not be
    // called before OnReceiveRedirect, so make sure the saved headers exist
    // before setting them.
    if (saved_headers) {
      current_response_->headers = saved_headers;
    }
    ContinueToBeforeRedirect(redirect_info, net::OK);
  } else {
    current_response_ = std::move(head);
    HandleResponseOrRedirectHeaders(
        base::BindOnce(&InProgressRequest::ContinueToBeforeRedirect,
                       weak_factory_.GetWeakPtr(), redirect_info));
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    OnNetworkError(status);
    return;
  }

  state_ = kCompleted;
  target_client_->OnComplete(status);
  factory_->request_handler_->OnCompleted(factory_->browser_context_,
                                          &info_.value(), status.error_code);

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::OnLoaderCreated(
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient>
        forwarding_header_client) {
  // When CORS is involved there may be multiple network::URLLoader associated
  // with this InProgressRequest, because CorsURLLoader may create a new
  // network::URLLoader for the same request id in redirect handling - see
  // CorsURLLoader::FollowRedirect. In such a case the old network::URLLoader
  // is going to be detached fairly soon, so we don't need to take care of it.
  // We need this explicit reset to avoid a DCHECK failure in mojo::Receiver.
  header_client_receiver_.reset();

  header_client_receiver_.Bind(std::move(receiver));
  if (is_header_client_receiver_paused_) {
    header_client_receiver_.Pause();
  }

  forwarding_header_client_.reset();
  forwarding_header_client_.Bind(std::move(forwarding_header_client));

  if (for_cors_preflight_) {
    // In this case we don't have |target_loader_| and
    // |proxied_client_receiver_|, and |receiver| is the only connection to the
    // network service, so we observe mojo connection errors.
    header_client_receiver_.set_disconnect_handler(
        base::BindOnce(&RequestFilterProxyingURLLoaderFactory::
                           InProgressRequest::OnNetworkError,
                       weak_factory_.GetWeakPtr(),
                       CreateURLLoaderCompletionStatus(net::ERR_FAILED)));
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                        OnBeforeSendHeadersCallback callback) {
  if (!current_request_uses_header_client_) {
    if (forwarding_header_client_) {
      forwarding_header_client_->OnBeforeSendHeaders(headers,
                                                     std::move(callback));
    } else {
      std::move(callback).Run(net::OK, std::nullopt);
    }
    return;
  }

  request_.headers = headers;
  on_before_send_headers_callback_ = std::move(callback);
  ContinueToBeforeSendHeadersWithOk();
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnHeadersReceived(const std::string& headers,
                      const net::IPEndPoint& remote_endpoint,
                      OnHeadersReceivedCallback callback) {
  if (!current_request_uses_header_client_) {
    if (forwarding_header_client_) {
      forwarding_header_client_->OnHeadersReceived(headers, remote_endpoint,
                                                   std::move(callback));
    } else {
      // Make sure the callback is run, otherwise XHRs would fail when
      // webrequest listeners was set.
      std::move(callback).Run(net::OK, std::nullopt, std::nullopt);
    }
    if (for_cors_preflight_) {
      // CORS preflight is supported only when "ExtraHeaders" are requested.
      // Deletes |this| unless we have a forwarding client dealing with the
      // callback. In that case, the forwarding client may delete itself,
      // resulting in us landing in OnRequestError.
      if (!forwarding_header_client_) {
        factory_->RemoveRequest(network_service_request_id_, request_id_);
      }
    }
    return;
  }

  on_headers_received_callback_ = std::move(callback);
  current_response_ = network::mojom::URLResponseHead::New();
  current_response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(headers);
  current_response_->remote_endpoint = remote_endpoint;
  HandleResponseOrRedirectHeaders(
      base::BindOnce(&InProgressRequest::ContinueToHandleOverrideHeaders,
                     weak_factory_.GetWeakPtr()));
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    HandleBeforeRequestRedirect() {
  // The filter requested a redirect. Close the connection with the current
  // URLLoader and inform the URLLoaderClient a request filter generated a
  // redirect. To load |redirect_url_|, a new URLLoader will be recreated
  // after receiving FollowRedirect().

  // Forgetting to close the connection with the current URLLoader caused
  // bugs. The latter doesn't know anything about the redirect. Continuing
  // the load with it gives unexpected results. See
  // https://crbug.com/882661#c72.
  proxied_client_receiver_.reset();
  header_client_receiver_.reset();
  target_loader_.reset();

  constexpr int kInternalRedirectStatusCode = net::HTTP_TEMPORARY_REDIRECT;

  net::RedirectInfo redirect_info =
      CreateRedirectInfo(request_, redirect_url_, kInternalRedirectStatusCode,
                         /*referrer_policy_header=*/std::nullopt);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers = base::StringPrintf(
      "HTTP/1.1 %i Internal Redirect\n"
      "Location: %s\n"
      "Non-Authoritative-Reason: Request Filtering\n\n",
      kInternalRedirectStatusCode, redirect_url_.spec().c_str());

  // Cross-origin requests need to modify the Origin header to 'null'. Since
  // CorsURLLoader sets |request_initiator| to the Origin request header in
  // NetworkService, we need to modify |request_initiator| here to craft the
  // Origin header indirectly.
  // Following checks implement the step 10 of "4.4. HTTP-redirect fetch",
  // https://fetch.spec.whatwg.org/#http-redirect-fetch
  if (request_.request_initiator &&
      (!url::IsSameOriginWith(redirect_url_, request_.url) &&
       !request_.request_initiator->IsSameOriginWith(request_.url))) {
    // Reset the initiator to pretend tainted origin flag of the spec is set.
    request_.request_initiator = url::Origin();
  }
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->encoded_data_length = 0;

  current_response_ = std::move(head);
  ContinueToBeforeRedirect(redirect_info, net::OK);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeaders(State state_on_error, int error_code) {
  if (error_code != net::OK) {
    OnRequestError(
        CreateURLLoaderCompletionStatus(error_code, collapse_initiator_),
        state_on_error);
    return;
  }

  if (!current_request_uses_header_client_ && !redirect_url_.is_empty()) {
    HandleBeforeRequestRedirect();
    return;
  }

  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }

  if (request_.url.SchemeIsHTTPOrHTTPS() ||
      request_.url.SchemeIs(url::kUuidInPackageScheme)) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onBeforeSendHeaders| is only dispatched for HTTP and HTTPS
    // and urn: requests.

    set_request_headers_.clear();
    removed_request_headers_.clear();

    state_on_error = State::kRejectedByOnBeforeSendHeaders;
    auto continuation =
        base::BindRepeating(&InProgressRequest::ContinueToSendHeaders,
                            weak_factory_.GetWeakPtr(), state_on_error);
    int result = factory_->request_handler_->OnBeforeSendHeaders(
        factory_->browser_context_, &info_.value(), continuation,
        &request_.headers, &set_request_headers_, &removed_request_headers_);

    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      // The request was cancelled synchronously. Dispatch an error notification
      // and terminate the request.
      OnRequestError(CreateURLLoaderCompletionStatus(result), state_on_error);
      return;
    }

    if (result == net::ERR_IO_PENDING) {
      // One or more listeners is blocking, so the request must be paused until
      // they respond. |continuation| above will be invoked asynchronously to
      // continue or cancel the request.
      //
      // We pause the binding here to prevent further client message processing.
      if (proxied_client_receiver_.is_bound()) {
        proxied_client_receiver_.Pause();
      }
      return;
    }
    DCHECK_EQ(net::OK, result);
  }

  ContinueToSendHeadersWithOk();
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeadersWithOk() {
  ContinueToBeforeSendHeaders(State::kInvalid, net::OK);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToStartRequest(State state_on_error, int error_code) {
  if (error_code != net::OK) {
    OnRequestError(
        CreateURLLoaderCompletionStatus(error_code, collapse_initiator_),
        state_on_error);
    return;
  }

  if (current_request_uses_header_client_ && !redirect_url_.is_empty()) {
    if (for_cors_preflight_) {
      // CORS preflight doesn't support redirect.
      OnRequestError(CreateURLLoaderCompletionStatus(net::ERR_FAILED),
                     state_on_error);
      return;
    }
    HandleBeforeRequestRedirect();
    return;
  }

  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }

  if (header_client_receiver_.is_bound()) {
    header_client_receiver_.Resume();
    is_header_client_receiver_paused_ = false;
  }

  if (for_cors_preflight_) {
    // For CORS preflight requests, we have already started the request in
    // the network service. We did block the request by blocking
    // |header_client_receiver_|, which we unblocked right above.
    return;
  }

  if (!target_loader_.is_bound() && factory_->target_factory_.is_bound()) {
    // No filter have cancelled us up to this point, so it's now OK to
    // initiate the real network request.
    uint32_t options = options_;
    // Even if this request does not use the header client, future redirects
    // might, so we need to set the option on the loader.
    if (has_any_extra_headers_listeners_) {
      options |= network::mojom::kURLLoadOptionUseHeaderClient;
    }
    factory_->target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(
            navigation_response_task_runner_),
        network_service_request_id_, options, request_,
        proxied_client_receiver_.BindNewPipeAndPassRemote(
            navigation_response_task_runner_),
        traffic_annotation_);
  }

  // From here the lifecycle of this request is driven by subsequent events on
  // either |proxied_loader_receiver_|, |proxied_client_receiver_|, or
  // |header_client_receiver_|.
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToStartRequestWithOk() {
  ContinueToStartRequest(State::kInvalid, net::OK);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeaders(State state_on_error, int error_code) {
  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code), state_on_error);
    return;
  }

  if (current_request_uses_header_client_) {
    DCHECK(on_before_send_headers_callback_);
    if (forwarding_header_client_) {
      forwarding_header_client_->OnBeforeSendHeaders(
          request_.headers,
          base::BindOnce(&ForwardOnBeforeSendHeadersCallback,
                         std::move(on_before_send_headers_callback_),
                         request_.headers));
    } else {
      std::move(on_before_send_headers_callback_)
          .Run(error_code, request_.headers);
    }
  } else if (pending_follow_redirect_params_) {
    pending_follow_redirect_params_->removed_headers.insert(
        pending_follow_redirect_params_->removed_headers.end(),
        removed_request_headers_.begin(), removed_request_headers_.end());

    for (auto& set_header : set_request_headers_) {
      std::optional<std::string> header_value =
          request_.headers.GetHeader(set_header);
      if (header_value) {
        pending_follow_redirect_params_->modified_headers.SetHeader(
            set_header, *header_value);
      } else {
        NOTREACHED();
      }
    }

    if (target_loader_.is_bound()) {
      target_loader_->FollowRedirect(
          pending_follow_redirect_params_->removed_headers,
          pending_follow_redirect_params_->modified_headers,
          pending_follow_redirect_params_->modified_cors_exempt_headers,
          pending_follow_redirect_params_->new_url);
    }

    pending_follow_redirect_params_.reset();
  }

  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }

  if (request_.url.SchemeIsHTTPOrHTTPS() ||
      request_.url.SchemeIs(url::kUuidInPackageScheme)) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onSendHeaders| is only dispatched for HTTP and HTTPS
    // requests.
    factory_->request_handler_->OnSendHeaders(factory_->browser_context_,
                                              &info_.value(), request_.headers);
  }

  if (!current_request_uses_header_client_) {
    ContinueToStartRequestWithOk();
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeadersWithOk() {
  ContinueToSendHeaders(State::kInvalid, net::OK);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToHandleOverrideHeaders(int error_code) {
  if (error_code != net::OK) {
    const int status_code = current_response_->headers
                                ? current_response_->headers->response_code()
                                : 0;
    State state;
    if (status_code == net::HTTP_UNAUTHORIZED) {
      state = State::kRejectedByOnHeadersReceivedForAuth;
    } else if (net::HttpResponseHeaders::IsRedirectResponseCode(status_code)) {
      state = State::kRejectedByOnHeadersReceivedForRedirect;
    } else {
      state = State::kRejectedByOnHeadersReceivedForFinalResponse;
    }
    OnRequestError(CreateURLLoaderCompletionStatus(error_code), state);
    return;
  }

  DCHECK(on_headers_received_callback_);
  std::optional<std::string> headers;
  if (override_headers_) {
    headers = override_headers_->raw_headers();
    if (current_request_uses_header_client_) {
      // Make sure to update current_response_,  since when OnReceiveResponse
      // is called we will not use its headers as it might be missing the
      // Set-Cookie line (as that gets stripped over IPC).
      current_response_->headers = override_headers_;
    }
  }

  if (for_cors_preflight_ && !redirect_url_.is_empty()) {
    OnRequestError(CreateURLLoaderCompletionStatus(net::ERR_FAILED),
                   State::kRejectedByOnHeadersReceivedForRedirect);
    return;
  }

  if (forwarding_header_client_) {
    forwarding_header_client_->OnHeadersReceived(
        headers ? *headers : current_response_->headers->raw_headers(),
        current_response_->remote_endpoint,
        base::BindOnce(&ForwardOnHeaderReceivedCallback,
                       std::move(on_headers_received_callback_), headers,
                       redirect_url_));
  } else {
    std::move(on_headers_received_callback_)
        .Run(net::OK, headers, redirect_url_);
  }
  override_headers_ = nullptr;

  if (for_cors_preflight_) {
    // If this is for CORS preflight, there is no associated client.
    info_->AddResponse(*current_response_);
    // Do not finish proxied preflight requests that require proxy auth.
    // The request is not finished yet, give control back to network service
    // which will start authentication process.
    if (current_response_->headers &&
        current_response_->headers->response_code() ==
            net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
      return;
    }
    // We notify the completion here, and delete |this|.
    factory_->request_handler_->OnResponseStarted(factory_->browser_context_,
                                                  &info_.value(), net::OK);
    factory_->request_handler_->OnCompleted(factory_->browser_context_,
                                            &info_.value(), net::OK);

    // Deletes |this| unless we have a forwarding client dealing with the
    // callback. In that case, the forwarding client may delete itself,
    // resulting in us landing in OnRequestError.
    if (!forwarding_header_client_) {
      factory_->RemoveRequest(network_service_request_id_, request_id_);
    }
    return;
  }

  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OverwriteHeadersAndContinueToResponseStarted(int error_code) {
  DCHECK(!for_cors_preflight_);
  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code),
                   State::kRejectedByOnHeadersReceivedForFinalResponse);
    return;
  }

  DCHECK(!current_request_uses_header_client_ || !override_headers_);

  if (!override_headers_) {
    ContinueToResponseStarted();
    return;
  }

  current_response_->headers = override_headers_;

  // The extension modified the response headers without specifying the
  // 'extraHeaders' option. We need to repopulate the ParsedHeader to reflect
  // the modified headers.
  //
  // TODO(crbug.com/40765899): Once problems with 'extraHeaders' are
  // sorted out, migrate these headers over to requiring 'extraHeaders' and
  // remove this code.
  //
  // Note: As an optimization, we reparse the ParsedHeaders only for navigation
  // and worker requests, since they are not used for subresource requests.
  using URLLoaderFactoryType =
      content::ContentBrowserClient::URLLoaderFactoryType;
  switch (factory_->loader_factory_type()) {
    case URLLoaderFactoryType::kDocumentSubResource:
    case URLLoaderFactoryType::kWorkerSubResource:
    case URLLoaderFactoryType::kServiceWorkerSubResource:
      ContinueToResponseStarted();
      return;
    case URLLoaderFactoryType::kNavigation:
    case URLLoaderFactoryType::kWorkerMainResource:
    case URLLoaderFactoryType::kServiceWorkerScript:
    case URLLoaderFactoryType::kDownload:
    case URLLoaderFactoryType::kPrefetch:
    case URLLoaderFactoryType::kDevTools:
    case URLLoaderFactoryType::kEarlyHints:
      break;
  }

  proxied_client_receiver_.Pause();
  content::GetNetworkService()->ParseHeaders(
      request_.url, current_response_->headers,
      base::BindOnce(
          &InProgressRequest::AssignParsedHeadersAndContinueToResponseStarted,
          weak_factory_.GetWeakPtr()));
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    AssignParsedHeadersAndContinueToResponseStarted(
        network::mojom::ParsedHeadersPtr parsed_headers) {
  current_response_->parsed_headers = std::move(parsed_headers);
  ContinueToResponseStarted();
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToResponseStarted() {
  if (state_ == State::kInProgress) {
    state_ = State::kInProgressWithFinalResponseReceived;
  }

  std::string redirect_location;
  if (override_headers_ && override_headers_->IsRedirect(&redirect_location)) {
    // The response headers may have been overridden by an |onHeadersReceived|
    // handler and may have been changed to a redirect. We handle that here
    // instead of acting like regular request completion.
    //
    // Note that we can't actually change how the Network Service handles the
    // original request at this point, so our "redirect" is really just
    // generating an artificial |onBeforeRedirect| event and starting a new
    // request to the Network Service. Our client shouldn't know the difference.
    GURL new_url(redirect_location);

    net::RedirectInfo redirect_info = CreateRedirectInfo(
        request_, new_url, override_headers_->response_code(),
        net::RedirectUtil::GetReferrerPolicyHeader(override_headers_.get()));

    // These will get re-bound if a new request is initiated by
    // |FollowRedirect()|.
    proxied_client_receiver_.reset();
    header_client_receiver_.reset();
    target_loader_.reset();

    ContinueToBeforeRedirect(redirect_info, net::OK);
    return;
  }

  info_->AddResponse(*current_response_);

  proxied_client_receiver_.Resume();

  factory_->request_handler_->OnResponseStarted(factory_->browser_context_,
                                                &info_.value(), net::OK);
  target_client_->OnReceiveResponse(current_response_.Clone(),
                                    std::move(current_body_),
                                    std::move(current_cached_metadata_));
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                             int error_code) {
  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code),
                   kRejectedByOnHeadersReceivedForRedirect);
    return;
  }

  info_->AddResponse(*current_response_);

  if (proxied_client_receiver_.is_bound()) {
    proxied_client_receiver_.Resume();
  }

  factory_->request_handler_->OnBeforeRedirect(
      factory_->browser_context_, &info_.value(), redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, current_response_.Clone());
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
  if (request_.trusted_params) {
    request_.trusted_params->isolation_info =
        request_.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(redirect_info.new_url));
  }

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod) {
    request_.request_body = nullptr;
  }
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    HandleResponseOrRedirectHeaders(net::CompletionOnceCallback continuation) {
  override_headers_ = nullptr;
  redirect_url_ = GURL();

  auto callback_pair = base::SplitOnceCallback(std::move(continuation));
  if (request_.url.SchemeIsHTTPOrHTTPS() ||
      request_.url.SchemeIs(url::kUuidInPackageScheme)) {
    DCHECK(info_.has_value());
    bool should_collapse_initiator = false;
    int result = factory_->request_handler_->OnHeadersReceived(
        factory_->browser_context_, &info_.value(),
        std::move(callback_pair.first), current_response_->headers.get(),
        &override_headers_, &redirect_url_, &should_collapse_initiator);
    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      const int status_code = current_response_->headers
                                  ? current_response_->headers->response_code()
                                  : 0;
      State state;
      if (status_code == net::HTTP_UNAUTHORIZED) {
        state = State::kRejectedByOnHeadersReceivedForAuth;
      } else if (net::HttpResponseHeaders::IsRedirectResponseCode(
                     status_code)) {
        state = State::kRejectedByOnHeadersReceivedForRedirect;
      } else {
        state = State::kRejectedByOnHeadersReceivedForFinalResponse;
      }
      OnRequestError(
          CreateURLLoaderCompletionStatus(result, should_collapse_initiator),
          state);
      return;
    }

    if (result == net::ERR_IO_PENDING) {
      if (proxied_client_receiver_.is_bound()) {
        // One or more listeners is blocking, so the request must be paused
        // until they respond. |continuation| above will be invoked
        // asynchronously to continue or cancel the request.
        //
        // We pause the binding here to prevent further client message
        // processing.
        proxied_client_receiver_.Pause();
      }
    }

    DCHECK_EQ(net::OK, result);
  }

  std::move(callback_pair.second).Run(net::OK);
}
void RequestFilterProxyingURLLoaderFactory::InProgressRequest::OnRequestError(
    const network::URLLoaderCompletionStatus& status,
    State state) {
  if (target_client_) {
    target_client_->OnComplete(status);
  }
  factory_->request_handler_->OnErrorOccurred(
      factory_->browser_context_, &info_.value(), true /* started */,
      status.error_code);
  state_ = state;

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::OnNetworkError(
    const network::URLLoaderCompletionStatus& status) {
  State state = state_;
  if (state_ == State::kInProgress) {
    state = State::kRejectedByNetworkError;
  } else if (state_ == State::kInProgressWithFinalResponseReceived) {
    state = State::kRejectedByNetworkErrorAfterReceivingFinalResponse;
  }
  OnRequestError(status, state);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnClientDisconnected() {
  State state = state_;
  if (state_ == State::kInProgress) {
    state = State::kDetachedFromClient;
  } else if (state_ == State::kInProgressWithFinalResponseReceived) {
    state = State::kDetachedFromClientAfterReceivingResponse;
  }
  OnRequestError(CreateURLLoaderCompletionStatus(net::ERR_ABORTED), state);
}

void RequestFilterProxyingURLLoaderFactory::InProgressRequest::
    OnLoaderDisconnected(uint32_t custom_reason,
                         const std::string& description) {
  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason &&
      description == blink::ThrottlingURLLoader::kFollowRedirectReason) {
    // Save the ID here because this request will be restarted with a new
    // URLLoader instead of continuing with FollowRedirect(). The saved ID will
    // be retrieved in the restarted request, which will call
    // RequestIDGenerator::Generate() with the same ID pair.
    factory_->request_id_generator_->SaveID(
        view_routing_id_, network_service_request_id_, request_id_);

    state_ = State::kRedirectFollowedByAnotherInProgressRequest;
    // Deletes |this|.
    factory_->RemoveRequest(network_service_request_id_, request_id_);
  } else {
    OnNetworkError(CreateURLLoaderCompletionStatus(net::ERR_ABORTED));
  }
}

// Determines whether it is safe to redirect from |from_url| to |to_url|.
bool RequestFilterProxyingURLLoaderFactory::InProgressRequest::IsRedirectSafe(
    const GURL& upstream_url,
    const GURL& target_url,
    bool is_navigation_request) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!is_navigation_request &&
      target_url.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(factory_->browser_context_)
            ->enabled_extensions()
            .GetByID(target_url.host());
    if (!extension) {
      return false;
    }
    return extensions::WebAccessibleResourcesInfo::
        IsResourceWebAccessibleRedirect(extension, target_url,
                                        original_initiator_, upstream_url);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return content::IsSafeRedirectTarget(upstream_url, target_url);
}

network::URLLoaderCompletionStatus RequestFilterProxyingURLLoaderFactory::
    InProgressRequest::CreateURLLoaderCompletionStatus(
        int error_code,
        bool collapse_initiator) {
  network::URLLoaderCompletionStatus status(error_code);
  status.should_collapse_initiator = collapse_initiator;
  return status;
}

RequestFilterProxyingURLLoaderFactory::RequestFilterProxyingURLLoaderFactory(
    content::BrowserContext* browser_context,
    int render_process_id,
    int frame_routing_id,
    int view_routing_id,
    RequestFilterManager::RequestHandler* request_handler,
    RequestFilterManager::RequestIDGenerator* request_id_generator,
    std::optional<int64_t> navigation_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        forwarding_header_client,
    RequestFilterManager::ProxySet* proxies,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
    : browser_context_(browser_context),
      render_process_id_(render_process_id),
      frame_routing_id_(frame_routing_id),
      view_routing_id_(view_routing_id),
      request_handler_(request_handler),
      request_id_generator_(request_id_generator),
      navigation_id_(std::move(navigation_id)),
      forwarding_url_loader_header_client_(std::move(forwarding_header_client)),
      proxies_(proxies),
      loader_factory_type_(loader_factory_type),
      navigation_response_task_runner_(
          std::move(navigation_response_task_runner)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // base::Unretained is safe here because the callback will be
  // canceled when |shutdown_notifier_subscription_| is destroyed, and
  // |proxies_| owns this.
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()
          ->Get(browser_context)
          ->Subscribe(
              base::BindRepeating(&RequestFilterManager::ProxySet::RemoveProxy,
                                  base::Unretained(proxies_), this));

  auto [loader_receiver, target_factory_remote] = factory_builder.Append();

  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &RequestFilterProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));
  proxy_receivers_.Add(this, std::move(loader_receiver),
                       navigation_response_task_runner_);
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &RequestFilterProxyingURLLoaderFactory::OnProxyBindingError,
      base::Unretained(this)));

  if (header_client_receiver) {
    url_loader_header_client_receiver_.Bind(std::move(header_client_receiver),
                                            navigation_response_task_runner_);
  }
}

void RequestFilterProxyingURLLoaderFactory::StartProxying(
    content::BrowserContext* browser_context,
    int render_process_id,
    int frame_routing_id,
    int view_routing_id,
    RequestFilterManager::RequestHandler* request_handler,
    RequestFilterManager::RequestIDGenerator* request_id_generator,
    std::optional<int64_t> navigation_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        forwarding_header_client,
    RequestFilterManager::ProxySet* proxies,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto proxy = std::make_unique<RequestFilterProxyingURLLoaderFactory>(
      browser_context, render_process_id, frame_routing_id, view_routing_id,
      request_handler, request_id_generator, std::move(navigation_id),
      factory_builder, std::move(header_client_receiver),
      std::move(forwarding_header_client), proxies, loader_factory_type,
      std::move(navigation_response_task_runner));

  proxies->AddProxy(std::move(proxy));
}

void RequestFilterProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The|web_request_id| doesn't really matter. It just needs to be unique
  // per-BrowserContext so filters can make sense of it.  Note that
  // |network_service_request_id_| by contrast is not necessarily unique, so we
  // don't use it for identity here. This request ID may be the same as a
  // previous request if the previous request was redirected to a URL that
  // required a different loader.
  const uint64_t filtered_request_id =
      request_id_generator_->Generate(view_routing_id_, request_id);

  if (request_id) {
    // Only requests with a non-zero request ID can have their proxy associated
    // with said ID.
    network_request_id_to_filtered_request_id_.emplace(request_id,
                                                       filtered_request_id);
  }

  auto result = requests_.emplace(
      filtered_request_id,
      std::make_unique<InProgressRequest>(
          this, filtered_request_id, request_id, view_routing_id_,
          frame_routing_id_, options, request, traffic_annotation,
          std::move(loader_receiver), std::move(client),
          navigation_response_task_runner_));
  result.first->second->Restart();
}

void RequestFilterProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void RequestFilterProxyingURLLoaderFactory::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  auto it = network_request_id_to_filtered_request_id_.find(request_id);
  if (it == network_request_id_to_filtered_request_id_.end()) {
    if (forwarding_url_loader_header_client_) {
      forwarding_url_loader_header_client_->OnLoaderCreated(
          request_id, std::move(receiver));
    }
    return;
  }

  mojo::PendingRemote<network::mojom::TrustedHeaderClient>
      forwarding_header_client;
  if (forwarding_url_loader_header_client_) {
    forwarding_url_loader_header_client_->OnLoaderCreated(
        request_id, forwarding_header_client.InitWithNewPipeAndPassReceiver());
  }
  auto request_it = requests_.find(it->second);
  CHECK(request_it != requests_.end(), base::NotFatalUntil::M130);
  request_it->second->OnLoaderCreated(std::move(receiver),
                                      std::move(forwarding_header_client));
}

void RequestFilterProxyingURLLoaderFactory::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  // Please note that the URLLoader is now starting, without waiting for
  // additional signals from here. The URLLoader will be blocked before
  // sending HTTP request headers (TrustedHeaderClient.OnBeforeSendHeaders),
  // but the connection set up will be done before that. This is acceptable from
  // the request filter API because the filters have already allowed to set up
  // a connection to the same URL (i.e., the actual request), and distinguishing
  // two connections for the actual request and the preflight request before
  // sending request headers is very difficult.
  const uint64_t web_request_id =
      request_id_generator_->Generate(MSG_ROUTING_NONE, 0);

  auto result = requests_.insert(std::make_pair(
      web_request_id, std::make_unique<InProgressRequest>(
                          this, web_request_id, frame_routing_id_, request)));

  mojo::PendingRemote<network::mojom::TrustedHeaderClient>
      forwarding_header_client;
  if (forwarding_url_loader_header_client_) {
    forwarding_url_loader_header_client_->OnLoaderForCorsPreflightCreated(
        request, forwarding_header_client.InitWithNewPipeAndPassReceiver());
  }

  result.first->second->OnLoaderCreated(std::move(receiver),
                                        std::move(forwarding_header_client));
  result.first->second->Restart();
}

RequestFilterProxyingURLLoaderFactory::
    ~RequestFilterProxyingURLLoaderFactory() = default;

void RequestFilterProxyingURLLoaderFactory::OnTargetFactoryError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  target_factory_.reset();
  proxy_receivers_.Clear();

  MaybeRemoveProxy();
}

void RequestFilterProxyingURLLoaderFactory::OnProxyBindingError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  MaybeRemoveProxy();
}

void RequestFilterProxyingURLLoaderFactory::RemoveRequest(
    int32_t network_service_request_id,
    uint64_t request_id) {
  network_request_id_to_filtered_request_id_.erase(network_service_request_id);
  requests_.erase(request_id);

  MaybeRemoveProxy();
}

void RequestFilterProxyingURLLoaderFactory::MaybeRemoveProxy() {
  // We can delete this factory only when
  //  - there are no existing requests, and
  //  - it is impossible for a new request to arrive in the future.
  if (!requests_.empty() || !proxy_receivers_.empty()) {
    return;
  }

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

void RequestFilterProxyingURLLoaderFactory::EnsureAssociatedFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}
}  // namespace vivaldi
