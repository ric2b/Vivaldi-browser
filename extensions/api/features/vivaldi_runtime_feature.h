// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_FEATURES_VIVALDI_RUNTIME_FEATURE_H_
#define EXTENSIONS_API_FEATURES_VIVALDI_RUNTIME_FEATURE_H_

#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"

namespace content {
class BrowserContext;
}

namespace vivaldi_runtime_feature {

struct Entry {
  std::string friendly_name;
  std::string description;
  // in-memory active value
  bool enabled = false;
  // Default value for internal/sopranos builds.
  bool internal_default_enabled = false;
  // true if set by cmd line, value is fixed.
  bool force_value = false;
};

using EntryMap = base::flat_map<std::string, Entry>;

void Init();

const EntryMap* GetAll(content::BrowserContext* browser_context);

// Call to check if a named feature is enabled.
bool IsEnabled(content::BrowserContext* browser_context,
               base::StringPiece feature_name);

bool Enable(content::BrowserContext* browser_context,
            base::StringPiece feature_name,
            bool enabled);

}  // namespace vivaldi_runtime_feature

#endif  // EXTENSIONS_API_FEATURES_VIVALDI_RUNTIME_FEATURE_H_
