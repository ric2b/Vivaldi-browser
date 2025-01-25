// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_CACHE_IMPL_H_

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ip_protection/common/ip_protection_config_cache.h"
#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"

namespace ip_protection {

// An implementation of IpProtectionConfigCache that fills itself by making
// IPC calls to the IpProtectionConfigGetter in the browser process.
class IpProtectionConfigCacheImpl
    : public IpProtectionConfigCache,
      net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // If `config_getter` is unbound, no tokens will be provided.
  explicit IpProtectionConfigCacheImpl(
      std::unique_ptr<IpProtectionConfigGetter> config_getter);
  ~IpProtectionConfigCacheImpl() override;

  // IpProtectionConfigCache implementation.
  bool AreAuthTokensAvailable() override;
  std::optional<BlindSignedAuthToken> GetAuthToken(size_t chain_index) override;
  void InvalidateTryAgainAfterTime() override;
  bool IsProxyListAvailable() override;
  void QuicProxiesFailed() override;
  std::vector<net::ProxyChain> GetProxyChainList() override;
  void RequestRefreshProxyList() override;
  void GeoObserved(const std::string& geo_id) override;
  void SetIpProtectionTokenManagerForTesting(
      ProxyLayer proxy_layer,
      std::unique_ptr<IpProtectionTokenManager> ipp_token_manager) override;
  IpProtectionTokenManager* GetIpProtectionTokenManagerForTesting(
      ProxyLayer proxy_layer) override;
  void SetIpProtectionProxyConfigManagerForTesting(
      std::unique_ptr<IpProtectionProxyConfigManager> ipp_proxy_config_manager)
      override;
  IpProtectionProxyConfigManager* GetIpProtectionProxyConfigManagerForTesting()
      override;

  // `NetworkChangeNotifier::NetworkChangeObserver` implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  // Source of auth tokens and proxy list, when needed.
  std::unique_ptr<IpProtectionConfigGetter> config_getter_;

  // A manager for the list of currently cached proxy hostnames.
  std::unique_ptr<IpProtectionProxyConfigManager> ipp_proxy_config_manager_;

  // Proxy layer managers for cache of blind-signed auth tokens.
  std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>
      ipp_token_managers_;

  // If true, this class will try to connect to IP Protection proxies via QUIC.
  // Once this value becomes false, it stays false until a network change or
  // browser restart.
  bool ipp_over_quic_;

  // Feature flag to safely introduce token caching by geo.
  const bool enable_token_caching_by_geo_;

  base::WeakPtrFactory<IpProtectionConfigCacheImpl> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
