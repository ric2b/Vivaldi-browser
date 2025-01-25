// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/flat_rules_compiler.h"

#include <bitset>
#include <map>
#include <type_traits>

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/ios/ios_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chromium/url/url_util.h"
#include "components/ad_blocker/adblock_content_injection_rule.h"
#include "components/ad_blocker/adblock_request_filter_rule.h"
#include "components/ad_blocker/utils.h"
#include "ios/ad_blocker/utils.h"

namespace adblock_filter {

namespace {

constexpr auto kResourceTypeMap =
    base::MakeFixedFlatMap<RequestFilterRule::ResourceType, std::string_view>({
        {RequestFilterRule::kStylesheet, "style-sheet"},
        {RequestFilterRule::kImage, "image"},
        {RequestFilterRule::kObject, "media"},
        {RequestFilterRule::kScript, "script"},
        {RequestFilterRule::kXmlHttpRequest, "fetch"},
        {RequestFilterRule::kSubDocument, "document"},
        {RequestFilterRule::kFont, "font"},
        {RequestFilterRule::kMedia, "media"},
        {RequestFilterRule::kWebSocket, "websocket"},
        {RequestFilterRule::kPing, "ping"},
        {RequestFilterRule::kOther, "other"},
    });

constexpr char kDelim = '^';
constexpr char kWildcard = '*';
constexpr char kRegexBegin[] = "^";
constexpr char kRegexEnd[] = "$";
constexpr char kRegexOptional[] = "?";
constexpr char kSchemeRegex[] = "^[a-z][a-z0-9.+-]*:(\\/\\/)?";
constexpr char kUserInfoRegex[] = "([^\\/]+@)?";
constexpr char kUserInfoAndSubdomainRegex[] = "(([^\\/]+@)?[^@:\\/\\[]+\\.)?";
constexpr char kEndDomainRegex[] = "[:\\/]";
constexpr char kDelimRegex[] = "[^a-zA-Z0-9_.%-]";
constexpr char kLastDelimRegex[] = "([^a-zA-Z0-9_.%-].*)?$";
constexpr char kWildcardRegex[] = ".*";

enum class LoadContext { kAny = 0, kTopFrame, kChildFrame };

class Trigger {
 public:
  Trigger(std::string url_filter, bool case_sensitive)
      : url_filter_(url_filter), case_sensitive_(case_sensitive) {}
  virtual ~Trigger() = default;

  Trigger& operator=(Trigger&&) = default;
  Trigger(Trigger&&) = default;

  Trigger Clone() const { return Trigger(*this); }

  base::Value::Dict ToDict() const {
    base::Value::Dict result;
    result.Set(rules_json::kUrlFilter, url_filter_);
    result.Set(rules_json::kUrlFilterIsCaseSensitive, case_sensitive_);
    if (!resource_type_.all() && !resource_type_.none()) {
      base::Value::List resource_type;
      for (size_t i = 0; i < resource_type_.size(); i++) {
        if (resource_type_.test(i)) {
          resource_type.Append(base::Value(kResourceTypeMap.at(i)));
        }
      }
      result.Set(rules_json::kResourceType, std::move(resource_type));
    }
    if (!load_type_.all() && !load_type_.none()) {
      base::Value::List load_type;
      if (load_type_.test(RequestFilterRule::kFirstParty))
        load_type.Append(base::Value(rules_json::kFirstParty));
      if (load_type_.test(RequestFilterRule::kThirdParty))
        load_type.Append(base::Value(rules_json::kThirdParty));
      result.Set(rules_json::kLoadType, std::move(load_type));
    }
    base::Value::List load_context;
    switch (load_context_) {
      case LoadContext::kChildFrame:
        load_context.Append(rules_json::kChildFrame);
        break;
      case LoadContext::kTopFrame:
        load_context.Append(rules_json::kTopFrame);
        break;
      case LoadContext::kAny:
        break;
    }
    if (!load_context.empty()) {
      result.Set(rules_json::kLoadContext, std::move(load_context));
    }

    if (!top_url_filter_.empty()) {
      base::Value::List top_url_filter;
      for (const auto& url : top_url_filter_) {
        top_url_filter.Append(url);
      }
      result.Set(top_url_filter_is_excluding_ ? rules_json::kUnlessTopUrl
                                              : rules_json::kIfTopUrl,
                 std::move(top_url_filter));
      result.Set(rules_json::kTopUrlFilterIsCaseSensitive,
                 top_url_filter_is_case_sensitive_);
    }

    return result;
  }

