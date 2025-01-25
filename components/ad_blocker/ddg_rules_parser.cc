// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/ddg_rules_parser.h"

#include "base/strings/string_util.h"
#include "components/ad_blocker/parse_result.h"
#include "components/ad_blocker/parse_utils.h"

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
const char kSurrogateKey[] = "surrogate";
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
        [[fallthrough]];
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

  const base::Value* trackers = root.GetDict().Find(kTrackersKey);
  if (!trackers) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
    return;
  }

  const base::Value* entities = root.GetDict().Find(kEntitiesKey);
  if (!entities) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
    return;
  }

  parse_result_->tracker_infos = base::Value::Dict();

  for (const auto item : trackers->GetDict()) {
    const std::string& domain = item.first;
    if (!item.second.is_dict()) {
      parse_result_->rules_info.invalid_rules++;
      continue;
    }

    const std::string* default_action =
        item.second.GetDict().FindString(kDefaultActionKey);
    if (!default_action || (default_action->compare(kActionBlock) != 0 &&
                            default_action->compare(kActionIgnore) != 0)) {
      parse_result_->rules_info.invalid_rules++;
      continue;
    }

    const base::Value::List* excluded_origins = nullptr;
    const std::string* owner =
        item.second.GetDict().FindStringByDottedPath(kOwnerNamePath);
    if (owner) {
      const base::Value* entity = entities->GetDict().Find(*owner);
      if (entity)
        excluded_origins = entity->GetDict().FindList(kDomainsKey);
    }

    base::Value::Dict tracker_info;
    const base::Value* owner_dict = item.second.GetDict().Find(kOwnerKey);
    if (owner_dict)
      tracker_info.Set(kOwnerKey, owner_dict->Clone());
    const base::Value* categories = item.second.GetDict().Find(kCategoriesKey);
    if (categories)
      tracker_info.Set(kCategoriesKey, categories->Clone());

    if (owner || categories)
      parse_result_->tracker_infos->Set(domain, std::move(tracker_info));

    bool default_ignore = false;
    if (default_action->compare(kActionIgnore) == 0) {
      default_ignore = true;
    } else {
      AddBlockingRuleForDomain(domain, excluded_origins);
    }

    const base::Value::List* rules = item.second.GetDict().FindList(kRulesKey);
    if (!rules)
      continue;

    for (const auto& rule : *rules) {
      ParseRule(rule, domain, default_ignore, excluded_origins);
    }
  }

  if (parse_result_->request_filter_rules.empty()) {
    parse_result_->fetch_result = FetchResult::kFileUnsupported;
  } else {
    parse_result_->fetch_result = FetchResult::kSuccess;
    parse_result_->metadata.title = kListTitle;
    parse_result_->metadata.expires = base::Hours(kValidityHours);
  }
}

void DuckDuckGoRulesParser::AddBlockingRuleForDomain(
    const std::string& domain,
    const base::Value::List* excluded_origins) {
  RequestFilterRule rule;
  rule.resource_types.set();
  rule.party.set();
  rule.anchor_type.set(RequestFilterRule::kAnchorHost);
  rule.host = domain;
  rule.pattern = domain;

  if (excluded_origins) {
    for (const auto& origin : *excluded_origins) {
      if (origin.is_string())
        rule.excluded_domains.insert(origin.GetString());
    }
  }

  parse_result_->request_filter_rules.push_back(std::move(rule));
  parse_result_->rules_info.valid_rules++;
}

