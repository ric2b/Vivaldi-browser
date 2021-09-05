// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_UTILS_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_UTILS_H_

#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "components/request_filter/adblock_filter/adblock_metadata.h"

namespace flatbuffers {
struct String;
}

namespace url {
class Origin;
}

class GURL;

namespace adblock_filter {

namespace flat {
struct FilterRule;
}


std::string GetIndexVersionHeader();
std::string GetRulesListVersionHeader();

base::FilePath::StringType GetRulesFolderName();
base::FilePath::StringType GetGroupFolderName(RuleGroup group);

std::string CalculateBufferChecksum(base::span<const uint8_t> data);

int CompareDomains(base::StringPiece lhs_domain, base::StringPiece rhs_domain);
base::StringPiece ToStringPiece(const flatbuffers::String* string);
int GetRulePriority(const flat::FilterRule& rule);
int GetMaxRulePriority();
bool IsFullCSPAllowRule(const flat::FilterRule& rule);
bool IsThirdParty(const GURL& url, const url::Origin& origin);
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_UTILS_H_
