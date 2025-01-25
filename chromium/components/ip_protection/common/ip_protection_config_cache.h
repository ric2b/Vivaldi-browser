// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_CACHE_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_CACHE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "components/ip_protection/common/ip_protection_token_manager.h"

namespace ip_protection {

// A cache for blind-signed auth tokens.
//
// There is no API to fill the cache - it is the implementation's responsibility
// to do that itself.
//
// This class provides sync access to a token, returning nullopt if none is
// available, thereby avoiding adding latency to proxied requests.
class IpProtectionConfigCache {
 public:
  virtual ~IpProtectionConfigCache() = default;

  // Check whether tokens are available in all token caches.
  //
  // This function is called on every URL load, so it should complete quickly.
  virtual bool AreAuthTokensAvailable() = 0;

  // Get a token, if one is available.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason. This method may return `nullopt` even if
  // `IsAuthTokenAvailable()` recently returned `true`.
  virtual std::optional<BlindSignedAuthToken> GetAuthToken(
      size_t chain_index) = 0;

  // Invalidate any previous instruction that token requests should not be
  // made until after a specified time.
  virtual void InvalidateTryAgainAfterTime() = 0;

  // Check whether a proxy chain list is available.
  virtual bool IsProxyListAvailable() = 0;

  // Notify the ConfigCache that QUIC proxies failed for a request, suggesting
  // that QUIC may not work on this network.
  virtual void QuicProxiesFailed() = 0;

  // Return the currently cached proxy chain lists. This contains the lists of
  // hostnames corresponding to each proxy chain that should be used. This
  // may be empty even if `IsProxyListAvailable()` returned true.
  virtual std::vector<net::ProxyChain> GetProxyChainList() = 0;

  // Request a refresh of the proxy chain list. Call this when it's likely that
  // the proxy chain list is out of date.
  virtual void RequestRefreshProxyList() = 0;

  // Callback function used by `IpProtectionProxyConfigManager` and
  // `IpProtectionTokenManager` to signal a possible geo change due to a
  // refreshed proxy list or refill of tokens.
  virtual void GeoObserved(const std::string& geo_id) = 0;

  // Set the token cache manager for the cache.
  virtual void SetIpProtectionTokenManagerForTesting(
      ProxyLayer proxy_layer,
      std::unique_ptr<IpProtectionTokenManager> ipp_token_manager) = 0;

  // Fetch the token cache manager.
  virtual IpProtectionTokenManager* GetIpProtectionTokenManagerForTesting(
      ProxyLayer proxy_layer) = 0;

  // Set the proxy chain list manager for the cache.
  virtual void SetIpProtectionProxyConfigManagerForTesting(
      std::unique_ptr<IpProtectionProxyConfigManager>
          ipp_proxy_config_manager) = 0;

  // Fetch the proxy chain list manager.
  virtual IpProtectionProxyConfigManager*
  GetIpProtectionProxyConfigManagerForTesting() = 0;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONFIG_CACHE_H_
