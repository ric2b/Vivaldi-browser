// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_RUNTIME_FEATURE_H_
#define BROWSER_VIVALDI_RUNTIME_FEATURE_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace vivaldi_runtime_feature {

struct Feature {
  Feature();
  ~Feature();
  Feature(const Feature&);
  Feature& operator=(const Feature&);

  std::string friendly_name;
  std::string description;

  // True if the feature is not applicable to current OS.
  bool inactive = false;

  // true if set by cmd line or forced for internal builds
  absl::optional<bool> forced;
};

using FeatureMap = base::flat_map<std::string, Feature>;
using EnabledSet = base::flat_set<std::string>;

CONTENT_EXPORT void Init();

CONTENT_EXPORT const FeatureMap& GetAllFeatures();

CONTENT_EXPORT const EnabledSet* GetEnabled(
    content::BrowserContext* browser_context);

// Call to check if a named feature is enabled.
CONTENT_EXPORT bool IsEnabled(content::BrowserContext* browser_context,
                              base::StringPiece feature_name);

CONTENT_EXPORT bool Enable(content::BrowserContext* browser_context,
                           base::StringPiece feature_name,
                           bool enabled);

}  // namespace vivaldi_runtime_feature

#endif  // BROWSER_VIVALDI_RUNTIME_FEATURE_H_