  void set_resource_type(std::bitset<RequestFilterRule::kTypeCount> type) {
    resource_type_ = type;
  }

  void set_load_type(std::bitset<RequestFilterRule::kPartyCount> load_type) {
    load_type_ = load_type;
  }

  void set_load_context(LoadContext context) { load_context_ = context; }

  void set_top_url_filter(std::string url,
                          bool is_exclude,
                          bool case_sensitive) {
    std::vector<std::string> urls;
    urls.push_back(std::move(url));
    set_top_url_filter(urls, is_exclude, case_sensitive);
  }
  void set_top_url_filter(std::vector<std::string> urls,
                          bool is_exclude,
                          bool case_sensitive) {
    top_url_filter_ = urls;
    top_url_filter_is_excluding_ = is_exclude;
    top_url_filter_is_case_sensitive_ = case_sensitive;
  }

 private:
  Trigger(const Trigger&) = default;

  std::string url_filter_;
  bool case_sensitive_;
  std::bitset<RequestFilterRule::kTypeCount> resource_type_;
  std::bitset<RequestFilterRule::kPartyCount> load_type_;
  LoadContext load_context_;
  std::vector<std::string> top_url_filter_;
  bool top_url_filter_is_excluding_;
  bool top_url_filter_is_case_sensitive_;
};

class Action {
 public:
  ~Action() = default;

  Action(Action&&) = default;
  Action& operator=(Action&&) = default;

  Action Clone() const { return Action(*this); }

  static Action BlockAction() { return Action(rules_json::kBlock); }

  static Action IgnorePreviousAction() {
    return Action(rules_json::kIgnorePrevious);
  }

  static Action CssHideAction(std::string selector) {
    Action action(rules_json::kCssHide);
    action.selector_ = selector;
    return action;
  }

  virtual base::Value::Dict ToDict() const {
    base::Value::Dict result;
    result.Set(rules_json::kType, type_);
    if (type_ == rules_json::kCssHide) {
      result.Set(rules_json::kSelector, selector_);
    } else if (type_ == rules_json::kRedirect) {
      result.Set(rules_json::kUrl, redirect_url_);
    } else if (type_ == rules_json::kCsp) {
      result.Set(rules_json::kPriority, 0);
      base::Value::Dict modify_header_info;
      modify_header_info.Set(rules_json::kOperation, rules_json::kAppend);
      modify_header_info.Set(rules_json::kHeader, rules_json::kCsp);
      modify_header_info.Set(rules_json::kValue, csp_);
      base::Value::Dict modify_header_actions;
      modify_header_actions.Set(rules_json::kResponseHeaders,
                                std::move(modify_header_info));
      result.Set(rules_json::kModifyHeaders, std::move(modify_header_actions));
    }
    return result;
  }

 private:
  Action(const char* type) : type_(type) {}
  Action(const Action&) = default;
  Action& operator=(const Action&) = default;

