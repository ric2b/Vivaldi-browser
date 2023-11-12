// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "components/browser/vivaldi_brand_select.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_version_info.h"

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"

#include "prefs/vivaldi_pref_names.h"

namespace vivaldi {

namespace {
PrefService* client_hints_prefs(nullptr);

std::string GetVivaldiReleaseVersion() {
  return base::NumberToString(GetVivaldiVersion().components()[0]) + "." +
         base::NumberToString(GetVivaldiVersion().components()[1]);
}

}  // namespace

void ClientHintsBrandRegisterProfilePrefs(PrefService* prefs) {
  client_hints_prefs = prefs;
}

void SelectClientHintsBrand(absl::optional<std::string>& brand,
                            std::string& major_version,
                            std::string& full_version) {
  if (!IsVivaldiRunning() || !client_hints_prefs)
    return;

  BrandSelection brand_selection = BrandSelection(
      client_hints_prefs->GetInteger(vivaldiprefs::kVivaldiClientHintsBrand));

  switch (brand_selection) {
    case BrandSelection::kChromeBrand:
      brand.emplace("Google Chrome");
      break;

    case BrandSelection::kEdgeBrand:
      brand.emplace("Microsoft Edge");
      full_version = major_version;  // We can't track full version for Edge
      break;

    case BrandSelection::kVivaldiBrand:
      brand.emplace("Vivaldi");
      major_version = GetVivaldiReleaseVersion();
      full_version = GetVivaldiVersionString();
      break;

    case BrandSelection::kCustomBrand: {
      std::string custom_brand = client_hints_prefs->GetString(
          vivaldiprefs::kVivaldiClientHintsBrandCustomBrand);
      std::string custom_brand_version = client_hints_prefs->GetString(
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
  if (!IsVivaldiRunning() || !client_hints_prefs)
    return;

  BrandSelection brand_selection = BrandSelection(
      client_hints_prefs->GetInteger(vivaldiprefs::kVivaldiClientHintsBrand));

  if (brand_selection == BrandSelection::kVivaldiBrand)
    return;

  if (!client_hints_prefs->GetBoolean(
          vivaldiprefs::kVivaldiClientHintsBrandAppendVivaldi))
    return;

  brands.emplace_back("Vivaldi", GetVivaldiReleaseVersion());
}

}  // namespace vivaldi
