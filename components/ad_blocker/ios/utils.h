// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_IOS_UTILS_H_
#define COMPONENTS_AD_BLOCKER_IOS_UTILS_H_

#include <string>

#include "base/values.h"

namespace adblock_filter {

namespace rules_json {
inline constexpr char kVersion[] = "version";
inline constexpr char kPartnerListAllowedDocuments[] =
    "partner-list-allowed-documents";
inline constexpr char kNetworkRules[] = "network";
inline constexpr char kCosmeticRules[] = "cosmetic";
inline constexpr char kScriptletRules[] = "scriptlet";
inline constexpr char kBlockRules[] = "block";
inline constexpr char kAllowRules[] = "allow";
inline constexpr char kBlockImportantRules[] = "block-important";
inline constexpr char kGeneric[] = "generic";
inline constexpr char kSpecific[] = "specific";
inline constexpr char kGenericAllowRules[] = "generic-allow";
inline constexpr char kBlockAllowPairs[] = "block-allow-pairs";
// These names are used in a dict otherwise containing domain name fragments
// Prefixing them with a dot ensures they won't collide with an actual fragemnt.
inline constexpr char kIncluded[] = ".included";
inline constexpr char kExcluded[] = ".excluded";

inline constexpr char kTrigger[] = "trigger";
inline constexpr char kUrlFilter[] = "url-filter";
inline constexpr char kUrlFilterIsCaseSensitive[] =
    "url-filter-is-case-sensitive";
inline constexpr char kResourceType[] = "resource-type";
inline constexpr char kLoadType[] = "load-type";
inline constexpr char kFirstParty[] = "first-party";
inline constexpr char kThirdParty[] = "third-party";
inline constexpr char kLoadContext[] = "load-context";
inline constexpr char kTopFrame[] = "top-frame";
inline constexpr char kChildFrame[] = "child-frame";
inline constexpr char kIfTopUrl[] = "if-top-url";
inline constexpr char kUnlessTopUrl[] = "unless-top-url";
inline constexpr char kTopUrlFilterIsCaseSensitive[] =
    "top-url-filter-is-case-sensitive";

inline constexpr char kAction[] = "action";
inline constexpr char kType[] = "type";
inline constexpr char kBlock[] = "block";
inline constexpr char kIgnorePrevious[] = "ignore-previous-rules";
inline constexpr char kCssHide[] = "css-display-none";
inline constexpr char kRedirect[] = "redirect";
inline constexpr char kModifyHeaders[] = "modify-headers";
inline constexpr char kSelector[] = "selector";
inline constexpr char kUrl[] = "url";
inline constexpr char kPriority[] = "priority";
inline constexpr char kResponseHeaders[] = "response-headers";
inline constexpr char kOperation[] = "operation";
inline constexpr char kAppend[] = "append";
inline constexpr char kHeader[] = "header";
inline constexpr char kCsp[] = "Content-Security-Policy";
inline constexpr char kValue[] = "value";

inline constexpr char kNonIosRulesAndMetadata[] = "non-ios-rules-and-metadata";
inline constexpr char kMetadata[] = "metadata";
inline constexpr char kListChecksums[] = "list-checksums";
inline constexpr char kExceptionRule[] = "exception-rule";
inline constexpr char kIosContentBlockerRules[] = "ios-content-blocker-rules";
}  // namespace rules_json

int GetIntermediateRepresentationVersionNumber();
int GetOrganizedRulesVersionNumber();

std::string CalculateBufferChecksum(const std::string& data);

struct ContentInjectionArgumentsCompare {
  bool operator()(const base::Value::List* lhs,
                  const base::Value::List* rhs) const;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_IOS_UTILS_H_
