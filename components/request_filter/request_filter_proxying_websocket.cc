// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter_proxying_websocket.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/ip_endpoint.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#endif

namespace vivaldi {
namespace {

// This shutdown notifier makes sure the proxy is destroyed if an incognito
// browser context is destroyed. This is needed because WebRequestAPI only
// clears the proxies when the original browser context is destroyed.
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
            "RequestFilterProxyingWebSocket") {}
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
    int32_t error_code,
    const std::optional<std::string>& headers,
    const std::optional<GURL>& preserve_fragment_on_redirect_url) {
  if (headers) {
    std::move(callback).Run(error_code, headers,
                            preserve_fragment_on_redirect_url);
  } else {
    std::move(callback).Run(error_code, initial_headers,
                            preserve_fragment_on_redirect_url);
  }
}

}  // namespace

RequestFilterProxyingWebSocket::RequestFilterProxyingWebSocket(
    WebSocketFactory factory,
    const network::ResourceRequest& request,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
        authentication_handler,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
    bool has_extra_headers,
    int process_id,
    int render_frame_id,
    content::BrowserContext* browser_context,
    RequestFilterManager::RequestIDGenerator* request_id_generator,
    RequestFilterManager::RequestHandler* request_handler,
    RequestFilterManager::ProxySet* proxies)
    : factory_(std::move(factory)),
      browser_context_(browser_context),
      request_handler_(request_handler),
      forwarding_handshake_client_(std::move(handshake_client)),
      forwarding_authentication_handler_(std::move(authentication_handler)),
      forwarding_header_client_(std::move(header_client)),
      request_headers_(request.headers),
      response_(network::mojom::URLResponseHead::New()),
      additional_headers_(std::move(additional_headers)),
      has_extra_headers_(has_extra_headers || header_client),
      info_(request_id_generator->Generate(MSG_ROUTING_NONE, 0),
            process_id,
            render_frame_id,
            request,
            content::ContentBrowserClient::URLLoaderFactoryType::
                kDocumentSubResource,
            /*is_async=*/true,
            /*is_webtransport=*/false,
            /*navigation_id=*/std::nullopt),
      proxies_(proxies) {
  // base::Unretained is safe here because the callback will be canceled when
  // |shutdown_notifier_subscription_| is destroyed, and |proxies_| owns this.
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()
          ->Get(browser_context)
          ->Subscribe(
              base::BindRepeating(&RequestFilterManager::ProxySet::RemoveProxy,
                                  base::Unretained(proxies_), this));
}

RequestFilterProxyingWebSocket::~RequestFilterProxyingWebSocket() {
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  request_handler_->OnRequestWillBeDestroyed(browser_context_, &info_);
  if (on_before_send_headers_callback_) {
    std::move(on_before_send_headers_callback_)
        .Run(net::ERR_ABORTED, std::nullopt);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, std::nullopt, std::nullopt);
  }
}

void RequestFilterProxyingWebSocket::Start() {
  // If the header client will be used, we start the request immediately, and
  // OnBeforeSendHeaders and OnSendHeaders will be handled there. Otherwise,
  // send these events before the request starts.
  base::RepeatingCallback<void(int)> continuation;
  if (has_extra_headers_) {
    continuation = base::BindRepeating(
        &RequestFilterProxyingWebSocket::ContinueToStartRequest,
        weak_factory_.GetWeakPtr());
  } else {
    continuation = base::BindRepeating(
        &RequestFilterProxyingWebSocket::OnBeforeRequestComplete,
        weak_factory_.GetWeakPtr());
  }

  // TODO(yhirano): Consider having throttling here (probably with aligned with
  // WebRequestProxyingURLLoaderFactory).
  int result = request_handler_->OnBeforeRequest(
      browser_context_, &info_, continuation, &redirect_url_, nullptr);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  continuation.Run(net::OK);
}

void RequestFilterProxyingWebSocket::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK(forwarding_handshake_client_);
  forwarding_handshake_client_->OnOpeningHandshakeStarted(std::move(request));
}

void RequestFilterProxyingWebSocket::OnFailure(const std::string&,
                                               int net_error,
                                               int response_code) {}

void RequestFilterProxyingWebSocket::ContinueToHeadersReceived() {
  auto continuation = base::BindRepeating(
      &RequestFilterProxyingWebSocket::OnHeadersReceivedComplete,
      weak_factory_.GetWeakPtr());
  int result = request_handler_->OnHeadersReceived(
      browser_context_, &info_, continuation, response_->headers.get(),
      &override_headers_, &redirect_url_, nullptr);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedComplete(net::OK);
}

