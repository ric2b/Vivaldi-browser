// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_rule_parser.h"

#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_string_value_serializer.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/ad_blocker/parse_utils.h"
#include "net/base/ip_address.h"

namespace adblock_filter {

namespace {
const char kHomepageTag[] = "Homepage:";
const char kTitleTag[] = "Title:";
const char kLicenseTag[] = "Licence:";
const char kRedirectTag[] = "Redirect:";
const char kExpiresTag[] = "Expires:";
const char kVersionTag[] = "Version:";

const char kRewritePrefix[] = "abp-resource:";

enum class OptionType {
  kAll,
  kThirdParty,
  kMatchCase,
  kDomain,
  kCSP,
  kHost,  // Vivaldi-specific, allows us to handle DDG filter.
  kRewrite,
  kRedirect,
  kRedirectRule,
  kImportant,
  // Document can be both an activation and an explicit type
  kDocument,
  kAdQueryTrigger,
  kAdAttributionTracker
};

struct OptionDefinition {
  OptionType type;
  bool invert = false;
  bool allow_invert = false;
  enum { kRequired, kRequiredForModify, kForbidden } value = kForbidden;
};

constexpr auto kOptionMap = base::MakeFixedFlatMap<std::string_view,
                                                   OptionDefinition>(
    {{"all", {.type = OptionType::kAll}},
     {"third-party", {.type = OptionType::kThirdParty, .allow_invert = true}},
     {"3p", {.type = OptionType::kThirdParty, .allow_invert = true}},
     {"first-party",
      {.type = OptionType::kThirdParty, .invert = true, .allow_invert = true}},
     {"1p",
      {.type = OptionType::kThirdParty, .invert = true, .allow_invert = true}},
     {"match-case", {.type = OptionType::kMatchCase}},
     {"domain",
      {.type = OptionType::kDomain, .value = OptionDefinition::kRequired}},
     {"from",
      {.type = OptionType::kDomain, .value = OptionDefinition::kRequired}},
     {"host",
      {.type = OptionType::kHost, .value = OptionDefinition::kRequired}},
     {"csp",
      {.type = OptionType::kCSP,
       .value = OptionDefinition::kRequiredForModify}},
     {"rewrite",
      {.type = OptionType::kRewrite, .value = OptionDefinition::kRequired}},
     {"redirect",
      {.type = OptionType::kRedirect,
       .value = OptionDefinition::kRequiredForModify}},
     {"redirect-rule",
      {.type = OptionType::kRedirectRule,
       .value = OptionDefinition::kRequiredForModify}},
     {"important", {.type = OptionType::kImportant}},
     {"document",
      {.type = OptionType::kDocument,
       .allow_invert = true,
       .value = OptionDefinition::kForbidden}},
     {"doc",
      {.type = OptionType::kDocument,
       .allow_invert = true,
       .value = OptionDefinition::kForbidden}},
     {"ad-query-trigger",
      {.type = OptionType::kAdQueryTrigger,
       .value = OptionDefinition::kRequiredForModify}},
     {"ad-attribution-tracker",
      {.type = OptionType::kAdAttributionTracker,
       .value = OptionDefinition::kRequired}}});
constexpr auto kExplicitTypeStringMap =
    base::MakeFixedFlatMap<std::string_view, int>(
        {{"popup", RequestFilterRule::kPopup}});

constexpr auto kActivationStringMap =
    base::MakeFixedFlatMap<std::string_view, int>(
        {{"elemhide", RequestFilterRule::kElementHide},
         {"ehide", RequestFilterRule::kElementHide},
         {"generichide", RequestFilterRule::kGenericHide},
         {"ghide", RequestFilterRule::kGenericHide},
         {"genericblock", RequestFilterRule::kGenericBlock},
         {"attribute-ads", RequestFilterRule::kAttributeAds}});

constexpr auto kAbpMainSnippetNames = base::MakeFixedFlatSet<std::string_view>({
#include "vivaldi/components/ad_blocker/abp_snippets_lists/main.inc"
});

constexpr auto kAbpIsolatedSnippetNames =
    base::MakeFixedFlatSet<std::string_view>({
#include "vivaldi/components/ad_blocker/abp_snippets_lists/isolated.inc"
    });

bool GetMetadata(std::string_view comment,
                 const std::string& tag_name,
                 std::string_view* result) {
  if (!base::StartsWith(comment, tag_name))
    return false;

  *result = base::TrimWhitespaceASCII(comment.substr(tag_name.length()),
                                      base::TRIM_LEADING);
  return true;
}

std::optional<GURL> GetUrlFromDomainString(std::string_view domain) {
  if (domain.find_first_of("/?") != std::string_view::npos)
    return std::nullopt;

  std::string url_str = "https://";
  url_str.append(domain);
  // This should result in a valid URL with only a host part.
  GURL validation_url(url_str);
  if (!validation_url.is_valid() || validation_url.has_port() ||
      validation_url.has_username())
    return std::nullopt;

  return validation_url;
}
}  // namespace

RuleParser::RuleParser(ParseResult* parse_result,
                       RuleSourceSettings source_settings)
    : parse_result_(parse_result), source_settings_(source_settings) {}
RuleParser::~RuleParser() = default;

RuleParser::Result RuleParser::Parse(std::string_view rule_string) {
  // Empty line are treated as a comment.
  if (rule_string.empty()) {
    return kComment;
  }

  // Assume the rules were trimmed before being passed to us.
  DCHECK(!base::IsAsciiWhitespace(rule_string.front()) &&
         !base::IsAsciiWhitespace(rule_string.back()));

  // Filters which consist of a single alphanumerical character are valid, but
  // do not make sense.
  if (rule_string.length() == 1 && (base::IsAsciiAlpha(rule_string[0]) ||
                                    base::IsAsciiDigit(rule_string[0]))) {
    return kUnsupported;
  }

  if (base::ToLowerASCII(rule_string).starts_with("[adblock")) {
    return kComment;
  }

  if (rule_string == "#" || rule_string.starts_with("# ") ||
      rule_string.starts_with("####")) {
    return kComment;
  }

  if (rule_string[0] == '!') {
    rule_string.remove_prefix(1);

    if (MaybeParseMetadata(
            base::TrimWhitespaceASCII(rule_string, base::TRIM_LEADING)))
      return kMetadata;
    return kComment;
  }

  size_t selector_separator_2 = std::string_view::npos;
  size_t maybe_selector_separator = rule_string.find('#');
  if (maybe_selector_separator != std::string_view::npos) {
    selector_separator_2 = rule_string.find('#', maybe_selector_separator + 1);
  }

  if (selector_separator_2 != std::string_view::npos) {
    ContentInjectionRuleCore content_injection_rule_core;
    std::string_view body = rule_string.substr(selector_separator_2 + 1);
    Result result = IsContentInjectionRule(
        rule_string, maybe_selector_separator, &content_injection_rule_core);
    switch (result) {
      case Result::kCosmeticRule: {
        if (!ParseCosmeticRule(body, std::move(content_injection_rule_core)))
          return Result::kError;
        return result;
      }
      case Result::kScriptletInjectionRule: {
        if (!ParseScriptletInjectionRule(
                body, std::move(content_injection_rule_core)))
          return Result::kError;
        return result;
      }
      case Result::kRequestFilterRule:
        break;
      default:
        return result;
    }
  }

  std::optional<Result> host_result = ParseHostsFileOrNakedHost(rule_string);
  if (host_result) {
    return *host_result;
  }

  RequestFilterRule rule;
  Result result = ParseRequestFilterRule(rule_string, rule);
  if (result != kRequestFilterRule)
    return result;

  parse_result_->request_filter_rules.push_back(std::move(rule));
  return result;
}

// clang-format off
/*
abp = AdBlock Plus
adg = AdGuard
uBO = uBlock Origin

 spearator | hostnames optional | meaning
-----------------------------------------
 ##        | depends on body    | regular cosmetic rule or any uBO extended rule
 #@#       | depends on body    | regular cosmetic exception rule or any uBO extended allow rule
 #?#       | abp: no, adg : yes | abp or adg cosmetic rule with extended CSS selectors
 #@?#      | yes                | adg cosmetic exception rule wth extended CSS selectors
 #$#       | no                 | abp snippet rule
 #$#       | yes                | adg CSS injection rule
 #@$#      | yes                | adg CSS injection exception rule
 #$?#      | yes                | adg CSS injection rule with extended selectors
 #@$?#     | yes                | adg CSS injection exception rule with extended selectors
 #%#       | yes                | adg javascript injection rule
 #@%#      | yes                | adg javascript injection exception rule
*/
// clang-format on

RuleParser::Result RuleParser::IsContentInjectionRule(
    std::string_view rule_string,
    size_t separator,
    ContentInjectionRuleCore* core) {
  // This assumes we have another '#' separator to look forward to.Under that
  // assumption, the following parsing code is safe until it encounters the
  // second separator.
  DCHECK(rule_string.find('#', separator + 1) != std::string_view::npos);

  size_t position = separator + 1;
  if (rule_string[position] == '@') {
    core->is_allow_rule = true;
    position++;
  }

  Result result = Result::kCosmeticRule;
  if (rule_string[position] == '%' || rule_string[position] == '?') {
    // "#%...", "#@%...", "#?..." or "#@?..."
    result = Result::kUnsupported;
    position++;
  } else if (rule_string[position] == '$') {
    // "#$..." or "#@$..."
    if (!source_settings_.allow_abp_snippets) {
      // Assume that if abp snippet rules are not allowed, we are dealing with
      // an adg CSS injection rule and vice-versa
      result = Result::kUnsupported;
    } else if (core->is_allow_rule) {
      // Snippet rules exceptions are not a thing.
      result = Result::kError;
    } else {
      result = Result::kScriptletInjectionRule;
    }
    position++;

    if (rule_string[position] == '?') {
      // "#$?..." or "#@$?..."
      if (source_settings_.allow_abp_snippets) {
        // adg rules in abp-specific rule file is considered an error.
        result = Result::kError;
      }
      position++;
    }
  }

  if (rule_string[position] != '#') {
    // If we haven't reached the second separator at this point, we have an
    // unexpected character sequence. Better try parsing this as a request
    // filter rule.
    return Result::kRequestFilterRule;
  }

  if (!ParseDomains(rule_string.substr(0, separator), ",",
                    core->included_domains, core->excluded_domains))
    return Result::kError;
  if (result == Result::kScriptletInjectionRule &&
      core->included_domains.empty())
    return Result::kError;

  return result;
}

bool RuleParser::ParseCosmeticRule(std::string_view body,
                                   ContentInjectionRuleCore rule_core) {
  // Rules should consist of a list of selectors. No actual CSS rules allowed.
  if (body.empty() || body.find('{') != std::string_view::npos ||
      body.find('}') != std::string_view::npos)
    return false;

  CosmeticRule rule;
  rule.selector = std::string(body);
  rule.core = std::move(rule_core);
  parse_result_->cosmetic_rules.push_back(std::move(rule));
  return true;
}

bool RuleParser::ParseScriptletInjectionRule(
    std::string_view body,
    ContentInjectionRuleCore rule_core) {
  ScriptletInjectionRule main_world_rule;
  ScriptletInjectionRule isolated_world_rule;
  main_world_rule.core = rule_core.Clone();
  isolated_world_rule.core = std::move(rule_core);
  // Use these names to signal an abp snippet filter.
  main_world_rule.scriptlet_name = kAbpSnippetsMainScriptletName;
  isolated_world_rule.scriptlet_name = kAbpSnippetsIsolatedScriptletName;

  std::string main_world_arguments_list;
  std::string isolated_world_arguments_list;

  for (std::string_view injection : base::SplitStringPiece(
           body, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    bool escaped = false;
    bool in_quotes = false;
    bool after_quotes = false;
    bool parsing_code_point = false;
    std::string code_point_str;
    base::Value::List arguments;
    std::string argument;

    for (const char c : injection) {
      if (parsing_code_point) {
        code_point_str += c;
        if (code_point_str.length() == 4) {
          parsing_code_point = false;
          uint32_t code_point;
          if (!base::HexStringToUInt(code_point_str, &code_point))
            continue;
          base::WriteUnicodeCharacter(code_point, &argument);
        }
      } else if (escaped) {
        switch (c) {
          case 'n':
            argument += '\n';
            break;
          case 'r':
            argument += '\r';
            break;
          case 't':
            argument += '\t';
            break;
          case 'u':
            code_point_str.clear();
            parsing_code_point = true;
            break;
          default:
            argument += c;
        }
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '\'') {
        in_quotes = !in_quotes;
        after_quotes = !in_quotes;
      } else if (in_quotes || !base::IsAsciiWhitespace(c)) {
        argument += c;
      } else if (!argument.empty() || after_quotes) {
        arguments.Append(std::move(argument));
      }

      if (c != '\'')
        after_quotes = false;
    }

    if (!argument.empty() || after_quotes) {
      arguments.Append(std::move(argument));
    }

    // Can happen if we have an argument string containing only a '\\' or a '\''
    if (arguments.size() == 0)
      continue;

    std::string command_name = arguments.front().GetString();
    std::string serialized_arguments;
    JSONStringValueSerializer(&serialized_arguments)
        .Serialize(base::Value(std::move(arguments)));
    bool valid = false;

    auto add_to_list = [&serialized_arguments, &valid](std::string& list) {
      list.append(serialized_arguments);
      list.append(",");
      valid = true;
    };

    if (kAbpMainSnippetNames.contains(command_name)) {
      add_to_list(main_world_arguments_list);
    }

    if (kAbpIsolatedSnippetNames.contains(command_name)) {
      add_to_list(isolated_world_arguments_list);
    }

    if (!valid) {
      return false;
    }
  }

  // We purposefully leave a trailing comma after the last item of the list
  // here. It will be taken into account in
  // ContentInjectionIndexTraversalResults::ToInjectionData

  if (!main_world_arguments_list.empty()) {
    main_world_rule.arguments.push_back(std::move(main_world_arguments_list));
    parse_result_->scriptlet_injection_rules.push_back(
        std::move(main_world_rule));
  }

  if (!isolated_world_arguments_list.empty()) {
    isolated_world_rule.arguments.push_back(
        std::move(isolated_world_arguments_list));
    parse_result_->scriptlet_injection_rules.push_back(
        std::move(isolated_world_rule));
  }

  return true;
}

RuleParser::Result RuleParser::ParseRequestFilterRule(
    std::string_view rule_string,
    RequestFilterRule& rule) {
  if (base::StartsWith(rule_string, "@@")) {
    rule.decision = RequestFilterRule::kPass;
    rule_string.remove_prefix(2);
  }

  // The pattern part of regex rules starts and ends with '/'. Since
  // those rules can contain a '$' as an end-of-string marker, we only try to
  // find a '$' marking the beginning of the options section if the pattern
  // doesn't look like a whole-line regex
  size_t options_start = std::string_view::npos;
  if (rule_string[0] != '/' || rule_string.back() != '/') {
    options_start = rule_string.rfind('$');
  }
  if (options_start != std::string_view::npos && options_start != 0 &&
      rule_string[options_start - 1] == '$') {
    // AdGuard HTML filtering rules use $$ as separator
    return kUnsupported;
  }

  std::string_view options_string;
  if (options_start != std::string_view::npos)
    options_string = rule_string.substr(options_start);

  // Even if the options string is empty, there is some common setup code
  // that we want to run.
  Result result = ParseRequestFilterRuleOptions(options_string, rule);
  if (result != kRequestFilterRule)
    return result;

  std::string_view pattern = rule_string.substr(0, options_start);

  if (base::StartsWith(pattern, "/") && base::EndsWith(pattern, "/") &&
      pattern.length() > 1) {
    pattern.remove_prefix(1);
    pattern.remove_suffix(1);
    rule.pattern_type = RequestFilterRule::kRegex;
    rule.pattern = std::string(pattern);
    rule.ngram_search_string = BuildNgramSearchString(pattern);
    return kRequestFilterRule;
  }

  bool process_hostname = false;
  bool maybe_pure_host = true;

  if (base::StartsWith(pattern, "||")) {
    pattern.remove_prefix(2);

    // The host part would never start with a separator, so a separator
    // would not make sense.
    if (base::StartsWith(pattern, "^"))
      return kUnsupported;

    process_hostname = true;
    rule.anchor_type.set(RequestFilterRule::kAnchorHost);
  } else if (base::StartsWith(pattern, "|")) {
    rule.anchor_type.set(RequestFilterRule::kAnchorStart);
    pattern.remove_prefix(1);
  }

  if (base::StartsWith(pattern, "*")) {
    // Starting with a wildcard makes anchoring at the start meaningless
    pattern.remove_prefix(1);
    rule.anchor_type.reset(RequestFilterRule::kAnchorHost);
    rule.anchor_type.reset(RequestFilterRule::kAnchorStart);

    // Only try to find a hostname in hostname anchored patterns if the
    // pattern starts with *. or without a wildcard.
    if (!base::StartsWith(pattern, ".")) {
      process_hostname = false;
    }
  }

  // Stars at the start don't contribute to the pattern
  while (base::StartsWith(pattern, "*")) {
    pattern.remove_prefix(1);
  }

  if (base::EndsWith(pattern, "|")) {
    pattern.remove_suffix(1);
    rule.anchor_type.set(RequestFilterRule::kAnchorEnd);
  }

  // We had a pattern of the form "|*|", which is equivalent to "*"
  if (pattern.empty()) {
    rule.anchor_type.reset(RequestFilterRule::kAnchorEnd);
  }

  if (base::EndsWith(pattern, "*")) {
    // Ending with a wildcard makes anchoring at the end meaningless
    pattern.remove_suffix(1);
    rule.anchor_type.reset(RequestFilterRule::kAnchorEnd);
    maybe_pure_host = false;
  }

  // Stars at the end don't contribute to the pattern
  while (base::EndsWith(pattern, "*")) {
    pattern.remove_suffix(1);
  }

  if (pattern.find_first_of("*") != std::string_view::npos) {
    rule.pattern_type = RequestFilterRule::kWildcarded;
  }

  if (!process_hostname) {
    if (rule.modifier == RequestFilterRule::kAdQueryTrigger) {
      // ad-query-trigger rules should have host-matching pattern
      return kError;
    }

    if (!rule.is_case_sensitive) {
      rule.pattern =
          base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(pattern)));
    } else {
      rule.ngram_search_string =
          base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(pattern)));
      rule.pattern = std::string(pattern);
    }
    return kRequestFilterRule;
  }

  // The pattern was (nominally) anchored, so see if we have a hostname to
  // normalize at the start of it.
  std::string canonicalized_pattern;
  size_t authority_begin = 0;

  if (base::StartsWith(pattern, ".")) {
    authority_begin = 1;
    canonicalized_pattern = ".";
    maybe_pure_host = false;
  }

  size_t authority_end = pattern.find_first_of("/^*?");
  size_t authority_length;

  if (rule.modifier == RequestFilterRule::kAdQueryTrigger &&
      pattern[authority_end] == '*') {
    // ad-query-trigger rules should have host-matching pattern
    return kError;
  }

  if (authority_end == std::string_view::npos) {
    authority_length = std::string_view::npos;
  } else {
    authority_length = authority_end - authority_begin;
    // ^ allows to match any url with the given host part, similarly to a
    // pure host.
    if (pattern[authority_end] != '^' || authority_end + 1 < pattern.length())
      maybe_pure_host = false;
  }
  std::string potential_authority(
      pattern.substr(authority_begin, authority_length));

  // If the URL is valid, we also get the host part converted to punycode for
  // free.
  GURL validation_url(std::string("https://") + potential_authority);
  if (validation_url.is_valid() && validation_url.has_host()) {
    // This pattern is equivalent to a plain host check;
    if (!validation_url.has_port() && maybe_pure_host) {
      // This would basically be a block everything rule. Ignore it.
      if (rule.host)
        return kError;
      rule.host = validation_url.host();
    }
    canonicalized_pattern += validation_url.host();
    if (validation_url.has_port()) {
      canonicalized_pattern += ":" + validation_url.port();
    }
  } else {
    canonicalized_pattern += potential_authority;
  }

  if (authority_end != std::string_view::npos) {
    canonicalized_pattern += std::string(pattern.substr(authority_end));
  }

  if (!rule.is_case_sensitive) {
    rule.pattern = base::UTF16ToUTF8(
        base::i18n::FoldCase(base::UTF8ToUTF16(canonicalized_pattern)));
  } else {
    rule.pattern = canonicalized_pattern;
    rule.ngram_search_string =
        base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(pattern)));
  }

  return kRequestFilterRule;
}

