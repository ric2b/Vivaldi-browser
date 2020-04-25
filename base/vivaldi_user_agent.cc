// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "base/vivaldi_user_agent.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "components/version_info/version_info_values.h"

namespace vivaldi_user_agent {

const char kVivaldiSuffix[] = " Vivaldi/" VIVALDI_UA_VERSION;

const GURL* g_ui_thread_gurl = nullptr;

namespace {

const base::StringPiece g_vivaldi_whitelisted_domains[] = {
#include "base/vivaldi_user_agent_white_list.inc"
};

// TODO: deduce this automatically at compile time.
constexpr size_t kMaxWhitelistedDomainLength = 20;

// We need to consider only the host suffix of this length. Extra byte accounts
// for the dot after the longest domain.
constexpr size_t kMaxHostSuffixToCheck = kMaxWhitelistedDomainLength + 1;

bool g_user_agent_switch_checked = false;
bool g_user_agent_switch_present = false;

}  // namespace

bool IsWhiteListedHost(base::StringPiece host) {
  if (host.length() == 0)
    return false;

  // Use the simplest linear scan as the list of white-listed domains is short.
  // If the list becomes longer, a better algorithm should be implemented.
  static_assert(
      30 > sizeof g_vivaldi_whitelisted_domains /
               sizeof g_vivaldi_whitelisted_domains[0],
      "the list of domains should be short for linear scan to be practical");

  bool match = false;
  for (base::StringPiece domain : g_vivaldi_whitelisted_domains) {
    DCHECK(domain.length() <= kMaxWhitelistedDomainLength);
    if (host.length() == domain.length()) {
      match = base::EqualsCaseInsensitiveASCII(host, domain);
      if (match)
        break;
    } else if (host.length() >= domain.length() + 1) {
      // Check if the tail matches .domain
      size_t dot_pos = host.length() - domain.length() - 1;
      if (host[dot_pos] == '.') {
        base::StringPiece tail = host.substr(dot_pos + 1);
        match = base::EqualsCaseInsensitiveASCII(tail, domain);
        if (match)
          break;
      }
    }
  }
  if (!match)
    return false;

  // If we have --user-agent switch, always respect it as if the white-list list
  // was cleared.
  if (!g_user_agent_switch_checked) {
    g_user_agent_switch_checked = true;
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Cannot use switches::kUserAgent from Chromium as that is in a wrong
    // library.
    if (command_line->HasSwitch("user-agent")) {
      g_user_agent_switch_present = true;
    }
  }
  if (g_user_agent_switch_present)
    return false;

  return true;
}

}  // namespace vivaldi_user_agent