void RequestFilterProxyingWebSocket::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> websocket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  DCHECK(forwarding_handshake_client_);
  DCHECK(!is_done_);
  is_done_ = true;
  websocket_ = std::move(websocket);
  client_receiver_ = std::move(client_receiver);
  handshake_response_ = std::move(response);
  readable_ = std::move(readable);
  writable_ = std::move(writable);

  response_->remote_endpoint = handshake_response_->remote_endpoint;

  // response_->headers will be set in OnBeforeSendHeaders if
  // |receiver_as_header_client_| is set.
  if (receiver_as_header_client_.is_bound()) {
    ContinueToCompleted();
    return;
  }

  response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(base::StringPrintf(
          "HTTP/%d.%d %d %s", handshake_response_->http_version.major_value(),
          handshake_response_->http_version.minor_value(),
          handshake_response_->status_code,
          handshake_response_->status_text.c_str()));
  for (const auto& header : handshake_response_->headers) {
    response_->headers->AddHeader(header->name, header->value);
  }

  ContinueToHeadersReceived();
}

void RequestFilterProxyingWebSocket::ContinueToCompleted() {
  DCHECK(forwarding_handshake_client_);
  DCHECK(is_done_);
  request_handler_->OnCompleted(browser_context_, &info_, net::ERR_WS_UPGRADE);
  forwarding_handshake_client_->OnConnectionEstablished(
      std::move(websocket_), std::move(client_receiver_),
      std::move(handshake_response_), std::move(readable_),
      std::move(writable_));

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

void RequestFilterProxyingWebSocket::OnAuthRequired(
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const net::IPEndPoint& remote_endpoint,
    OnAuthRequiredCallback callback) {
  if (!callback) {
    OnError(net::ERR_FAILED);
    return;
  }

  response_->headers = headers;
  response_->remote_endpoint = remote_endpoint;
  auth_required_callback_ = std::move(callback);

  auto continuation = base::BindRepeating(
      &RequestFilterProxyingWebSocket::OnHeadersReceivedCompleteForAuth,
      weak_factory_.GetWeakPtr(), auth_info);
  int result = request_handler_->OnHeadersReceived(
      browser_context_, &info_, continuation, response_->headers.get(),
      &override_headers_, &redirect_url_, nullptr);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedCompleteForAuth(auth_info, net::OK);
}

void RequestFilterProxyingWebSocket::OnBeforeSendHeaders(
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  DCHECK(receiver_as_header_client_.is_bound());

  request_headers_ = headers;
  on_before_send_headers_callback_ = std::move(callback);
  OnBeforeRequestComplete(net::OK);
}

void RequestFilterProxyingWebSocket::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& endpoint,
    OnHeadersReceivedCallback callback) {
  DCHECK(receiver_as_header_client_.is_bound());

  on_headers_received_callback_ = std::move(callback);
  response_->headers = base::MakeRefCounted<net::HttpResponseHeaders>(headers);

  ContinueToHeadersReceived();
}

void RequestFilterProxyingWebSocket::StartProxying(
    WebSocketFactory factory,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
        authentication_handler,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient> header_client,
    bool has_extra_headers,
    int process_id,
    int render_frame_id,
    RequestFilterManager::RequestIDGenerator* request_id_generator,
    RequestFilterManager::RequestHandler* request_handler,
    const url::Origin& origin,
    content::BrowserContext* browser_context,
    RequestFilterManager::ProxySet* proxies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network::ResourceRequest request;
  request.url = url;
  request.site_for_cookies = site_for_cookies;
  if (user_agent) {
    request.headers.SetHeader(net::HttpRequestHeaders::kUserAgent, *user_agent);
  }
  request.request_initiator = origin;

  auto proxy = std::make_unique<RequestFilterProxyingWebSocket>(
      std::move(factory), request, std::move(additional_headers),
      std::move(handshake_client), std::move(authentication_handler),
      std::move(header_client), has_extra_headers, process_id, render_frame_id,
      browser_context, request_id_generator, request_handler, proxies);

  auto* raw_proxy = proxy.get();
  proxies->AddProxy(std::move(proxy));
  raw_proxy->Start();
}

void RequestFilterProxyingWebSocket::OnBeforeRequestComplete(int error_code) {
  DCHECK(receiver_as_header_client_.is_bound() ||
         !receiver_as_handshake_client_.is_bound());
  DCHECK(info_.request.url.SchemeIsWSOrWSS());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  auto continuation = base::BindRepeating(
      &RequestFilterProxyingWebSocket::OnBeforeSendHeadersComplete,
      weak_factory_.GetWeakPtr());

  int result = request_handler_->OnBeforeSendHeaders(
      browser_context_, &info_, continuation, &request_headers_, nullptr,
      nullptr);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnBeforeSendHeadersComplete(net::OK);
}

void RequestFilterProxyingWebSocket::OnBeforeSendHeadersComplete(
    int error_code) {
  DCHECK(receiver_as_header_client_.is_bound() ||
         !receiver_as_handshake_client_.is_bound());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  if (receiver_as_header_client_.is_bound()) {
    DCHECK(on_before_send_headers_callback_);
    if (forwarding_header_client_) {
      forwarding_header_client_->OnBeforeSendHeaders(
          request_headers_,
          base::BindOnce(&ForwardOnBeforeSendHeadersCallback,
                         std::move(on_before_send_headers_callback_),
                         request_headers_));
    } else {
      std::move(on_before_send_headers_callback_)
          .Run(error_code, request_headers_);
    }
  }

  request_handler_->OnSendHeaders(browser_context_, &info_, request_headers_);

  if (!receiver_as_header_client_.is_bound()) {
    ContinueToStartRequest(net::OK);
  }
}