  const char* type_;
  std::string selector_;
  std::string redirect_url_;
  std::string csp_;
};

base::Value::Dict MakeRule(const Trigger& trigger, const Action& action) {
  base::Value::Dict result;
  result.Set(rules_json::kTrigger, trigger.ToDict());
  result.Set(rules_json::kAction, action.ToDict());

  return result;
}

void AppendfromPattern(std::string_view pattern, std::string& result) {
  for (const auto c : pattern) {
    switch (c) {
      case kWildcard:
        result.append(kWildcardRegex);
        break;
      case kDelim:
        result.append(kDelimRegex);
        break;
      case '.':
      case '+':
      case '$':
      case '?':
      case '{':
      case '}':
      case '(':
      case ')':
      case '[':
      case ']':
      case '|':
      case '/':
      case '\\':
        result.append("\\");
        [[fallthrough]];
      default:
        result.append(1, c);
    }
  }
}

std::optional<std::string> GetRegexFromRule(const RequestFilterRule& rule) {
  std::string_view pattern(rule.pattern);

  // Unicode not supported by iOS content blocker
  if (!base::IsStringASCII(pattern))
    return std::nullopt;

  if (pattern.empty())
    return kWildcardRegex;

  if (rule.pattern_type == RequestFilterRule::kRegex) {
    bool escaped = false;

    if (pattern.front() == '^') {
      pattern.remove_prefix(1);
    }

    for (const auto c : pattern) {
      switch (c) {
        case '\\':
          escaped = !escaped;
          break;
        case '{':
        case '|':
        case '^':
          if (!escaped)
            return std::nullopt;
          break;
        default:
          // character classes, word boundaries and backreferences unsupported
          if ((base::IsAsciiAlpha(c) || base::IsAsciiDigit(c)) && escaped)
            return std::nullopt;
      }
    }

    return rule.pattern;
  }
  std::string result = "";

  bool start_anchored = rule.anchor_type.test(RequestFilterRule::kAnchorStart);
  bool host_anchored = rule.anchor_type.test(RequestFilterRule::kAnchorHost);
  if (rule.host && !start_anchored && !host_anchored) {
    std::string_view host(*rule.host);
    bool pattern_matches_host = false;
    size_t first_slash = pattern.find_first_of("/^");
    size_t pattern_host_size = first_slash;
    bool has_first_slash = true;
    if (first_slash == std::string_view::npos) {
      pattern_host_size = pattern.size();
      has_first_slash = false;
    }
    if (host.size() < pattern_host_size &&
        pattern.substr(pattern_host_size - host.size(), host.size()) == host &&
        pattern[pattern_host_size - host.size() - 1] == '.') {
      GURL validation_url(std::string("https://") +
                          std::string(pattern.substr(0, pattern_host_size)));
      if (validation_url.is_valid() && validation_url.has_host() &&
          !validation_url.has_query() && !validation_url.has_ref() &&
          !validation_url.has_username() && !validation_url.has_password()) {
        host_anchored = true;
        pattern_matches_host = true;
      }
    }
    if (host.size() >= pattern_host_size) {
      if (has_first_slash &&
          host.substr(host.size() - pattern_host_size, pattern_host_size) ==
              pattern.substr(0, pattern_host_size)) {
        pattern.remove_prefix(pattern_host_size);
        pattern_matches_host = true;
      } else if (!has_first_slash &&
                 host.find(pattern) != std::string_view::npos) {
        pattern_matches_host = true;
      }
    }

    if (!host_anchored) {
      result.append(kSchemeRegex);
      result.append(kUserInfoAndSubdomainRegex);
      AppendfromPattern(*rule.host, result);
    }

    if (!has_first_slash && pattern_matches_host) {
      if (host_anchored) {
        result.append(kSchemeRegex);
        result.append(kUserInfoAndSubdomainRegex);
        AppendfromPattern(pattern, result);
      }
      result.append(kDelimRegex);
      return result;
    }

    if (!pattern_matches_host) {
      result.append(kDelimRegex);
      result.append(kWildcardRegex);
    }
  }

  if (start_anchored) {
    result.append(kRegexBegin);
  } else if (host_anchored) {
    result.append(kSchemeRegex);
    result.append(kUserInfoAndSubdomainRegex);
  }

  bool ends_with_delim = pattern.back() == kDelim;
  if (ends_with_delim) {
    pattern.remove_suffix(1);
  }

  AppendfromPattern(pattern, result);

  if (rule.anchor_type.test(RequestFilterRule::kAnchorEnd)) {
    if (ends_with_delim) {
      result.append(kDelimRegex);
      result.append(kRegexOptional);
    }
    result.append(kRegexEnd);
  } else if (ends_with_delim) {
    result.append(kLastDelimRegex);
  }

  return result;
}

std::string DomainToIfURL(std::string domain, bool subdomains) {
  std::string result(kSchemeRegex);
  if (subdomains)
    result.append(kUserInfoAndSubdomainRegex);
  else
    result.append(kUserInfoRegex);
  AppendfromPattern(domain, result);
  result.append(kEndDomainRegex);

  return result;
}

std::vector<std::string> DomainsToIfURL(std::set<std::string> domains) {
  std::vector<std::string> results;
  std::transform(
      domains.cbegin(), domains.cend(), std::back_inserter(results),
      [](std::string domain) { return DomainToIfURL(domain, true); });

  return results;
}

struct DomainTreeNode {
  std::map<std::string, DomainTreeNode> subdomains;
  enum DomainType { kNone = 0, kIncluded, kExcluded };
  DomainType domain_type = kNone;
  bool overriden = false;
};

void BuildDomainTree(std::set<std::string> domains,
                     bool excluded,
                     DomainTreeNode& root) {
  for (const auto& domain : domains) {
    DomainTreeNode* current = &root;
    std::vector<std::string> domain_pieces = base::SplitString(
        domain, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (std::string& piece : base::Reversed(domain_pieces)) {
      if ((current->domain_type == DomainTreeNode::kExcluded && !excluded) ||
          (current->domain_type == DomainTreeNode::kIncluded && excluded)) {
        current->overriden = true;
      }
      current = &(current->subdomains[std::move(piece)]);
    }

    // Exclusions have priority over inclusions.
    if (current->domain_type != DomainTreeNode::kExcluded)
      current->domain_type =
          excluded ? DomainTreeNode::kExcluded : DomainTreeNode::kIncluded;
  }
}

struct DomainForRule {
  DomainForRule(std::string domain, bool overriden)
      : domain(domain), overriden(overriden) {}
  std::string domain;
  bool overriden;
};

std::vector<std::string> DomainsForRuleToIfUrls(
    std::vector<DomainForRule> domains_for_rule) {
  std::vector<std::string> results;
  std::transform(domains_for_rule.cbegin(), domains_for_rule.cend(),
                 std::back_inserter(results),
                 [](DomainForRule domain_for_rule) {
                   return DomainToIfURL(domain_for_rule.domain, true);
                 });

  return results;
}
// Compute which blocks/allows are actually meaningful. Each entry in the map
// is a further level of allowing/blocking in the domain tree. Once this is
// done, the domains listed for each depth are subdomains exempted from the
// rule set at the depth one lower. For instance, if we have a blocking rule
// with domains=example.com,~bad.example.com,good.x.bad.example.com, we'll end
// up with example.com at level 0, bad.example.com at level one and
// good.x.example.com at level2. The even layers list domains included by the
// rule and odd layers list domains which are excluded. As such, if we are
// trying to populate an even depth and encounter an exclusion domain, it is
// already superceded by an excludion at the preceding level and we can ignore
// it. Same goes for inclusions and odd levels. Note that excluded domains are
// ignored at level 0 because the presence of inclusions implies that
// everything else is excluded and those exclusions are therefore redundant.
void TraverseDomainTree(
    const DomainTreeNode& node,
    std::string domain,
    int depth,
    std::map<int, std::vector<DomainForRule>>& domains_for_rule) {
  if ((node.domain_type == DomainTreeNode::kIncluded && depth % 2 == 0) ||
      (node.domain_type == DomainTreeNode::kExcluded && depth % 2 == 1)) {
    domains_for_rule[depth++].push_back(DomainForRule(domain, node.overriden));
  }

  for (const auto& [subdomain, sub_node] : node.subdomains) {
    TraverseDomainTree(sub_node,
                       domain.empty() ? subdomain : subdomain + "." + domain,
                       depth, domains_for_rule);
  }
}

base::Value::List* GetTarget(base::Value::Dict& compiled_rules,
                             RequestFilterRule::Decision decision,
                             bool is_generic) {
  switch (decision) {
    case RequestFilterRule::kModify:
      return compiled_rules.EnsureDict(rules_json::kBlockRules)
          ->EnsureList(is_generic ? rules_json::kGeneric
                                  : rules_json::kSpecific);
    case RequestFilterRule::kPass:
      return compiled_rules.EnsureList(rules_json::kAllowRules);
    case RequestFilterRule::kModifyImportant:
      return compiled_rules.EnsureList(rules_json::kBlockImportantRules);
  }
}

// iOS cannot handle triggers with both if-* and unless-* rules.
// First, we try to process the lists to remove anything redundant and split
// out instances where some inclusions/exclusions are subdomains of each
// other.
void CompileRuleWithDomains(RequestFilterRule::Decision decision,
                            const std::set<std::string>& included_domains,
                            const std::set<std::string>& excluded_domains,
                            base::Value::Dict& compiled_rules,
                            Trigger trigger,
                            Action block_action) {
  if (included_domains.empty() || excluded_domains.empty()) {
    bool is_generic = true;
    if (!excluded_domains.empty()) {
      trigger.set_top_url_filter(DomainsToIfURL(excluded_domains), true, true);
    }

    if (!included_domains.empty()) {
      trigger.set_top_url_filter(DomainsToIfURL(included_domains), false, true);
      is_generic = false;
    }

    base::Value::List* target = GetTarget(compiled_rules, decision, is_generic);
    CHECK(target);

    Action action = (decision == RequestFilterRule::kPass)
                        ? Action::IgnorePreviousAction()
                        : std::move(block_action);

    target->Append(MakeRule(trigger, action));
    return;
  }

  DomainTreeNode root;
  BuildDomainTree(included_domains, false, root);
  BuildDomainTree(excluded_domains, true, root);

  std::map<int, std::vector<DomainForRule>> domains_for_rule;
  TraverseDomainTree(root, "", 0, domains_for_rule);

  if (domains_for_rule.count(0) == 0) {
    // All inclusions were cancelled by exclusions, making the rule a noop
    return;
  }

  if (domains_for_rule.count(1) == 0) {
    // All exclusions were redundant. Make a rule based on inclusions only.
    trigger.set_top_url_filter(DomainsForRuleToIfUrls(domains_for_rule.at(0)),
                               false, true);
    base::Value::List* target = GetTarget(compiled_rules, decision, false);
    CHECK(target);
    Action action = (decision == RequestFilterRule::kPass)
                        ? Action::IgnorePreviousAction()
                        : std::move(block_action);
    target->Append(MakeRule(trigger, action));
    return;
  }
  if (decision == RequestFilterRule::kPass) {
    // Unfortunately, for allow rules, we have no way of producing a rule that
    // cancels an ignore previous action for subdomains. So, we just elect to
    // exclude not use a wildcard domain if a subdomain has an override.
    std::vector<std::string> if_urls;
    for (int i = 0; domains_for_rule.count(i) != 0; i += 2) {
      for (const auto& domain_for_rule : domains_for_rule.at(i)) {
        if_urls.push_back(
            DomainToIfURL(domain_for_rule.domain, !domain_for_rule.overriden));
      }
    }
    trigger.set_top_url_filter(std::move(if_urls), false, true);
    compiled_rules.EnsureList(rules_json::kAllowRules)
        ->Append(MakeRule(trigger, Action::IgnorePreviousAction()));
    return;
  }

  base::Value::List* target =
      compiled_rules.EnsureList(rules_json::kBlockAllowPairs);
  base::Value::List current_pair;
  int i;
  for (i = 0; domains_for_rule.count(i) != 0; i++) {
    Trigger trigger2 = trigger.Clone();
    trigger2.set_top_url_filter(DomainsForRuleToIfUrls(domains_for_rule.at(i)),
                                false, true);
    if (i % 2 == 0) {
      DCHECK(current_pair.empty());
      current_pair.Append(MakeRule(trigger2, block_action.Clone()));
    } else {
      current_pair.Append(MakeRule(trigger2, Action::IgnorePreviousAction()));
      target->Append(std::move(current_pair));
    }
  }
  if (i % 2 != 0) {
    target->Append(std::move(current_pair));
  }
}

void CompilePlainRequestFilter(const RequestFilterRule& rule,
                               base::Value::Dict& compiled_request_filter_rules,
                               Trigger trigger) {
  if (rule.modifier != RequestFilterRule::kNoModifier) {
    // TODO(julien): Implement
    return;
  }

  if (!rule.ad_domains_and_query_triggers.empty()) {
    // No possibility to support this on iOS.
    return;
  }

  CompileRuleWithDomains(rule.decision, rule.included_domains,
                         rule.excluded_domains, compiled_request_filter_rules,
                         std::move(trigger), Action::BlockAction());
}

void CompileRequestFilterRule(
    const RequestFilterRule& rule,
    base::Value::Dict& compiled_request_filter_rules,
    base::Value::Dict& compiled_cosmetic_filter_rules) {
  static const std::bitset<RequestFilterRule::kTypeCount> subdocument_type =
      (1 << RequestFilterRule::kSubDocument);
  std::optional<std::string> url_filter = GetRegexFromRule(rule);
  if (!url_filter)
    return;
  auto resource_types = rule.resource_types;
  auto explicit_types = rule.explicit_types;
  auto activations = rule.activation_types;
  if (!resource_types.none() ||
      (explicit_types.test(RequestFilterRule::kDocument) &&
       rule.decision != RequestFilterRule::kPass)) {
    Trigger trigger(*url_filter, rule.is_case_sensitive);
    trigger.set_load_type(rule.party);

    if (explicit_types.test(RequestFilterRule::kDocument) &&
        rule.decision != RequestFilterRule::kPass) {
      resource_types.set(RequestFilterRule::kSubDocument);
    } else if (resource_types.test(RequestFilterRule::kSubDocument)) {
      resource_types.reset(RequestFilterRule::kSubDocument);
      Trigger subdocument_trigger = trigger.Clone();
      subdocument_trigger.set_load_context(LoadContext::kChildFrame);
      subdocument_trigger.set_resource_type(subdocument_type);
      CompilePlainRequestFilter(rule, compiled_request_filter_rules,
                                std::move(subdocument_trigger));
    }

    // Unsupported on iOS.
    resource_types.reset(RequestFilterRule::kWebTransport);
    resource_types.reset(RequestFilterRule::kWebBundle);
    resource_types.reset(RequestFilterRule::kWebRTC);

    // Remaining types after handling subdocument
    if (!resource_types.none()) {
      trigger.set_resource_type(resource_types);
      CompilePlainRequestFilter(rule, compiled_request_filter_rules,
                                std::move(trigger));
    }
  }

  if (rule.decision == RequestFilterRule::kPass && !activations.none()) {
    Trigger trigger(kWildcardRegex, false);
    trigger.set_load_type(rule.party);
    trigger.set_top_url_filter(*url_filter, false, rule.is_case_sensitive);
    if (activations.test(RequestFilterRule::kDocument)) {
      compiled_request_filter_rules.EnsureList(rules_json::kAllowRules)
          ->Append(MakeRule(trigger, Action::IgnorePreviousAction()));
      compiled_cosmetic_filter_rules.EnsureList(rules_json::kAllowRules)
          ->Append(MakeRule(trigger, Action::IgnorePreviousAction()));
    }

    if (activations.test(RequestFilterRule::kGenericBlock)) {
      compiled_request_filter_rules.EnsureList(rules_json::kGenericAllowRules)
          ->Append(MakeRule(trigger, Action::IgnorePreviousAction()));
    }

    if (activations.test(RequestFilterRule::kElementHide)) {
      compiled_cosmetic_filter_rules.EnsureList(rules_json::kAllowRules)
          ->Append(MakeRule(trigger, Action::IgnorePreviousAction()));
    }

    if (activations.test(RequestFilterRule::kGenericHide)) {
      compiled_cosmetic_filter_rules.EnsureList(rules_json::kGenericAllowRules)
          ->Append(MakeRule(trigger, Action::IgnorePreviousAction()));
    }
  }
}

void CompileCosmeticRule(const CosmeticRule& rule,
                         base::Value::Dict& compiled_cosmetic_filter_rules) {
  CompileRuleWithDomains(
      rule.core.is_allow_rule ? RequestFilterRule::kPass
                              : RequestFilterRule::kModify,
      rule.core.included_domains, rule.core.excluded_domains,
      *(compiled_cosmetic_filter_rules.EnsureDict(rules_json::kSelector)
            ->EnsureDict(rule.selector)),
      Trigger(kWildcardRegex, false), Action::CssHideAction(rule.selector));
}

void AddScriptletRule(const ScriptletInjectionRule& rule,
                      const DomainTreeNode& node,
                      base::Value::Dict& dict) {
  if (node.domain_type != DomainTreeNode::kNone) {
    base::Value::List arguments;
    for (const auto& argument : rule.arguments) {
      arguments.Append(argument);
    }
    dict.EnsureDict((node.domain_type == DomainTreeNode::kIncluded)
                        ? rules_json::kIncluded
                        : rules_json::kExcluded)
        ->EnsureList(rule.scriptlet_name)
        ->Append(std::move(arguments));
    if (!node.overriden)
      return;
  }
  for (const auto& [subdomain, sub_node] : node.subdomains) {
    AddScriptletRule(rule, sub_node, *dict.EnsureDict(subdomain));
  }
}

void CompileScriptletInjectionRule(
    const ScriptletInjectionRule& rule,
    base::Value::Dict& compiled_cosmetic_filter_rules) {
  // We don't expect allow rules so long as only abp scriptlets are supported
  DCHECK(!rule.core.is_allow_rule);
  DCHECK(!rule.core.included_domains.empty());

  DomainTreeNode root;
  BuildDomainTree(rule.core.included_domains, false, root);
  BuildDomainTree(rule.core.excluded_domains, true, root);

  AddScriptletRule(rule, root, compiled_cosmetic_filter_rules);
}
}  // namespace

std::string CompileIosRulesToString(const ParseResult& parse_result,
                                    bool pretty_print) {
  base::Value::Dict compiled_request_filter_rules;
  base::Value::Dict compiled_cosmetic_filter_rules;
  base::Value::Dict compiled_scriptlet_injection_rules;
  for (const auto& request_filter_rule : parse_result.request_filter_rules) {
    CompileRequestFilterRule(request_filter_rule, compiled_request_filter_rules,
                             compiled_cosmetic_filter_rules);
  }
  for (const auto& cosmetic_rule : parse_result.cosmetic_rules) {
    CompileCosmeticRule(cosmetic_rule, compiled_cosmetic_filter_rules);
  }
  for (const auto& scriptlet_injection_rule :
       parse_result.scriptlet_injection_rules) {
    CompileScriptletInjectionRule(scriptlet_injection_rule,
                                  compiled_scriptlet_injection_rules);
  }
  base::Value::Dict result;
  result.Set(rules_json::kVersion,
             GetIntermediateRepresentationVersionNumber());
  result.Set(rules_json::kNetworkRules,
             std::move(compiled_request_filter_rules));
  result.Set(rules_json::kCosmeticRules,
             std::move(compiled_cosmetic_filter_rules));
  result.Set(rules_json::kScriptletRules,
             std::move(compiled_scriptlet_injection_rules));
  std::string output;
  JSONStringValueSerializer serializer(&output);
  serializer.set_pretty_print(pretty_print);
  serializer.Serialize(base::Value(std::move(result)));
  return output;
}

bool CompileIosRules(const ParseResult& parse_result,
                     const base::FilePath& output_path,
                     std::string& checksum) {
  if (!base::CreateDirectory(output_path.DirName()))
    return false;
  std::string ios_rules = CompileIosRulesToString(parse_result, false);
  checksum = CalculateBufferChecksum(ios_rules);
  return base::WriteFile(output_path, ios_rules);
}

base::Value CompileExceptionsRule(const std::set<std::string>& exceptions,
                                  bool process_list) {
  Trigger trigger(kWildcardRegex, false);
  trigger.set_top_url_filter(DomainsToIfURL(exceptions), process_list, true);

  return base::Value(MakeRule(trigger, Action::IgnorePreviousAction()));
}
}  // namespace adblock_filter
