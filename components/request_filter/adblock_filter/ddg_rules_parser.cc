// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/ddg_rules_parser.h"

#include "components/request_filter/adblock_filter/parse_result.h"
#include "components/request_filter/adblock_filter/parse_utils.h"

namespace {
const char kTrackersKey[] = "trackers";
const char kEntitiesKey[] = "entities";
const char kOwnerKey[] = "owner";
const char kCategoriesKey[] = "categories";
const char kOwnerNamePath[] = "owner.name";
const char kDefaultActionKey[] = "default";
const char kActionBlock[] = "block";
const char kActionIgnore[] = "ignore";
const char kRulesKey[] = "rules";
const char kRuleKey[] = "rule";
const char kActionKey[] = "action";
const char kOptionssKey[] = "options";
const char kExceptionsKey[] = "exceptions";
const char kTypesKey[] = "types";
const char kDomainsKey[] = "domains";

const char kListTitle[] = "DuckDuckGo blocking list";
const int kValidityHours = 12;

std::string MaybeConvertRegexToPlainPattern(const std::string& regex) {
  bool escaped = false;
  std::string result;
  for (auto c : regex) {
    switch (c) {
      case ')':
      case ']':
      case '}':
        if (!escaped) {
          result.push_back(c);
          break;
        }
        FALLTHROUGH;
      case '/':
      case '|':
      case '^':
      case '$':
      case '(':
      case '[':
      case '.':
      case '{':
      case '*':
      case '?':
      case '+':
        if (escaped) {
          result.push_back(c);
          escaped = false;
        } else {
          return "";
        }
        break;
      case '\\':
        escaped = true;
        break;
      default:
        if (escaped)
          return "";
        result.push_back(c);
        break;
    }
  }
  return result;
}
}  // namespace

namespace adblock_filter {

DuckDuckGoRulesParser::DuckDuckGoRulesParser(ParseResult* parse_result)
    : parse_result_(parse_result) {}
DuckDuckGoRulesParser::~DuckDuckGoRulesParser() = default;

void DuckDuckGoRulesParser::Parse(const base::Value& root) {
  if (!root.is_dict()) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
    return;
  }

  const base::Value* trackers = root.FindDictKey(kTrackersKey);
  if (!trackers) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
    return;
  }

  const base::Value* entities = root.FindDictKey(kEntitiesKey);
  if (!entities) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
    return;
  }

  parse_result_->tracker_infos = base::Value(base::Value::Type::DICTIONARY);

  for (const auto& item : trackers->DictItems()) {
    const std::string& domain = item.first;
    if (!item.second.is_dict()) {
      parse_result_->rules_info.invalid_rules++;
      continue;
    }

    const std::string* default_action =
        item.second.FindStringKey(kDefaultActionKey);
    if (!default_action || (default_action->compare(kActionBlock) != 0 &&
                            default_action->compare(kActionIgnore) != 0)) {
      parse_result_->rules_info.invalid_rules++;
      continue;
    }

    const base::Value* excluded_origins = nullptr;
    const std::string* owner = item.second.FindStringPath(kOwnerNamePath);
    if (owner) {
      const base::Value* entity = entities->FindDictKey(*owner);
      if (entity)
        excluded_origins = entity->FindListKey(kDomainsKey);
    }

    base::DictionaryValue tracker_info;
    const base::Value* owner_dict = item.second.FindKey(kOwnerKey);
    if (owner_dict)
      tracker_info.SetKey(kOwnerKey, owner_dict->Clone());
    const base::Value* categories = item.second.FindKey(kCategoriesKey);
    if (categories)
      tracker_info.SetKey(kCategoriesKey, categories->Clone());

    if (owner || categories)
      parse_result_->tracker_infos.SetKey(domain, std::move(tracker_info));

    bool default_ignore = false;
    if (default_action->compare(kActionIgnore) == 0) {
      default_ignore = true;
    } else {
      AddBlockingRuleForDomain(domain, excluded_origins);
    }

    const base::Value* rules = item.second.FindListKey(kRulesKey);
    if (!rules)
      continue;

    for (const auto& rule : rules->GetList()) {
      ParseRule(rule, domain, default_ignore, excluded_origins);
    }
  }

  if (parse_result_->filter_rules.empty()) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
  } else {
    parse_result_->fetch_result = FetchResult::kSuccess;
    parse_result_->metadata.title = kListTitle;
    parse_result_->metadata.expires =
        base::TimeDelta::FromHours(kValidityHours);
  }
}

