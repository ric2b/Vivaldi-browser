// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_PROXYING_URL_LOADER_FACTORY_H_
#define COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_PROXYING_URL_LOADER_FACTORY_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/request_filter/request_filter_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class HttpRequestHeaders;
class HttpResponseHeaders;
class IPEndPoint;
struct RedirectInfo;
}  // namespace net

namespace network {
struct URLLoaderCompletionStatus;
}

namespace vivaldi {

// Owns URLLoaderFactory bindings for RequestFilterManager proxies with the
// Network Service enabled.
class RequestFilterProxyingURLLoaderFactory
    : public RequestFilterManager::Proxy,
      public network::mojom::URLLoaderFactory,
      public network::mojom::TrustedURLLoaderHeaderClient {
 public:
  class InProgressRequest : public network::mojom::URLLoader,
                            public network::mojom::URLLoaderClient,
                            public network::mojom::TrustedHeaderClient {
   public:
    // For usual requests
    InProgressRequest(
        RequestFilterProxyingURLLoaderFactory* factory,
        uint64_t request_id,
        int32_t network_service_request_id,
        int32_t view_routing_id,
        int32_t frame_routing_id,
        uint32_t options,
        const network::ResourceRequest& request,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
        mojo::PendingReceiver<network::mojom::URLLoader>,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        scoped_refptr<base::SequencedTaskRunner>
            navigation_response_task_runner);
    // For CORS preflights
    InProgressRequest(RequestFilterProxyingURLLoaderFactory* factory,
                      uint64_t request_id,
                      int32_t frame_routing_id,
                      const network::ResourceRequest& request);

    InProgressRequest(const InProgressRequest&) = delete;
    InProgressRequest& operator=(const InProgressRequest&) = delete;

    ~InProgressRequest() override;

    void Restart();

    // network::mojom::URLLoader:
    void FollowRedirect(
        const std::vector<std::string>& removed_headers,
        const net::HttpRequestHeaders& modified_headers,
        const net::HttpRequestHeaders& modified_cors_exempt_headers,
        const std::optional<GURL>& new_url) override;
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override;
    void PauseReadingBodyFromNet() override;
    void ResumeReadingBodyFromNet() override;

    // network::mojom::URLLoaderClient:
    void OnReceiveEarlyHints(
        network::mojom::EarlyHintsPtr early_hints) override;
    void OnReceiveResponse(
        network::mojom::URLResponseHeadPtr head,
        mojo::ScopedDataPipeConsumerHandle body,
        std::optional<::mojo_base::BigBuffer> cached_metadata) override;
    void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                           network::mojom::URLResponseHeadPtr head) override;
    void OnUploadProgress(int64_t current_position,
                          int64_t total_size,
                          OnUploadProgressCallback callback) override;
    void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
    void OnComplete(const network::URLLoaderCompletionStatus& status) override;

    void OnLoaderCreated(
        mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver,
        mojo::PendingRemote<network::mojom::TrustedHeaderClient>
            forwarding_header_client);

    // network::mojom::TrustedHeaderClient:
    void OnBeforeSendHeaders(const net::HttpRequestHeaders& headers,
                             OnBeforeSendHeadersCallback callback) override;
    void OnHeadersReceived(const std::string& headers,
                           const net::IPEndPoint& endpoint,
                           OnHeadersReceivedCallback callback) override;

   private:
    // The state of an InprogressRequest. Not really used, but we want it to
    // make it easier to merge code changes form
    // WebRequestProxyingURLLoaderFactory.
    enum State {
      kInProgress = 0,
      kInProgressWithFinalResponseReceived,
      kInvalid,  // This is an invalid state and must not be recorded.
      kRedirectFollowedByAnotherInProgressRequest,
      kRejectedByNetworkError,
      kRejectedByNetworkErrorAfterReceivingFinalResponse,
      kDetachedFromClient,
      kDetachedFromClientAfterReceivingResponse,
      kRejectedByOnBeforeRequest,
      kRejectedByOnBeforeSendHeaders,
      kRejectedByOnHeadersReceivedForFinalResponse,
      kRejectedByOnHeadersReceivedForRedirect,
      kRejectedByOnHeadersReceivedForAuth,
      kRejectedByOnAuthRequired,
      kCompleted,
      kMaxValue = kCompleted,
    };

    // These two methods combined form the implementation of Restart().
    void UpdateRequestInfo();
    void RestartInternal();

    void ContinueToBeforeSendHeaders(State state_on_error, int error_code);
    void ContinueToBeforeSendHeadersWithOk();
    void ContinueToSendHeaders(State state_on_error, int error_code);
    void ContinueToSendHeadersWithOk();
    void ContinueToStartRequest(State state_on_error, int error_code);
    void ContinueToStartRequestWithOk();
    void ContinueToHandleOverrideHeaders(int error_code);
    void OverwriteHeadersAndContinueToResponseStarted(int error_code);
    void AssignParsedHeadersAndContinueToResponseStarted(
        network::mojom::ParsedHeadersPtr parsed_headers);
    void ContinueToResponseStarted();
    void ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                                  int error_code);
    void HandleResponseOrRedirectHeaders(
        net::CompletionOnceCallback continuation);
    void OnLoaderDisconnected(uint32_t custom_reason,
                              const std::string& description);
    void OnRequestError(const network::URLLoaderCompletionStatus& status,
                        State state);
    void OnNetworkError(const network::URLLoaderCompletionStatus& status);
    void OnClientDisconnected();
    bool IsRedirectSafe(const GURL& from_url,
                        const GURL& to_url,
                        bool is_navigation_request);
    void HandleBeforeRequestRedirect();

