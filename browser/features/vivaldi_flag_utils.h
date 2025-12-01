// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_FLAGS_VIVALDI_FLAG_UTILS_H_
#define BROWSER_FLAGS_VIVALDI_FLAG_UTILS_H_

namespace flags_ui {
class FlagsStorage;
struct FeatureEntry;
}

namespace vivaldi {

bool VivaldiShouldSkipConditionalFeatureEntry(
    const flags_ui::FlagsStorage* storage,
    const flags_ui::FeatureEntry& entry);
}

#endif  // BROWSER_FLAGS_VIVALDI_FLAG_UTILS_H_