void DuckDuckGoRulesParser::AddBlockingRuleForDomain(
    const std::string& domain,
    const base::Value* excluded_origins) {
  FilterRule rule;
  rule.resource_types.set();
  rule.party.set();
  rule.anchor_type.set(FilterRule::kAnchorHost);
  rule.host = domain;
  rule.pattern = domain;

  if (excluded_origins) {
    DCHECK(excluded_origins->is_list());
    for (const auto& origin : excluded_origins->GetList()) {
      if (origin.is_string())
        rule.excluded_domains.push_back(origin.GetString());
    }
  }

  parse_result_->filter_rules.push_back(std::move(rule));
  parse_result_->rules_info.valid_rules++;
}

void DuckDuckGoRulesParser::ParseRule(const base::Value& rule,
                                      const std::string& domain,
                                      bool default_ignore,
                                      const base::Value* excluded_origins) {
  if (!rule.is_dict())
    return;

  const std::string* pattern = rule.FindStringKey(kRuleKey);
  if (!pattern) {
    parse_result_->rules_info.invalid_rules++;
    return;
  }

  bool ignore = false;
  const std::string* action = rule.FindStringKey(kActionKey);
  if (action) {
    if (action->compare(kActionIgnore) == 0) {
      ignore = true;
    } else if (action->compare(kActionBlock) != 0) {
      parse_result_->rules_info.invalid_rules++;
      return;
    }
  }

  const base::Value* exceptions = rule.FindDictKey(kExceptionsKey);
  const base::Value* options = rule.FindDictKey(kOptionssKey);

  if ((!exceptions || options) && ignore == default_ignore) {
    // If the rule has the same action as the default action and
    // doesn't have any exception, it is redundant. If it has exceptions
    // and options, it's unclear what the intent would be.
    parse_result_->rules_info.unsupported_rules++;
    return;
  }

  std::bitset<FilterRule::kTypeCount> exception_types;
  std::vector<std::string> exception_domains;
  std::bitset<FilterRule::kTypeCount> option_types;
  std::vector<std::string> option_domains;
  if (exceptions) {
    exception_types = GetTypes(exceptions);
    exception_domains = GetDomains(exceptions);
  }

  if (options) {
    option_types = GetTypes(options);
    option_domains = GetDomains(options);
  }

  FilterRule filter_rule;
  filter_rule.party.set();
  if (!default_ignore)
    filter_rule.is_allow_rule = true;
  if (default_ignore == ignore) {
    filter_rule.included_domains.swap(exception_domains);
    filter_rule.resource_types = exception_types;
    if (filter_rule.resource_types.none())
      filter_rule.resource_types.set();
  } else {
    if (option_types.none())
      option_types.set();

    filter_rule.included_domains.swap(option_domains);
    filter_rule.excluded_domains.swap(exception_domains);
    // Exceptions have priority over options.
    filter_rule.resource_types = option_types & ~exception_types;
    if (filter_rule.resource_types.none()) {
      parse_result_->rules_info.unsupported_rules++;
      return;
    }
  }

  std::string plain_pattern = MaybeConvertRegexToPlainPattern(*pattern);

  if (plain_pattern.empty()) {
    filter_rule.pattern_type = FilterRule::kRegex;
    filter_rule.pattern = *pattern;
    filter_rule.ngram_search_string = BuildNgramSearchString(*pattern);
  } else {
    filter_rule.pattern = plain_pattern;
  }
  filter_rule.host = domain;

  if (excluded_origins) {
    DCHECK(excluded_origins->is_list());
    for (const auto& origin : excluded_origins->GetList()) {
      if (origin.is_string())
        filter_rule.excluded_domains.push_back(origin.GetString());
    }
  }

  parse_result_->filter_rules.push_back(std::move(filter_rule));
  parse_result_->rules_info.valid_rules++;
}

std::bitset<FilterRule::kTypeCount> DuckDuckGoRulesParser::GetTypes(
    const base::Value* rule_properties) {
  std::bitset<FilterRule::kTypeCount> types;
  const base::Value* types_value = rule_properties->FindListKey(kTypesKey);
  if (!types_value)
    return types;

  for (const auto& type_name : types_value->GetList()) {
    if (!type_name.is_string())
      continue;

    const auto type = kTypeStringMap.find(type_name.GetString());
    if (type == kTypeStringMap.end())
      continue;
    else
      types.set(type->second);
  }

  return types;
}

std::vector<std::string> DuckDuckGoRulesParser::GetDomains(
    const base::Value* rule_properties) {
  std::vector<std::string> domains;
  const base::Value* domains_value = rule_properties->FindListKey(kDomainsKey);
  if (!domains_value)
    return domains;

  for (const auto& domain : domains_value->GetList()) {
    if (!domain.is_string())
      continue;

    domains.push_back(domain.GetString());
  }

  return domains;
}

}  // namespace adblock_filter
