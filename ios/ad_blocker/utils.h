// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_AD_BLOCKER_UTILS_H_
#define IOS_AD_BLOCKER_UTILS_H_

#include <string>

#include "base/values.h"

namespace adblock_filter {

namespace rules_json {
constexpr char kVersion[] = "version";
constexpr char kNetworkRules[] = "network";
constexpr char kCosmeticRules[] = "cosmetic";
constexpr char kScriptletRules[] = "scriptlet";
constexpr char kBlockRules[] = "block";
constexpr char kAllowRules[] = "allow";
constexpr char kBlockImportantRules[] = "block-important";
constexpr char kGeneric[] = "generic";
constexpr char kSpecific[] = "specific";
constexpr char kGenericAllowRules[] = "generic-allow";
constexpr char kBlockAllowPairs[] = "block-allow-pairs";
// These names are used in a dict otherwise containing domain name fragments
// Prefixing them with a dot ensures they won't collide with an actual fragemnt.
constexpr char kIncluded[] = ".included";
constexpr char kExcluded[] = ".excluded";

constexpr char kTrigger[] = "trigger";
constexpr char kUrlFilter[] = "url-filter";
constexpr char kUrlFilterIsCaseSensitive[] = "url-filter-is-case-sensitive";
constexpr char kResourceType[] = "resource-type";
constexpr char kLoadType[] = "load-type";
constexpr char kFirstParty[] = "first-party";
constexpr char kThirdParty[] = "third-party";
constexpr char kLoadContext[] = "load-context";
constexpr char kTopFrame[] = "top-frame";
constexpr char kChildFrame[] = "child-frame";
constexpr char kIfTopUrl[] = "if-top-url";
constexpr char kUnlessTopUrl[] = "unless-top-url";
constexpr char kTopUrlFilterIsCaseSensitive[] =
    "top-url-filter-is-case-sensitive";

constexpr char kAction[] = "action";
constexpr char kType[] = "type";
constexpr char kBlock[] = "block";
constexpr char kIgnorePrevious[] = "ignore-previous-rules";
constexpr char kCssHide[] = "css-display-none";
constexpr char kRedirect[] = "redirect";
constexpr char kModifyHeaders[] = "modify-headers";
constexpr char kSelector[] = "selector";
constexpr char kUrl[] = "url";
constexpr char kPriority[] = "priority";
constexpr char kResponseHeaders[] = "response-headers";
constexpr char kOperation[] = "operation";
constexpr char kAppend[] = "append";
constexpr char kHeader[] = "header";
constexpr char kCsp[] = "Content-Security-Policy";
constexpr char kValue[] = "value";

constexpr char kNonIosRulesAndMetadata [] ="non-ios-rules-and-metadata";
constexpr char kMetadata [] ="metadata";
constexpr char kListChecksums [] ="list-checksums";
constexpr char kExceptionRule [] ="exception-rule";
constexpr char kIosContentBlockerRules[] = "ios-content-blocker-rules";
}

int GetIntermediateRepresentationVersionNumber();
int GetOrganizedRulesVersionNumber();

std::string CalculateBufferChecksum(const std::string& data);

struct ContentInjectionArgumentsCompare {
  bool operator()(const base::Value::List* lhs,
                  const base::Value::List* rhs) const;
};
}  // namespace adblock_filter

#endif  // IOS_AD_BLOCKER_UTILS_H_