bool RuleParser::MaybeAddPureHostRule(std::string_view maybe_hostname) {
  // Implement  /^([\da-z][\da-z_-]*\.)*[\da-z][\da-z-]*[\da-z]$/ to match
  // ublock

  bool last_component_has_underscore = false;
  char last_char = '.';
  bool has_multiple_components = false;
  for (const char c : maybe_hostname) {
    if (last_char == '.') {
      // These characters can't be the first of a component
      if (c == '.' || c == '-' || c == '_') {
        return false;
      }
    }
    last_char = c;

    if (base::IsAsciiAlphaNumeric(c) || c == '-') {
      continue;
    }

    if (c == '.') {
      last_component_has_underscore = false;
      has_multiple_components = true;
      continue;
    }

    if (c == '_') {
      last_component_has_underscore = true;
      continue;
    }

    // Unsupported character
    return false;
  }

  if (last_component_has_underscore || last_char == '.' || last_char == '-' ||
      !has_multiple_components) {
    return false;
  }

  RequestFilterRule rule;
  rule.anchor_type.set(RequestFilterRule::kAnchorHost);
  rule.host = maybe_hostname;
  rule.party.set();
  rule.resource_types.set();
  rule.pattern_type = RequestFilterRule::kPlain;
  rule.pattern = maybe_hostname;
  rule.pattern.append("^");
  parse_result_->request_filter_rules.push_back(std::move(rule));

  return true;
}

