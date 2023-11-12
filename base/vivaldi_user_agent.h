// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef BASE_VIVALDI_USER_AGENT_H_
#define BASE_VIVALDI_USER_AGENT_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/strings/string_piece.h"

class GURL;

namespace vivaldi_user_agent {

extern BASE_EXPORT const char kVivaldiSuffix[];

// Couple of globals to pass GURL and KURL arguments through call chain without
// patching multiple Chromium call sites.
extern BASE_EXPORT const GURL* g_ui_thread_gurl;

BASE_EXPORT bool IsWhiteListedHost(base::StringPiece host);

// Update the user agent string based on the current `g_ui_thread_gurl`
BASE_EXPORT void UpdateAgentString(bool reduced, std::string& user_agent);

BASE_EXPORT std::vector<std::string> GetVivaldiWhitelist();
BASE_EXPORT std::vector<std::string> GetVivaldiEdgeList();

}  // namespace vivaldi_user_agent

#endif  // BASE_VIVALDI_USER_AGENT_H_
