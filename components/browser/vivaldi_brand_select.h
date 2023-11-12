// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BROWSER_VIVALDI_BRAND_SELECT_H_
#define COMPONENTS_BROWSER_VIVALDI_BRAND_SELECT_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

class PrefService;

namespace vivaldi {
enum class BrandSelection {
  kNoBrand = 0,
  kVivaldiBrand = 1,
  kCustomBrand = 2,
  kChromeBrand = 3,
  kEdgeBrand = 4,
};

struct BrandConfiguration {
  BrandSelection brand;
  bool SpecifyVivaldiBrand;
  std::string customBrand;
  std::string customBrandVersion;
};

void ClientHintsBrandRegisterProfilePrefs(PrefService*);

void SelectClientHintsBrand(absl::optional<std::string>& brand,
                            std::string& major_version,
                            std::string& full_version);

void UpdateBrands(int seed, blink::UserAgentBrandList& brands);

void ConfigureClientHintsOverrides();

std::string GetBrandFullVersion();

}  // namespace vivaldi

#endif  // COMPONENTS_BROWSER_VIVALDI_BRAND_SELECT_H_
