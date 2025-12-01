// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef CHROME_UTILITY_IMPORTER_IMPORT_RESULT_H_
#define CHROME_UTILITY_IMPORTER_IMPORT_RESULT_H_

#include <string>
#include <utility>

#include "base/types/expected.h"
#include "chrome/common/importer/importer_bridge.h"
#include "ui/base/l10n/l10n_util.h"

// Passed by value any result from a call inside import or errorcode if unsuccessful
template<typename T> using ImportValue = base::expected<T, int>;

// Opaque result with no carried value.
using ImportResult = ImportValue<void>;

namespace import_result {

inline ImportResult Success() { return base::ok(); }
inline ImportResult Error(int message_id) {
  return base::unexpected{message_id};
}

template<typename T>
inline ImportValue<T> Success(const T&& v) { return base::ok(v); }
template<typename T>
inline ImportValue<T> Error(int message_id) {
  return base::unexpected{message_id};
}

// Helper to automatically notify bridge based on result
inline void NotifyBridge(const ImportResult& r,
                         ImporterBridge* bridge,
                         user_data_importer::ImportItem item) {
  if (r.has_value()) {
    bridge->NotifyItemEnded(item);
  } else {
    bridge->NotifyItemFailed(item, l10n_util::GetStringUTF8(r.error()));
  }
}

} // namespace import_result

#endif  // CHROME_UTILITY_IMPORTER_IMPORT_RESULT_H_
