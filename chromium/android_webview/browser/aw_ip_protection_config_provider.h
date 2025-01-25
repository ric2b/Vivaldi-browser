// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_CONFIG_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_CONFIG_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/ip_protection/blind_sign_message_android_impl.h"
#include "components/ip_protection/ip_protection_config_provider_helper.h"
#include "components/ip_protection/ip_protection_proxy_config_fetcher.h"
#include "components/ip_protection/ip_protection_proxy_config_retriever.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace quiche {
class BlindSignAuthInterface;
class BlindSignAuth;
struct BlindSignToken;
}  // namespace quiche

namespace android_webview {

// The result of a fetch of tokens from the IP Protection auth token server.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Keep this in sync with
// AwIpProtectionTokenBatchRequestResult in enums.xml.
enum class AwIpProtectionTryGetAuthTokensResult {
  // The request was successful and resulted in new tokens.
  kSuccess = 0,
  // A transient error, implies that retrying the action (with backoff) is
  // appropriate.
  kFailedBSATransient = 1,
  // A persistent error, implies that the action should not be retried.
  kFailedBSAPersistent = 2,
  // Any other issue calling BSA.
  kFailedBSAOther = 3,
  // The attempt to request tokens failed because IP Protection is disabled by
  // WebView.
  kFailedDisabled = 4,

  kMaxValue = kFailedDisabled,
};

// Fetches IP protection tokens and proxy list on demand for the network
// service.

// TODO(b/346997109): Refactor AwIpProtectionConfigProvider to reduce code
// duplication once a common implementation of IpProtectionConfigGetter is
// added.
class AwIpProtectionConfigProvider
    : public KeyedService,
      public network::mojom::IpProtectionConfigGetter {
 public:
  explicit AwIpProtectionConfigProvider(AwBrowserContext* aw_browser_context);

  ~AwIpProtectionConfigProvider() override;

  AwIpProtectionConfigProvider(const AwIpProtectionConfigProvider&) = delete;
  AwIpProtectionConfigProvider& operator=(const AwIpProtectionConfigProvider&) =
      delete;

  // IpProtectionConfigGetter:
  // Get a batch of blind-signed auth tokens.
  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;
  // Get the list of IP Protection proxies.
  void GetProxyList(GetProxyListCallback callback) override;

  // KeyedService:

  // We do not currently support destroying WebView's browser context. No
  // shutdown code will be executed on termination of the browser process so
  // this is not actually being tested yet. However, we would like to support
  // destroying browser context in the future so this method contains an idea of
  // how this could be done. Note that Shutdown should not be called more than
  // once.
  void Shutdown() override;

  static AwIpProtectionConfigProvider* Get(
      AwBrowserContext* aw_browser_context);

  static bool CanIpProtectionBeEnabled();

  // Checks if IP Protection is disabled.
  bool IsIpProtectionEnabled();

  // Binds Mojo interfaces to be passed to a new network service.
  void AddNetworkService(
      mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
          pending_receiver,
      mojo::PendingRemote<network::mojom::IpProtectionProxyDelegate>
          pending_remote);

  // Like `SetUp()`, but providing values for each of the member variables.
  void SetUpForTesting(
      std::unique_ptr<ip_protection::IpProtectionProxyConfigRetriever>
          ip_protection_proxy_config_retriever,
      std::unique_ptr<ip_protection::BlindSignMessageAndroidImpl>
          blind_sign_message_android_impl,
      quiche::BlindSignAuthInterface* bsa);

 private:
  // Set up
  // `blind_sign_message_android_impl_`,`ip_protection_proxy_config_retriever_`
  // and `bsa_`, if not already initialized.
  void SetUp();

  // `FetchBlindSignedToken()` calls into the `quiche::BlindSignAuth` library to
  // request a blind-signed auth token for use at the IP Protection proxies.
  void FetchBlindSignedToken(int batch_size,
                             network::mojom::IpProtectionProxyLayer proxy_layer,
                             TryGetAuthTokensCallback callback);
  void OnFetchBlindSignedTokenCompleted(
      base::TimeTicks bsa_get_tokens_start_time,
      TryGetAuthTokensCallback callback,
      absl::StatusOr<absl::Span<quiche::BlindSignToken>>);

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      std::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          bsa_tokens,
      TryGetAuthTokensCallback callback,
      AwIpProtectionTryGetAuthTokensResult result);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  std::optional<base::TimeDelta> CalculateBackoff(
      AwIpProtectionTryGetAuthTokensResult result);

  std::unique_ptr<ip_protection::IpProtectionProxyConfigFetcher>
      ip_protection_proxy_config_fetcher_;
  std::unique_ptr<ip_protection::BlindSignMessageAndroidImpl>
      blind_sign_message_android_impl_;
  std::unique_ptr<quiche::BlindSignAuth> blind_sign_auth_;

  // Injected browser context.
  raw_ptr<AwBrowserContext> aw_browser_context_;

  // For testing, BlindSignAuth is accessed via its interface. In production,
  // this is the same pointer as `blind_sign_auth_`.
  raw_ptr<quiche::BlindSignAuthInterface> bsa_ = nullptr;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`. `last_try_get_auth_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver.
  AwIpProtectionTryGetAuthTokensResult last_try_get_auth_tokens_result_ =
      AwIpProtectionTryGetAuthTokensResult::kSuccess;
  std::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The `mojo::Receiver` objects allowing the network service to call methods
  // on `this`.
  mojo::ReceiverSet<network::mojom::IpProtectionConfigGetter> receivers_;

  // Similar to `receivers_`, but containing remotes for all existing
  // IpProtectionProxyDelegates.
  mojo::RemoteSet<network::mojom::IpProtectionProxyDelegate> remotes_;

  // This must be the last member in this class.
  base::WeakPtrFactory<AwIpProtectionConfigProvider> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_IP_PROTECTION_CONFIG_PROVIDER_H_
