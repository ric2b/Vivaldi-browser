// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "components/history/core/browser/url_database.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "db/vivaldi_history_types.h"
#include "base/strings/string_split.h"

namespace history {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
bool URLDatabase::CreateVivaldiURLsLastVisitIndex() {
  return GetDB().Execute("DROP INDEX IF EXISTS urls_idx_last_visit_time;");
}

bool URLDatabase::CreateVivaldiURLScoreIndex() {
  return GetDB().Execute("DROP INDEX IF EXISTS urls_score_index;");
}

#endif

}  // namespace history