    network::URLLoaderCompletionStatus CreateURLLoaderCompletionStatus(
        int error_code,
        bool collapse_initiator = false);

    const raw_ptr<RequestFilterProxyingURLLoaderFactory> factory_;
    network::ResourceRequest request_;
    const std::optional<url::Origin> original_initiator_;
    const uint64_t request_id_ = 0;
    const int32_t network_service_request_id_ = 0;
    const int32_t view_routing_id_ = MSG_ROUTING_NONE;
    const int32_t frame_routing_id_ = MSG_ROUTING_NONE;
    const uint32_t options_ = 0;
    const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
    mojo::Receiver<network::mojom::URLLoader> proxied_loader_receiver_;
    mojo::Remote<network::mojom::URLLoaderClient> target_client_;

    std::optional<FilteredRequestInfo> info_;

    mojo::Receiver<network::mojom::URLLoaderClient> proxied_client_receiver_{
        this};
    mojo::Remote<network::mojom::URLLoader> target_loader_;

    // NOTE: This is state which ExtensionWebRequestEventRouter needs to have
    // persisted across some phases of this request -- namely between
    // |OnHeadersReceived()| and request completion or restart. Pointers to
    // these fields are stored in a |BlockedRequest| (created and owned by
    // ExtensionWebRequestEventRouter) through much of the request's lifetime.
    network::mojom::URLResponseHeadPtr current_response_;
    mojo::ScopedDataPipeConsumerHandle current_body_;
    std::optional<mojo_base::BigBuffer> current_cached_metadata_;
    scoped_refptr<net::HttpResponseHeaders> override_headers_;
    std::set<std::string> set_request_headers_;
    std::set<std::string> removed_request_headers_;
    bool collapse_initiator_;
    GURL redirect_url_;

    const bool for_cors_preflight_ = false;

