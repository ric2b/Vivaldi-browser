// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "browser/features/vivaldi_flag_utils.h"

#include "browser/features/vivaldi_flag_descriptions.h"
#include "components/webui/flags/feature_entry.h"
#include "components/webui/flags/flags_storage.h"

#include "app/vivaldi_version_info.h"

namespace vivaldi {

bool VivaldiShouldSkipConditionalFeatureEntry(
    const flags_ui::FlagsStorage* storage,
    const flags_ui::FeatureEntry& entry) {
  return false;
}

}  // namespace vivaldi