std::optional<RuleParser::Result> RuleParser::ParseHostsFileOrNakedHost(
    std::string_view rule_string) {
  size_t first_space = rule_string.find_first_of(" \t");
  if (first_space == std::string_view::npos) {
    if (source_settings_.naked_hostname_is_pure_host &&
        MaybeAddPureHostRule(rule_string)) {
      return kRequestFilterRule;
    }
    return std::nullopt;
  }
  // See if we have a hosts file entry.
  if (net::IPAddress::FromIPLiteral(rule_string.substr(0, first_space)) ==
      std::nullopt) {
    return std::nullopt;
  }
  rule_string.remove_prefix(first_space + 1);

  Result result = kUnsupported;
  for (auto hostname : base::SplitStringPiece(
           rule_string, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    if (net::IPAddress::FromIPLiteral(rule_string.substr(0, first_space)) !=
            std::nullopt ||
        hostname == "broadcasthost" || hostname == "local" ||
        hostname == "localhost" || hostname.starts_with("localhost.") ||
        hostname.starts_with("ip6-")) {
      // This is a valid entry, but we don't have a use for it.
      if (result != kRequestFilterRule) {
        result = kComment;
      }
      continue;
    }
    result = MaybeAddPureHostRule(hostname) ? kRequestFilterRule : result;
  }

  return result;
}

RuleParser::Result RuleParser::ParseRequestFilterRuleOptions(
    std::string_view options,
    RequestFilterRule& rule) {
  if (!options.empty()) {
    DCHECK_EQ('$', options[0]);
    options.remove_prefix(1);
  }

  bool add_implicit_types = true;
  std::bitset<RequestFilterRule::kTypeCount> types_set;
  std::bitset<RequestFilterRule::kTypeCount> types_unset;
  std::bitset<RequestFilterRule::kExplicitTypeCount> explicit_types_set;
  std::bitset<RequestFilterRule::kExplicitTypeCount> explicit_types_unset;
  std::bitset<RequestFilterRule::kActivationCount> activations_set;
  std::bitset<RequestFilterRule::kActivationCount> activations_unset;
  for (std::string_view option : base::SplitStringPiece(
           options, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // Any option that's a run of underscores is a noop.
    if (base::StartsWith(option, "_")) {
      if (option.find_first_not_of("_") != std::string_view::npos) {
        return kUnsupported;
      }
      continue;
    }

    bool invert = false;
    if (base::StartsWith(option, "~")) {
      option.remove_prefix(1);
      invert = true;
    }

    size_t option_name_end = option.find('=');
    std::string_view option_name = option.substr(0, option_name_end);
    std::optional<std::string_view> option_value;
    if (option_name_end != std::string::npos) {
      option_value = option.substr(option_name_end + 1);
    }

    auto type_option = kTypeStringMap.find(option_name);
    if (type_option != kTypeStringMap.end()) {
      if (option_value) {
        return kError;
      }
      if (invert)
        types_unset.set(type_option->second);
      else
        types_set.set(type_option->second);
      // Only add implicit types if we haven't added any otherwise.
      add_implicit_types = false;
      continue;
    }

    auto explicit_type_option = kExplicitTypeStringMap.find(option_name);
    if (explicit_type_option != kExplicitTypeStringMap.end()) {
      if (option_value) {
        return kError;
      }
      if (invert)
        explicit_types_unset.set(explicit_type_option->second);
      else
        explicit_types_set.set(explicit_type_option->second);
      // Only add implicit types if we haven't added any otherwise.
      add_implicit_types = false;
      continue;
    }

    auto activation_option = kActivationStringMap.find(option_name);
    if (activation_option != kActivationStringMap.end()) {
      if (option_value) {
        return kError;
      }
      if (invert)
        activations_unset.set(activation_option->second);
      else
        activations_set.set(activation_option->second);
      // Rules with activation types don't create regular filtering rules by
      // default. Don't add types.
      add_implicit_types = false;
      continue;
    }

    auto other_option = kOptionMap.find(option_name);
    if (other_option == kOptionMap.end())
      return kUnsupported;

    OptionDefinition option_definition = other_option->second;
    if (!option_definition.allow_invert && invert) {
      return kError;
    }
    if (option_definition.invert) {
      invert = !invert;
    }

    if (option_definition.value == OptionDefinition::kForbidden &&
        option_value) {
      return kError;
    }
    if (option_definition.value == OptionDefinition::kRequired &&
        !option_value) {
      return kError;
    }
    if (option_definition.value == OptionDefinition::kRequiredForModify &&
        rule.decision != RequestFilterRule::kPass && !option_value) {
      return kError;
    }

    OptionType option_type = option_definition.type;

    switch (option_type) {
      case OptionType::kAll:
        add_implicit_types = false;
        types_set.set();
        explicit_types_set.set();
        break;

      case OptionType::kDocument:
        add_implicit_types = false;
        if (option_value) {
          return kError;
        }

        if (invert)
          explicit_types_unset.set(RequestFilterRule::kDocument);
        else
          explicit_types_set.set(RequestFilterRule::kDocument);
        // Block rules are irrelevant for the document activation, since a
        // blocked document doesn't load any resource by definition.
        if (source_settings_.use_whole_document_allow &&
            rule.decision == RequestFilterRule::kPass) {
          if (invert)
            activations_unset.set(RequestFilterRule::kWholeDocument);
          else
            activations_set.set(RequestFilterRule::kWholeDocument);
          break;
        }
        break;

      case OptionType::kThirdParty:
        rule.party.set(invert ? RequestFilterRule::kFirstParty
                              : RequestFilterRule::kThirdParty);
        break;

      case OptionType::kImportant:
        if (rule.decision == RequestFilterRule::kPass) {
          return kError;
        }
        rule.decision = RequestFilterRule::kModifyImportant;
        break;

      case OptionType::kMatchCase:
        rule.is_case_sensitive = true;
        break;

      case OptionType::kDomain:
        if (!ParseDomains(*option_value, "|", rule.included_domains,
                          rule.excluded_domains))
          return Result::kError;
        break;

      case OptionType::kRewrite:
        CHECK(option_value);
        if (!base::StartsWith(*option_value, kRewritePrefix))
          return kError;
        option_value->remove_prefix(std::size(kRewritePrefix) - 1);
        if (!SetModifier(rule, RequestFilterRule::kRedirect, option_value)) {
          return kError;
        }
        break;

      case OptionType::kRedirectRule:
        rule.modify_block = false;
        [[fallthrough]];
      case OptionType::kRedirect:
        if (!option_value) {
          CHECK(rule.decision == RequestFilterRule::kPass);
          // uBlock makes all redirect allow rules affect only redirect.
          rule.modify_block = false;
        }
        if (!SetModifier(rule, RequestFilterRule::kRedirect, option_value)) {
          return kError;
        }
        break;

      case OptionType::kCSP:
        // CSP rules don't create regular filtering rules by default. Don't add
        // types
        add_implicit_types = false;
        if (option_value) {
          for (auto csp :
               base::SplitStringPiece(*option_value, ";", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY)) {
            if (base::StartsWith(csp, "report"))
              return kError;
          }
        }
        if (!SetModifier(rule, RequestFilterRule::kCsp, option_value)) {
          return kError;
        }
        break;

      case OptionType::kHost: {
        if (rule.host)
          return kError;

        if (option_value->find_first_of("/?") != std::string_view::npos)
          return kError;
        std::string host_str(*option_value);

        // This should result in a valid URL with only a host part.
        GURL validation_url(std::string("https://") + host_str);
        if (!validation_url.is_valid() || validation_url.has_port() ||
            validation_url.has_username())
          return kError;

        rule.host = host_str;
        break;
      }

      case OptionType::kAdQueryTrigger: {
        if (!source_settings_.allow_attribution_tracker_rules) {
          return kUnsupported;
        }
        add_implicit_types = false;
        rule.modify_block = false;

        CHECK(option_value);

        std::vector<std::string> params =
            base::SplitString(*option_value, "|", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY);

        if (!SetModifier(
                rule, RequestFilterRule::kAdQueryTrigger,
                std::set<std::string>(std::make_move_iterator(params.begin()),
                                      std::make_move_iterator(params.end())))) {
          return kError;
        }
        break;
      }

      case OptionType::kAdAttributionTracker: {
        if (!source_settings_.allow_attribution_tracker_rules) {
          return kUnsupported;
        }

        if (rule.decision != RequestFilterRule::kPass) {
          return kError;
        }

        CHECK(option_value);

        base::StringPairs domain_and_query_trigger;
        if (!base::SplitStringIntoKeyValuePairs(*option_value, '/', '|',
                                                &domain_and_query_trigger)) {
          return kError;
        }
        for (const auto& [domain, query_trigger] : domain_and_query_trigger) {
          std::optional<GURL> url_for_domain = GetUrlFromDomainString(domain);
          if (!url_for_domain) {
            return kError;
          }

          rule.ad_domains_and_query_triggers.insert(url_for_domain->host() +
                                                    "|" + query_trigger);
        }
        break;
      }

      default:
        // Was already handled
        NOTREACHED();
    }
  }

  // Enabling WebSocket explicitly for redirect rules is an error, because we
  // cannot redirect WebSocket requests. We allow it to be turned on implicity
  // further down however, because having the bit set on won't have any
  // effect.
  if (rule.modifier == RequestFilterRule::kRedirect &&
      (rule.resource_types.test(RequestFilterRule::kWebSocket))) {
    return kError;
  }

  rule.activation_types = activations_set & ~activations_unset;
  rule.explicit_types = explicit_types_set & ~explicit_types_unset;

  if (rule.activation_types.test(RequestFilterRule::kAttributeAds) &&
      !source_settings_.allow_attribution_tracker_rules) {
    return kUnsupported;
  }

  if (types_unset.any()) {
    rule.resource_types = ~types_unset | types_set;
  } else if (types_set.any()) {
    rule.resource_types = types_set;
  }
  if (add_implicit_types) {
    CHECK(rule.resource_types.none());
    rule.resource_types.set();
  }

  if (rule.modifier == RequestFilterRule::kAdQueryTrigger) {
    if (rule.explicit_types.any() || rule.resource_types.any() ||
        rule.activation_types.any()) {
      return kError;
    }

    rule.explicit_types.set(RequestFilterRule::kDocument);
    rule.modify_block = false;
  }

  if (rule.resource_types.none() && rule.explicit_types.none() &&
      rule.activation_types.none() &&
      rule.modifier != RequestFilterRule::kCsp) {
    // This rule wouldn't match anything.
    return kError;
  }

  if (rule.resource_types.none() && rule.explicit_types.none()) {
    if (rule.modifier == RequestFilterRule::kRedirect) {
      return kError;
    }
    rule.modify_block = false;
  }

  if (rule.party.none())
    rule.party.set();

  return kRequestFilterRule;
}

bool RuleParser::MaybeParseMetadata(std::string_view comment) {
  std::string_view metadata;
  if (GetMetadata(comment, kTitleTag, &metadata)) {
    parse_result_->metadata.title = std::string(metadata);
  } else if (GetMetadata(comment, kHomepageTag, &metadata)) {
    parse_result_->metadata.homepage = GURL(metadata);
  } else if (GetMetadata(comment, kRedirectTag, &metadata)) {
    parse_result_->metadata.redirect = GURL(metadata);
  } else if (GetMetadata(comment, kLicenseTag, &metadata)) {
    parse_result_->metadata.license = GURL(metadata);
  } else if (GetMetadata(comment, kExpiresTag, &metadata)) {
    auto expire_data = base::SplitStringPiece(metadata, base::kWhitespaceASCII,
                                              base::TRIM_WHITESPACE,
                                              base::SPLIT_WANT_NONEMPTY);
    if (expire_data.size() < 2)
      return false;
    int count;
    if (!base::StringToInt(expire_data[0], &count))
      return false;

    if (expire_data[1].compare("days") == 0) {
      parse_result_->metadata.expires = base::Days(count);
    } else if (expire_data[1].compare("hours") == 0) {
      parse_result_->metadata.expires = base::Hours(count);
    } else {
      return false;
    }
  } else if (GetMetadata(comment, kVersionTag, &metadata)) {
    int64_t version;
    if (!base::StringToInt64(metadata, &version))
      return false;
    parse_result_->metadata.version = version;
  } else {
    return false;
  }

  return true;
}

bool RuleParser::ParseDomains(std::string_view domain_string,
                              std::string separator,
                              std::set<std::string>& included_domains,
                              std::set<std::string>& excluded_domains) {
  for (auto domain :
       base::SplitStringPiece(domain_string, separator, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    bool excluded = domain[0] == '~';
    if (excluded)
      domain.remove_prefix(1);
    std::optional<GURL> url_for_domain = GetUrlFromDomainString(domain);

    if (!url_for_domain) {
      return false;
    }

    if (excluded)
      excluded_domains.insert(url_for_domain->host());
    else
      included_domains.insert(url_for_domain->host());
  }
  return true;
}

bool RuleParser::SetModifier(RequestFilterRule& rule,
                             RequestFilterRule::ModifierType type,
                             std::optional<std::string_view> value) {
  if (value) {
    return SetModifier(rule, type, std::set<std::string>{std::string(*value)});
  } else {
    return SetModifier(rule, type, std::set<std::string>());
  }
}

bool RuleParser::SetModifier(RequestFilterRule& rule,
                             RequestFilterRule::ModifierType type,
                             std::set<std::string> value) {
  CHECK(type != RequestFilterRule::kNoModifier);
  if (rule.modifier != RequestFilterRule::kNoModifier) {
    return false;
  }

  CHECK(!value.empty() || rule.decision == RequestFilterRule::kPass);

  rule.modifier = type;
  rule.modifier_values = std::move(value);
  return true;
}
}  // namespace adblock_filter
