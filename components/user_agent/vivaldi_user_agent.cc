// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "components/user_agent/vivaldi_user_agent.h"

#include "app/vivaldi_apptools.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_util.h"
#include "components/browser/vivaldi_brand_select.h"
#include "components/google/core/common/google_util.h"
#include "components/version_info/version_info_values.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"
#include "vivaldi/base/base/edge_version.h"

namespace vivaldi_user_agent {

#if !BUILDFLAG(IS_IOS)
const char kVivaldiSuffix[] = " Vivaldi/" VIVALDI_UA_VERSION;
const char kVivaldiSuffixReduced[] = " Vivaldi/" VIVALDI_UA_VERSION_REDUCED;
#else
// Note:(prio@vivaldi.com) Use VivaiOS for iOS.
// Ref:(https://bugs.vivaldi.com/browse/VIB-659)
const char kVivaldiSuffix[] = " VivaiOS/" VIVALDI_UA_VERSION;
const char kVivaldiSuffixReduced[] = " VivaiOS/" VIVALDI_UA_VERSION_REDUCED;
#endif

const GURL* g_ui_thread_gurl = nullptr;

namespace {

constexpr auto kVivaldiAllowedDomains =
    base::MakeFixedFlatSet<std::string_view>({
#include "components/user_agent/vivaldi_user_agent_allow_list.inc"
    });

constexpr auto kVivaldiEdgeDomains =
    base::MakeFixedFlatSet<std::string_view>({"bing.com"});

const char kEdgeSuffix[] = " Edg/" EDGE_FULL_VERSION;
const char kEdgeSuffixReduced[] = " Edg/" CHROME_PRODUCT_VERSION_REDUCED;

bool g_user_agent_switch_checked = false;
bool g_user_agent_switch_present = false;

#if BUILDFLAG(IS_ANDROID)
constexpr bool g_google_is_vivaldi_partner = false;
#else
constexpr bool g_google_is_vivaldi_partner = false;
#endif

template <typename Container>
bool MatchHost(std::string_view host, const Container& container) {
  const auto get_parent_host =
      [](const std::string_view host) -> std::string_view {
    const size_t dot_pos = host.find('.');
    return (dot_pos != std::string::npos && dot_pos + 1 < host.length())
               ? host.substr(dot_pos + 1)
               : "";
  };

  do {
    if (container.contains(host)) {
      return true;
    }
    host = get_parent_host(host);
  } while (!host.empty());
  return false;
}

bool HasUserAgentSwitch() {
  // If we have --user-agent switch, always respect it as if the allow-list
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

bool IsGooglePartnerUrl(const GURL& url) {
  using namespace google_util;

  const auto is_google_disallowed_path = [](const auto& path) {
    // Try to keep the list short
    static const char* kGoogleDisallowedPaths[] = {
        "/travel",  // VB-108684
    };
    if (!path.empty()) {
      for (const auto& disallowed_path : kGoogleDisallowedPaths) {
        if (base::StartsWith(path, disallowed_path,
                             base::CompareCase::INSENSITIVE_ASCII)) {
          return true;
        }
      }
    }
    return false;
  };

  // Allow "[www.]google.<TLD>" domains if Google is our partner.
  // Disallow subdomains to not acidentally break e.g. Google Docs with our UA.
  // Disallow specific paths that are known to break.
  return g_google_is_vivaldi_partner &&
         IsGoogleDomainUrl(url, SubdomainPermission::DISALLOW_SUBDOMAIN,
                           PortPermission::DISALLOW_NON_STANDARD_PORTS) &&
         !is_google_disallowed_path(url.path_piece());
}

}  // namespace

bool IsUrlAllowed(const GURL& url) {
  if (!url.is_valid() || url.is_empty())
    return false;

  // If we have --user-agent switch, always respect it as if the allow-list
  // was cleared.
  if (HasUserAgentSwitch())
    return false;

  if (IsGooglePartnerUrl(url)) {
    return true;
  }

  return MatchHost(url.host_piece(), kVivaldiAllowedDomains);
}

bool IsBingHost(std::string_view host) {
  if (host.length() == 0)
    return false;

  // If we have --user-agent switch, always respect it as if the allow-list
  // was cleared.
  if (HasUserAgentSwitch())
    return false;

  return MatchHost(host, kVivaldiEdgeDomains);
}

void UpdateAgentString(bool reduced, std::string& user_agent) {
  if (!vivaldi::IsVivaldiRunning())
    return;

  if (!g_ui_thread_gurl)
    return;

  if (IsBingHost(g_ui_thread_gurl->host_piece())) {
    user_agent += (reduced? kEdgeSuffixReduced : kEdgeSuffix);
  }

  if (!IsUrlAllowed(*g_ui_thread_gurl))
    return;

  user_agent += (reduced ? kVivaldiSuffixReduced :kVivaldiSuffix);
}

std::vector<std::string> GetVivaldiAllowlist() {
  std::vector<std::string> domain_allowlist;
  for (std::string_view domain : kVivaldiAllowedDomains) {
    domain_allowlist.emplace_back(std::string(domain));
  }

  return domain_allowlist;
}

std::vector<std::string> GetVivaldiEdgeList() {
  std::vector<std::string> edge_domain_list;
  for (std::string_view domain : kVivaldiEdgeDomains) {
    edge_domain_list.emplace_back(std::string(domain));
  }

  return edge_domain_list;
}

}  // namespace vivaldi_user_agent
