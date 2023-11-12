// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "base/vivaldi_user_agent.h"

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "vivaldi/base/base/edge_version.h"
#include "base/strings/string_util.h"
#include "components/browser/vivaldi_brand_select.h"
#include "components/version_info/version_info_values.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace vivaldi_user_agent {

const char kVivaldiSuffix[] = " Vivaldi/" VIVALDI_UA_VERSION;
const char kVivaldiSuffixReduced[] = " Vivaldi/" VIVALDI_UA_VERSION_REDUCED;

const GURL* g_ui_thread_gurl = nullptr;

namespace {

const base::StringPiece g_vivaldi_whitelisted_domains[] = {
#include "base/vivaldi_user_agent_white_list.inc"
};

const base::StringPiece g_vivaldi_edge_domains[] = {
  "bing.com"
};

const char kEdgeSuffix[] = " Edg/" EDGE_FULL_VERSION;
const char kEdgeSuffixReduced[] = " Edg/" CHROME_PRODUCT_VERSION_REDUCED;

// TODO: deduce this automatically at compile time.
constexpr size_t kMaxWhitelistedDomainLength = 20;

bool g_user_agent_switch_checked = false;
bool g_user_agent_switch_present = false;

bool MatchDomain(base::StringPiece host, base::StringPiece domain) {
  DCHECK(domain.length() <= kMaxWhitelistedDomainLength);
  if (host.length() == domain.length()) {
    if (base::EqualsCaseInsensitiveASCII(host, domain))
      return true;
  }
  else if (host.length() >= domain.length() + 1) {
    // Check if the tail matches .domain
    size_t dot_pos = host.length() - domain.length() - 1;
    if (host[dot_pos] == '.') {
      base::StringPiece tail = host.substr(dot_pos + 1);
      if(base::EqualsCaseInsensitiveASCII(tail, domain))
        return true;
    }
  }
  return false;
}

bool HasUserAgentSwitch() {
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
  return g_user_agent_switch_present;
}

}  // namespace

bool IsWhiteListedHost(base::StringPiece host) {
  if (host.length() == 0)
    return false;

  // If we have --user-agent switch, always respect it as if the white-list list
  // was cleared.
  if (HasUserAgentSwitch())
    return false;

  // Use the simplest linear scan as the list of white-listed domains is short.
  // If the list becomes longer, a better algorithm should be implemented.
  static_assert(
      30 > sizeof g_vivaldi_whitelisted_domains /
               sizeof g_vivaldi_whitelisted_domains[0],
      "the list of domains should be short for linear scan to be practical");

  bool match = false;
  for (base::StringPiece domain : g_vivaldi_whitelisted_domains) {
    match = MatchDomain(host, domain);
    if (match)
        break;
  }
  if (!match)
    return false;

  return true;
}

bool IsBingHost(base::StringPiece host) {
  if (host.length() == 0)
    return false;

  // If we have --user-agent switch, always respect it as if the white-list list
  // was cleared.
  if (HasUserAgentSwitch())
    return false;

  // Use the simplest linear scan as the list of white-listed domains is short.
  // If the list becomes longer, a better algorithm should be implemented.
  static_assert(
    30 > sizeof g_vivaldi_edge_domains /
    sizeof g_vivaldi_edge_domains[0],
    "the list of Bing domains should be short for linear scan to be practical");

  bool match = false;
  for (base::StringPiece domain : g_vivaldi_edge_domains) {
    match = MatchDomain(host, domain);
    if (match)
      break;
  }
  if (!match)
    return false;

  return true;
}

void UpdateAgentString(bool reduced, std::string& user_agent) {
  if (!vivaldi::IsVivaldiRunning())
    return;

  if (!g_ui_thread_gurl)
    return;

  if (IsBingHost(g_ui_thread_gurl->host_piece())) {
    user_agent += (reduced? kEdgeSuffixReduced : kEdgeSuffix);
  }

  if (!IsWhiteListedHost(g_ui_thread_gurl->host_piece()))
    return;

  user_agent += (reduced ? kVivaldiSuffixReduced :kVivaldiSuffix);
}

std::vector<std::string> GetVivaldiWhitelist() {
  std::vector<std::string> domain_whitelist;
  for (base::StringPiece domain : g_vivaldi_whitelisted_domains) {
    domain_whitelist.emplace_back(std::string(domain));
  }
  return domain_whitelist;
}

std::vector<std::string> GetVivaldiEdgeList() {
  std::vector<std::string> edge_domain_list;
  for (base::StringPiece domain : g_vivaldi_edge_domains) {
    edge_domain_list.emplace_back(std::string(domain));
  }
  return edge_domain_list;
}

}  // namespace vivaldi_user_agent
