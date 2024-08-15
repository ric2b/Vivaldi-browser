// Copyright (c) 2022 Vivaldi. All rights reserved.

#include "browser/stats_reporter_impl.h"

#include "base/json/json_string_value_serializer.h"
#include "chrome/browser/browser_process.h"
#include "components/browser/vivaldi_brand_select.h"
#include "components/embedder_support/user_agent_utils.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "base/logging.h"

namespace vivaldi {

using network::mojom::WebClientHintsType;

// static
PrefService* StatsReporterImpl::GetLocalState() {
  return g_browser_process->local_state();
}

// static
scoped_refptr<network::SharedURLLoaderFactory>
StatsReporterImpl::GetUrlLoaderFactory() {
  return g_browser_process->shared_url_loader_factory();
}

// static
std::string StatsReporterImpl::GetUserAgent() {
  return embedder_support::GetUserAgent();
}

// static
std::string StatsReporterImpl::GetClientHints() {
  BrandOverride brand_override(
      BrandConfiguration({BrandSelection::kVivaldiBrand, true, {}, {}}));

  blink::UserAgentMetadata metadata = embedder_support::GetUserAgentMetadata();

  base::Value::Dict client_hints;

  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUA),
      base::Value(metadata.SerializeBrandMajorVersionList()));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUAArch),
      base::Value(metadata.architecture));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUAPlatform),
      base::Value(metadata.platform));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUAModel),
      base::Value(metadata.model));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUAMobile),
      base::Value(metadata.mobile));
  client_hints.Set(network::GetClientHintToNameMap().at(
                       WebClientHintsType::kUAPlatformVersion),
                   base::Value(metadata.platform_version));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUABitness),
      base::Value(metadata.bitness));
  client_hints.Set(network::GetClientHintToNameMap().at(
                       WebClientHintsType::kUAFullVersionList),
                   base::Value(metadata.SerializeBrandFullVersionList()));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUAWoW64),
      base::Value(metadata.wow64));
  client_hints.Set(
      network::GetClientHintToNameMap().at(WebClientHintsType::kUAFormFactors),
      base::Value(metadata.SerializeFormFactors()));

  std::string result;

  if (!JSONStringValueSerializer(&result).Serialize(client_hints)) {
    return "{}";
  }
  LOG(INFO) << result;
  return result;
}

}  // namespace vivaldi