    // If |has_any_extra_headers_listeners_| is set to true, the request will be
    // sent with the network::mojom::kURLLoadOptionUseHeaderClient option, and
    // we expect events to come through the
    // network::mojom::TrustedURLLoaderHeaderClient binding on the factory. This
    // is only set to true if there is a listener that needs to view or modify
    // headers set in the network process.
    const bool has_any_extra_headers_listeners_ = false;
    bool current_request_uses_header_client_ = false;
    OnBeforeSendHeadersCallback on_before_send_headers_callback_;
    OnHeadersReceivedCallback on_headers_received_callback_;
    mojo::Receiver<network::mojom::TrustedHeaderClient> header_client_receiver_{
        this};
    mojo::Remote<network::mojom::TrustedHeaderClient> forwarding_header_client_;
    bool is_header_client_receiver_paused_ = false;

    // If |has_any_extra_headers_listeners_| is set to false and a redirect is
    // in progress, this stores the parameters to FollowRedirect that came from
    // the client. That way we can combine it with any other changes that
    // filters made to headers in their callbacks.
    struct FollowRedirectParams {
      FollowRedirectParams();
      FollowRedirectParams(const FollowRedirectParams&) = delete;
      FollowRedirectParams& operator=(const FollowRedirectParams&) = delete;
      ~FollowRedirectParams();
      std::vector<std::string> removed_headers;
      net::HttpRequestHeaders modified_headers;
      net::HttpRequestHeaders modified_cors_exempt_headers;
      std::optional<GURL> new_url;
    };
    std::unique_ptr<FollowRedirectParams> pending_follow_redirect_params_;
    State state_ = State::kInProgress;

    // A task runner that should be used for the request when non-null. Non-null
    // when this was created for a navigation request.
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner_;

    base::WeakPtrFactory<InProgressRequest> weak_factory_{this};
  };

  RequestFilterProxyingURLLoaderFactory(
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
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);

  RequestFilterProxyingURLLoaderFactory(
      const RequestFilterProxyingURLLoaderFactory&) = delete;
  RequestFilterProxyingURLLoaderFactory& operator=(
      const RequestFilterProxyingURLLoaderFactory&) = delete;

  ~RequestFilterProxyingURLLoaderFactory() override;

  static void StartProxying(
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
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_request,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

  // network::mojom::TrustedURLLoaderHeaderClient:
  void OnLoaderCreated(
      int32_t request_id,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;
  void OnLoaderForCorsPreflightCreated(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver)
      override;

  content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type()
      const {
    return loader_factory_type_;
  }

  static void EnsureAssociatedFactoryBuilt();

 private:
  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(int32_t network_service_request_id, uint64_t request_id);
  void MaybeRemoveProxy();

  const raw_ptr<content::BrowserContext> browser_context_;
  const int render_process_id_;
  const int frame_routing_id_;
  const int view_routing_id_;
  const raw_ptr<RequestFilterManager::RequestHandler> request_handler_;
  const raw_ptr<RequestFilterManager::RequestIDGenerator> request_id_generator_;
  std::optional<int64_t> navigation_id_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Receiver<network::mojom::TrustedURLLoaderHeaderClient>
      url_loader_header_client_receiver_{this};
  mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient>
      forwarding_url_loader_header_client_;
  // Owns |this|.
  const raw_ptr<RequestFilterManager::ProxySet> proxies_;

  const content::ContentBrowserClient::URLLoaderFactoryType
      loader_factory_type_;

  // Mapping from our own internally generated request ID to an
  // InProgressRequest instance.
  std::map<uint64_t, std::unique_ptr<InProgressRequest>> requests_;

  // A mapping from the network stack's notion of request ID to our own
  // internally generated request ID for the same request.
  std::map<int32_t, uint64_t> network_request_id_to_filtered_request_id_;

  // Notifies the proxy that the browser context has been shutdown.
  base::CallbackListSubscription shutdown_notifier_subscription_;

  // A task runner that should be used for requests when non-null. Non-null when
  // this was created for a navigation request.
  scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner_;

  base::WeakPtrFactory<RequestFilterProxyingURLLoaderFactory> weak_factory_{
      this};
};

}  // namespace vivaldi

#endif  // COMPONENTS_REQUEST_FILTER_REQUEST_FILTER_PROXYING_URL_LOADER_FACTORY_H_