void RequestFilterProxyingWebSocket::ContinueToStartRequest(int error_code) {
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  base::flat_set<std::string> used_header_names;
  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  for (net::HttpRequestHeaders::Iterator it(request_headers_); it.GetNext();) {
    additional_headers.push_back(
        network::mojom::HttpHeader::New(it.name(), it.value()));
    used_header_names.insert(base::ToLowerASCII(it.name()));
  }
  for (const auto& header : additional_headers_) {
    if (!used_header_names.contains(base::ToLowerASCII(header->name))) {
      additional_headers.push_back(
          network::mojom::HttpHeader::New(header->name, header->value));
    }
  }

  mojo::PendingRemote<network::mojom::TrustedHeaderClient>
      trusted_header_client = mojo::NullRemote();
  if (has_extra_headers_) {
    trusted_header_client =
        receiver_as_header_client_.BindNewPipeAndPassRemote();
  }

  std::move(factory_).Run(
      info_.request.url, std::move(additional_headers),
      receiver_as_handshake_client_.BindNewPipeAndPassRemote(),
      receiver_as_auth_handler_.BindNewPipeAndPassRemote(),
      std::move(trusted_header_client));

  // Here we detect mojo connection errors on |receiver_as_handshake_client_|.
  // See also CreateWebSocket in
  // //network/services/public/mojom/network_context.mojom.
  receiver_as_handshake_client_.set_disconnect_with_reason_handler(
      base::BindOnce(&RequestFilterProxyingWebSocket::
                         OnMojoConnectionErrorWithCustomReason,
                     base::Unretained(this)));
  forwarding_handshake_client_.set_disconnect_handler(
      base::BindOnce(&RequestFilterProxyingWebSocket::OnMojoConnectionError,
                     base::Unretained(this)));
}

void RequestFilterProxyingWebSocket::OnHeadersReceivedComplete(int error_code) {
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  if (on_headers_received_callback_) {
    std::optional<std::string> headers;
    if (override_headers_) {
      headers = override_headers_->raw_headers();
    }
    if (forwarding_header_client_) {
      forwarding_header_client_->OnHeadersReceived(
          headers ? *headers : response_->headers->raw_headers(),
          response_->remote_endpoint,
          base::BindOnce(&ForwardOnHeaderReceivedCallback,
                         std::move(on_headers_received_callback_), headers));
    } else {
      std::move(on_headers_received_callback_)
          .Run(net::OK, headers, std::nullopt);
    }
  }

  if (override_headers_) {
    response_->headers = override_headers_;
    override_headers_ = nullptr;
  }

  ResumeIncomingMethodCallProcessing();
  info_.AddResponse(*response_);
  request_handler_->OnResponseStarted(browser_context_, &info_, net::OK);

  if (!receiver_as_header_client_.is_bound()) {
    ContinueToCompleted();
  }
}

void RequestFilterProxyingWebSocket::OnHeadersReceivedCompleteForAuth(
    const net::AuthChallengeInfo& auth_info,
    int rv) {
  if (rv != net::OK) {
    OnError(rv);
    return;
  }
  ResumeIncomingMethodCallProcessing();
  info_.AddResponse(*response_);

  if (forwarding_authentication_handler_) {
    forwarding_authentication_handler_->OnAuthRequired(
        auth_info, response_->headers, response_->remote_endpoint,
        std::move(auth_required_callback_));
  } else {
    std::move(auth_required_callback_).Run(std::nullopt);
  }
}

void RequestFilterProxyingWebSocket::PauseIncomingMethodCallProcessing() {
  receiver_as_handshake_client_.Pause();
  receiver_as_auth_handler_.Pause();
  if (receiver_as_header_client_.is_bound()) {
    receiver_as_header_client_.Pause();
  }
}

void RequestFilterProxyingWebSocket::ResumeIncomingMethodCallProcessing() {
  receiver_as_handshake_client_.Resume();
  receiver_as_auth_handler_.Resume();
  if (receiver_as_header_client_.is_bound()) {
    receiver_as_header_client_.Resume();
  }
}

void RequestFilterProxyingWebSocket::OnError(int error_code) {
  if (!is_done_) {
    is_done_ = true;
    request_handler_->OnErrorOccurred(browser_context_, &info_,
                                      true /* started */, error_code);
  }

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

void RequestFilterProxyingWebSocket::OnMojoConnectionErrorWithCustomReason(
    uint32_t custom_reason,
    const std::string& description) {
  // Here we want to nofiy the custom reason to the client, which is why
  // we reset |forwarding_handshake_client_| manually.
  forwarding_handshake_client_.ResetWithReason(custom_reason, description);
  OnError(net::ERR_FAILED);
  // Deletes |this|.
}

void RequestFilterProxyingWebSocket::OnMojoConnectionError() {
  OnError(net::ERR_FAILED);
  // Deletes |this|.
}

// static
void RequestFilterProxyingWebSocket::EnsureAssociatedFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

}  // namespace vivaldi
