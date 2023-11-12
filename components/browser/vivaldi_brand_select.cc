// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "components/browser/vivaldi_brand_select.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_version_info.h"

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "base/vivaldi_user_agent.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"

#include "prefs/vivaldi_pref_names.h"
#include "vivaldi/base/base/edge_version.h"

namespace vivaldi {

namespace {
PrefService* client_hints_prefs(nullptr);

const BrandConfiguration* custom_brand_config(nullptr);

std::string GetVivaldiReleaseVersion() {
  return base::NumberToString(GetVivaldiVersion().components()[0]) + "." +
         base::NumberToString(GetVivaldiVersion().components()[1]);
}

void ConfigureSpecialBrand(const BrandConfiguration* brand_config) {
  custom_brand_config = brand_config;
}

void UnConfigureSpecialBrand() {
  custom_brand_config = nullptr;
}

}  // namespace

void ClientHintsBrandRegisterProfilePrefs(PrefService* prefs) {
  client_hints_prefs = prefs;
}

void SelectClientHintsBrand(absl::optional<std::string>& brand,
                            std::string& major_version,
                            std::string& full_version) {
  if (!IsVivaldiRunning() || (!client_hints_prefs && !custom_brand_config))
    return;

  BrandSelection brand_selection =
      custom_brand_config ? custom_brand_config->brand
                          : BrandSelection(client_hints_prefs->GetInteger(
                                vivaldiprefs::kVivaldiClientHintsBrand));

  switch (brand_selection) {
    case BrandSelection::kChromeBrand:
      brand.emplace("Google Chrome");
      break;

    case BrandSelection::kEdgeBrand:
      brand.emplace("Microsoft Edge");
      full_version = EDGE_FULL_VERSION;
      break;

    case BrandSelection::kVivaldiBrand:
      brand.emplace("Vivaldi");
      major_version = GetVivaldiReleaseVersion();
      full_version = GetVivaldiVersionString();
      break;

    case BrandSelection::kCustomBrand: {
      std::string custom_brand =
          custom_brand_config
              ? custom_brand_config->customBrand
              : client_hints_prefs->GetString(
                    vivaldiprefs::kVivaldiClientHintsBrandCustomBrand);
      std::string custom_brand_version =
          custom_brand_config
              ? custom_brand_config->customBrandVersion
              : client_hints_prefs->GetString(
                    vivaldiprefs::kVivaldiClientHintsBrandCustomBrandVersion);

      if (!custom_brand.empty() && !custom_brand_version.empty()) {
        brand.emplace(custom_brand);
        major_version = custom_brand_version;
        full_version = custom_brand_version;
      }
    } break;

    case BrandSelection::kNoBrand:
      break;
  }
}

void UpdateBrands(int seed, blink::UserAgentBrandList& brands) {
  if (!IsVivaldiRunning() || (!client_hints_prefs && !custom_brand_config))
    return;

  BrandSelection brand_selection =
      custom_brand_config ? custom_brand_config->brand
                          : BrandSelection(client_hints_prefs->GetInteger(
                                vivaldiprefs::kVivaldiClientHintsBrand));

  if (brand_selection == BrandSelection::kVivaldiBrand)
    return;

  if (!(custom_brand_config ? custom_brand_config->SpecifyVivaldiBrand : client_hints_prefs->GetBoolean(
          vivaldiprefs::kVivaldiClientHintsBrandAppendVivaldi)))
    return;

  brands.emplace_back("Vivaldi", GetVivaldiReleaseVersion());
}

std::string GetBrandFullVersion() {
  if (!IsVivaldiRunning() || (!client_hints_prefs && !custom_brand_config))
    return std::string(version_info::GetVersionNumber());

  BrandSelection brand_selection =
    custom_brand_config ? custom_brand_config->brand
    : BrandSelection(client_hints_prefs->GetInteger(
      vivaldiprefs::kVivaldiClientHintsBrand));

  switch (brand_selection) {
  case BrandSelection::kEdgeBrand:
    return EDGE_FULL_VERSION;
  case BrandSelection::kVivaldiBrand:
    return GetVivaldiVersionString();

  case BrandSelection::kChromeBrand:
  case BrandSelection::kCustomBrand:
  case BrandSelection::kNoBrand:
    return std::string(version_info::GetVersionNumber());
  }
}

void ConfigureClientHintsOverrides() {
  BrandConfiguration vivaldi_brand =
      { BrandSelection::kVivaldiBrand, false, {}, {} };
  ConfigureSpecialBrand(&vivaldi_brand);

  for (auto domain : vivaldi_user_agent::GetVivaldiWhitelist()) {
    blink::UserAgentOverride::AddGetUaMetaDataOverride(
      std::string(domain), embedder_support::GetUserAgentMetadata());
  }

  UnConfigureSpecialBrand();

  BrandConfiguration edge_brand =
     { BrandSelection::kEdgeBrand, false, {}, {} };
  ConfigureSpecialBrand(&edge_brand);

  for (auto domain : vivaldi_user_agent::GetVivaldiEdgeList()) {
    blink::UserAgentOverride::AddGetUaMetaDataOverride(
      domain, embedder_support::GetUserAgentMetadata());
  }

  UnConfigureSpecialBrand();
}

}  // namespace vivaldi
