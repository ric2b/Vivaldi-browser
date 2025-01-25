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
  bool specify_vivaldi_brand;
  std::string custom_brand;
  std::string custom_brand_version;
};

class BrandOverride {
 public:
  BrandOverride(BrandConfiguration brand_config);
  ~BrandOverride();

  BrandOverride(const BrandOverride&) = delete;
  BrandOverride& operator=(const BrandOverride&) = delete;

 private:
  BrandConfiguration brand_config_;
};

void ClientHintsBrandRegisterProfilePrefs(PrefService*);

void SelectClientHintsBrand(std::optional<std::string>& brand,
                            std::string& major_version,
                            std::string& full_version);

void UpdateBrands(
    std::optional<blink::UserAgentBrandVersion>& additional_brand_version);

void ConfigureClientHintsOverrides();

std::string GetBrandFullVersion();
}  // namespace vivaldi

#endif  // COMPONENTS_BROWSER_VIVALDI_BRAND_SELECT_H_
