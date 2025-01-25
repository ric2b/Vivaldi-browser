// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_VIVALDI_RUNTIME_FEATURE_H_
#define BROWSER_VIVALDI_RUNTIME_FEATURE_H_

#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "content/common/content_export.h"

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

  bool default_value = false;

  // True when the value cannot be altered by the user and the value from
  // preference is ignored.
  bool locked = false;

  // True if the feature is not applicable to the current OS.
  bool inactive = false;
};

using FeatureMap = base::flat_map<std::string, Feature>;
using EnabledSet = base::flat_set<std::string>;

CONTENT_EXPORT void Init();

CONTENT_EXPORT const FeatureMap& GetAllFeatures();

CONTENT_EXPORT const EnabledSet* GetEnabled(
    content::BrowserContext* browser_context);

// Call to check if a named feature is enabled.
CONTENT_EXPORT bool IsEnabled(content::BrowserContext* browser_context,
                              std::string_view feature_name);

CONTENT_EXPORT bool Enable(content::BrowserContext* browser_context,
                           std::string_view feature_name,
                           bool enabled);

}  // namespace vivaldi_runtime_feature

#endif  // BROWSER_VIVALDI_RUNTIME_FEATURE_H_
