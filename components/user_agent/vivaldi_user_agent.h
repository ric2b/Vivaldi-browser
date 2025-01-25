// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_USER_AGENT_VIVALDI_USER_AGENT_H_
#define COMPONENTS_USER_AGENT_VIVALDI_USER_AGENT_H_

#include <string>
#include <string_view>
#include <vector>

class GURL;

namespace vivaldi_user_agent {

extern const char kVivaldiSuffix[];

// Couple of globals to pass GURL and KURL arguments through call chain without
// patching multiple Chromium call sites.
extern const GURL* g_ui_thread_gurl;

bool IsUrlAllowed(const GURL& url);

// Update the user agent string based on the current `g_ui_thread_gurl`
void UpdateAgentString(bool reduced, std::string& user_agent);

std::vector<std::string> GetVivaldiAllowlist();
std::vector<std::string> GetVivaldiEdgeList();

}  // namespace vivaldi_user_agent

#endif  // COMPONENTS_USER_AGENT_VIVALDI_USER_AGENT_H_