void DuckDuckGoRulesParser::ParseRule(
    const base::Value& rule,
    const std::string& domain,
    bool default_ignore,
    const base::Value::List* excluded_origins) {
  if (!rule.is_dict())
    return;

  const std::string* pattern = rule.GetDict().FindString(kRuleKey);
  if (!pattern) {
    parse_result_->rules_info.invalid_rules++;
    return;
  }

  bool ignore = false;
  const std::string* action = rule.GetDict().FindString(kActionKey);
  if (action) {
    if (action->compare(kActionIgnore) == 0) {
      ignore = true;
    } else if (action->compare(kActionBlock) != 0) {
      parse_result_->rules_info.invalid_rules++;
      return;
    }
  }

  if (default_ignore && ignore) {
    // Ignore rules are always redundant for an ignored tracker under the
    // DDG extension implementation
    parse_result_->rules_info.unsupported_rules++;
    return;
  }

  const std::string* surrogate = rule.GetDict().FindString(kSurrogateKey);
  const base::Value* exceptions = rule.GetDict().Find(kExceptionsKey);
  const base::Value* options = rule.GetDict().Find(kOptionssKey);

  bool make_request_filter_rule = false;
  bool make_redirect_rule = false;
  if (!surrogate || ignore) {
    if (!default_ignore && !ignore && !exceptions) {
      DCHECK(!surrogate);
      // Block rules for trackers that block by default are redundant unless
      // they also come with exceptions
      parse_result_->rules_info.unsupported_rules++;
      return;
    }
    make_request_filter_rule = true;
  } else {  // surrogate && !ignore
    // All block rules with surrogates result in a redirect rule. In this case
    // the rule is not redundant if the tracker default is block. If the rule
    // has exceptions, those result in a separate pass rule.
    if (default_ignore == ignore && exceptions)
      make_request_filter_rule = true;
    make_redirect_rule = true;
  }

  std::optional<std::bitset<RequestFilterRule::kTypeCount>> exception_types;
  std::optional<std::set<std::string>> exception_domains;
  std::optional<std::bitset<RequestFilterRule::kTypeCount>> option_types;
  std::optional<std::set<std::string>> option_domains;
  if (exceptions) {
    exception_types = GetTypes(exceptions);
    exception_domains = GetDomains(exceptions);
    if (!exception_types && !exception_domains) {
      parse_result_->rules_info.invalid_rules++;
      return;
    }
  }

  if (options) {
    option_types = GetTypes(options);
    option_domains = GetDomains(options);
    if (!option_types && !option_domains) {
      parse_result_->rules_info.invalid_rules++;
      return;
    }
  }

  if ((exception_types && exception_types->none()) ||
      (exception_domains && exception_domains->empty()) ||
      (option_types && option_types->none()) ||
      (option_domains && option_domains->empty())) {
    // Exceptions / Options specifying types/domains should always provice
    // valid content for them
    parse_result_->rules_info.invalid_rules++;
    return;
  }

  if (!option_types) {
    option_types.emplace();
    option_types->set();
  }

  std::string plain_pattern = MaybeConvertRegexToPlainPattern(*pattern);
  std::string ngram_search_string;
  if (plain_pattern.empty())
    ngram_search_string = BuildNgramSearchString(*pattern);

  if (make_request_filter_rule) {
    RequestFilterRule filter_rule;
    filter_rule.party.set();
    if (!default_ignore)
      filter_rule.decision = RequestFilterRule::kPass;
    if (default_ignore == ignore) {
      DCHECK(ignore == false);
      DCHECK(exceptions);
      // Under the DDG implementation, if a block rule has options and
      // exceptions, the rule is matched if the options are matched and the
      // request is then ignored if the exceptions are matched in turn. So, to
      // implement this, we need an pass rule that matches both the options and
      // exceptions in the original rule.
      if (option_domains && exception_domains) {
        // Domain must have a match in both lists to be included.
        // Not the most efficient implementation O(n*m), but
        // 1. Rules that have this setup should be rare in practice
        //    (none currently). Cases with two big lists are even less likely.
        // 2. The parser can usually afford to be a bit slow, since it doesn't
        //    run on the UI thread and people aren't expected to load this list
        //    manually.
        for (const auto& option_domain : option_domains.value()) {
          std::string_view potential_domain;
          for (const auto& exception_domain : exception_domains.value()) {
            if (potential_domain.empty()) {
              if (option_domain == exception_domain) {
                potential_domain = option_domain;
                continue;
              } else if (option_domain.size() > exception_domain.size()) {
                size_t dot_position =
                    option_domain.size() - exception_domain.size() - 1;
                if (base::EndsWith(option_domain, exception_domain) &&
                    option_domain[dot_position] == '.')
                  potential_domain = option_domain;
                continue;
              }
            }

            if (option_domain.size() < exception_domain.size()) {
              size_t dot_position =
                  exception_domain.size() - option_domain.size() - 1;
              if (base::EndsWith(exception_domain, option_domain) &&
                  exception_domain[dot_position] == '.' &&
                  (potential_domain.empty() ||
                   potential_domain.size() > exception_domain.size()))
                potential_domain = exception_domain;
            }
          }
          if (!potential_domain.empty()) {
            filter_rule.included_domains.insert(std::string(potential_domain));
          }
        }
      } else if (option_domains) {
        filter_rule.included_domains.swap(option_domains.value());
      } else if (exception_domains) {
        filter_rule.included_domains.swap(exception_domains.value());
      }
      filter_rule.resource_types = option_types.value();
      if (exception_types)
        filter_rule.resource_types &= exception_types.value();
    } else {
      if (option_domains)
        filter_rule.included_domains.swap(option_domains.value());
      filter_rule.resource_types = option_types.value();
      if (!ignore) {
        DCHECK(default_ignore &&
               filter_rule.decision != RequestFilterRule::kPass);
        // Under the DDG implementation, exceptions always mean ignore, so
        // they're only meaningful for block rules
        if (exception_domains)
          filter_rule.excluded_domains.swap(exception_domains.value());
        // Exceptions have priority over options.
        if (exception_types)
          filter_rule.resource_types &= ~exception_types.value();
      }
    }

    if (filter_rule.resource_types.none()) {
      parse_result_->rules_info.unsupported_rules++;
      return;
    }

    if (plain_pattern.empty()) {
      filter_rule.pattern_type = RequestFilterRule::kRegex;
      filter_rule.pattern = *pattern;
      filter_rule.ngram_search_string = ngram_search_string;
    } else {
      filter_rule.pattern = plain_pattern;
    }
    filter_rule.host = domain;

    if (excluded_origins) {
      for (const auto& origin : *excluded_origins) {
        if (origin.is_string())
          filter_rule.excluded_domains.insert(origin.GetString());
      }
    }

    parse_result_->request_filter_rules.push_back(std::move(filter_rule));
    parse_result_->rules_info.valid_rules++;
  }

  if (make_redirect_rule) {
    RequestFilterRule redirect_rule;
    redirect_rule.party.set();
    if (option_domains)
      redirect_rule.included_domains.swap(option_domains.value());
    redirect_rule.resource_types = option_types.value();
    if (default_ignore) {
      // If we are blocking for the tracker, the exceptions are handled by
      // an pass rule instead
      if (exception_domains)
        redirect_rule.excluded_domains.swap(exception_domains.value());
      if (exception_types)
        redirect_rule.resource_types &= ~exception_types.value();
    }
    if (redirect_rule.resource_types.none()) {
      parse_result_->rules_info.unsupported_rules++;
      return;
    }

    if (plain_pattern.empty()) {
      redirect_rule.pattern_type = RequestFilterRule::kRegex;
      redirect_rule.pattern = *pattern;
      redirect_rule.ngram_search_string = ngram_search_string;
    } else {
      redirect_rule.pattern = plain_pattern;
    }
    redirect_rule.host = domain;

    if (excluded_origins) {
      for (const auto& origin : *excluded_origins) {
        if (origin.is_string())
          redirect_rule.excluded_domains.insert(origin.GetString());
      }
    }

    redirect_rule.modifier = RequestFilterRule::kRedirect;
    redirect_rule.modifier_values = {*surrogate};

    parse_result_->request_filter_rules.push_back(std::move(redirect_rule));
    parse_result_->rules_info.valid_rules++;
  }
}

std::optional<std::bitset<RequestFilterRule::kTypeCount>>
DuckDuckGoRulesParser::GetTypes(const base::Value* rule_properties) {
  std::bitset<RequestFilterRule::kTypeCount> types;
  const base::Value::List* types_value =
      rule_properties->GetDict().FindList(kTypesKey);
  if (!types_value)
    return std::nullopt;

  for (const auto& type_name : *types_value) {
    if (!type_name.is_string())
      continue;

    auto type = kTypeStringMap.find(type_name.GetString());
    if (type == kTypeStringMap.end())
      continue;
    else
      types.set(type->second);
  }

  return types;
}

std::optional<std::set<std::string>> DuckDuckGoRulesParser::GetDomains(
    const base::Value* rule_properties) {
  std::set<std::string> domains;
  const base::Value::List* domains_value =
      rule_properties->GetDict().FindList(kDomainsKey);
  if (!domains_value)
    return std::nullopt;

  for (const auto& domain : *domains_value) {
    if (!domain.is_string())
      continue;

    domains.insert(domain.GetString());
  }

  return domains;
}

}  // namespace adblock_filter
