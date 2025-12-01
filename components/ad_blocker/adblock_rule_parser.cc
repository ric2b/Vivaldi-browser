// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_rule_parser.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/char_iterator.h"
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
#include "third_party/re2/src/re2/re2.h"

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
  kBadFilter,
  kAll,
  kThirdParty,
  kStrictThirdParty,
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
  kAdAttributionTracker,
};

struct OptionDefinition {
  OptionType type;
  bool invert = false;
  bool allow_invert = false;
  enum { kRequired, kRequiredForModify, kForbidden } value = kForbidden;
};

constexpr auto kOptionMap = base::MakeFixedFlatMap<std::string_view,
                                                   OptionDefinition>(
    {{"badfilter", {.type = OptionType::kBadFilter}},
     {"all", {.type = OptionType::kAll}},
     {"third-party", {.type = OptionType::kThirdParty, .allow_invert = true}},
     {"3p", {.type = OptionType::kThirdParty, .allow_invert = true}},
     {"first-party",
      {.type = OptionType::kThirdParty, .invert = true, .allow_invert = true}},
     {"1p",
      {.type = OptionType::kThirdParty, .invert = true, .allow_invert = true}},
     {"strict3p",
      {.type = OptionType::kStrictThirdParty, .allow_invert = false}},
     {"strict1p",
      {.type = OptionType::kStrictThirdParty,
       .invert = true,
       .allow_invert = false}},
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

/**
 * Returns nullopt if the rule is unsupported.
 */
std::optional<size_t> GetOptionsStart(std::string_view rule_string) {
  size_t options_start = rule_string.rfind('$');

  if (options_start == rule_string.length() - 1) {
    // If the '$' character is the last character of the rule, we assume it's
    // part of the pattern instead of being the option section delimiter.
    return std::string_view::npos;
  }

  while (options_start != std::string::npos) {
    std::optional<char> before;
    if (options_start > 0) {
      before = rule_string[options_start - 1];
    }
    // Safe since we returned above if option_start was the last char of the
    // string.
    char after = rule_string[options_start + 1];

    if (before && before == '$') {
      return std::nullopt;  // adguard html filtering rule
    }

    // Prevent a '$' to be interpreted as option start when it comes before or
    // after certain delimiters. This mainly include usage at the end of a
    // regex, but it may also be used as a part of some option definitions in
    // ublock rules.
    if (after != '/' && after != '|' && after != ')' &&
        (!before || (*before != '"' && *before != '\'' && before != '\\' &&
                     before != '`')))
      return options_start;

    if (!before)
      return std::string_view::npos;

    options_start = rule_string.rfind('$', options_start - 1);
  }

  return options_start;
}

struct ParsedOption {
  bool invert = false;
  std::string_view name;
  std::optional<std::string> value;
};

enum class OptionParseResult {
  kSuccess = 0,
  kTryNextComma,
  kInvalid,
};

/**
 * Note that |result| is returned in reversed order
 */
OptionParseResult ParseRequestFilterRuleOptionRecursive(
    std::string_view options,
    std::vector<ParsedOption>* result) {
  ParsedOption rule_option;

  if (options.empty()) {
    return OptionParseResult::kTryNextComma;
  }

  // no-op option
  if (options.starts_with("_")) {
    std::size_t next = options.find_first_not_of('_');
    if (next == std::string_view::npos) {
      return OptionParseResult::kSuccess;
    }

    if (options[next] != ',') {
      return OptionParseResult::kTryNextComma;
    } else {
      options.remove_prefix(next + 1);
      if (ParseRequestFilterRuleOptionRecursive(options, result) !=
          OptionParseResult::kSuccess) {
        return OptionParseResult::kInvalid;
      }
    }
  }

  if (options.starts_with("~")) {
    rule_option.invert = true;
    options.remove_prefix(1);
  }

  size_t option_name_size = 0;
  while (option_name_size < options.length()) {
    char c = options[option_name_size];
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '-' &&
        c != '_') {
      break;
    }
    option_name_size++;
  }

  rule_option.name = options.substr(0, option_name_size);

  if (option_name_size == options.length()) {
    result->push_back(std::move(rule_option));
    return OptionParseResult::kSuccess;
  }

  if (option_name_size == 0) {
    return OptionParseResult::kTryNextComma;
  }

  options.remove_prefix(option_name_size);

  if (options.starts_with(",")) {
    options.remove_prefix(1);
    if (ParseRequestFilterRuleOptionRecursive(options, result) !=
        OptionParseResult::kSuccess) {
      return OptionParseResult::kInvalid;
    }
    result->push_back(std::move(rule_option));
    return OptionParseResult::kSuccess;
  }

  if (!options.starts_with("=")) {
    return OptionParseResult::kTryNextComma;
  }

  options.remove_prefix(1);

  if (options.empty()) {
    return OptionParseResult::kInvalid;
  }

  std::string_view options_backup = options;

  options = base::TrimWhitespaceASCII(options, base::TRIM_LEADING);

  char first_option_char = options.empty() ? ' ' : options[0];
  if (first_option_char == '"' || first_option_char == '\'' ||
      first_option_char == '`') {
    options.remove_prefix(1);
    size_t next_quote = options.find_first_of(first_option_char);
    if (next_quote == 0) {
      // Don't allow empty values.
      return OptionParseResult::kInvalid;
    }
    rule_option.value = "";

    while (next_quote != std::string_view::npos) {
      // Position of the first non-backslash before the quote.
      size_t backslashes = options.substr(0, next_quote).find_last_not_of('\\');

      size_t backslash_count = backslashes == std::string_view::npos
                                   ? 0
                                   : (next_quote - 1) - backslashes;

      // The quote is escaped.
      if ((backslash_count) % 2 != 0) {
        // Drop the backslash used for escaping.
        rule_option.value->append(options.substr(0, next_quote - 1));
        rule_option.value->push_back(first_option_char);
        options.remove_prefix(next_quote + 1);
        next_quote = options.find_first_of(first_option_char);
        continue;
      }

      rule_option.value->append(options.substr(0, next_quote));
      options.remove_prefix(next_quote + 1);
      options = base::TrimWhitespaceASCII(options, base::TRIM_LEADING);

      if (options.empty()) {
        result->push_back(std::move(rule_option));
        return OptionParseResult::kSuccess;
      }

      // If the next option doesn't start after the closing quote, then this
      // wasn't a quoted option.
      if (!options.starts_with(",")) {
        break;
      }

      options.remove_prefix(1);

      if (ParseRequestFilterRuleOptionRecursive(options, result) !=
          OptionParseResult::kSuccess) {
        return OptionParseResult::kInvalid;
      }

      result->push_back(std::move(rule_option));
      return OptionParseResult::kSuccess;
    }

    // Fall back to try reading to the next comma.
    options = options_backup;
  }

  size_t next_comma = 0;
  while (true) {
    next_comma = options.find_first_of(",", next_comma);
    rule_option.value = options.substr(0, next_comma);

    if (next_comma == std::string_view::npos) {
      result->push_back(std::move(rule_option));
      return OptionParseResult::kSuccess;
    }

    OptionParseResult next_option_parse_result =
        ParseRequestFilterRuleOptionRecursive(options.substr(next_comma + 1),
                                              result);
    if (next_option_parse_result == OptionParseResult::kTryNextComma) {
      next_comma++;
      continue;
    }

    if (next_comma == 0) {
      // Don't allow empty values.
      return OptionParseResult::kInvalid;
    }
    result->push_back(std::move(rule_option));
    return next_option_parse_result;
  }
}

bool ParseDomains(std::string_view domain_string,
                  std::string separator,
                  bool allow_exclusions,
                  std::set<std::string>& included_domains,
                  std::set<std::string>& excluded_domains) {
  for (auto domain :
       base::SplitStringPiece(domain_string, separator, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    bool excluded = domain[0] == '~';
    if (excluded) {
      if (!allow_exclusions) {
        return false;
      }
      domain.remove_prefix(1);
    }
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

bool SetModifier(RequestFilterRule& rule,
                 RequestFilterRule::ModifierType type,
                 std::set<std::string> value) {
  CHECK(type != RequestFilterRule::kNoModifier);
  if (rule.modifier != RequestFilterRule::kNoModifier) {
    return false;
  }

  // Only Pass rules can have an empty modifier value, which negates
  // all Modify rules for the given modifier.
  CHECK(!value.empty() || rule.decision == RequestFilterRule::kPass);

  rule.modifier = type;
  rule.modifier_values = std::move(value);
  return true;
}

bool SetModifier(RequestFilterRule& rule,
                 RequestFilterRule::ModifierType type,
                 std::optional<std::string_view> value) {
  if (value) {
    return SetModifier(rule, type, std::set<std::string>{std::string(*value)});
  } else {
    return SetModifier(rule, type, std::set<std::string>());
  }
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

  size_t maybe_selector_separator = rule_string.find('#');
  if (maybe_selector_separator != std::string_view::npos) {
    std::optional<Result> result =
        ParseContentInjectionRule(rule_string, maybe_selector_separator);
    if (result) {
      return *result;
    }

    if (maybe_selector_separator == 0) {
      // Line started with a #, but was not a content injection rule -> assume a
      // comment.
      return kComment;
    }

    size_t last_non_space =
        rule_string.find_last_not_of(' ', maybe_selector_separator - 1);
    // Remove inline comment
    if (last_non_space != maybe_selector_separator - 1) {
      CHECK(last_non_space < rule_string.length());
      rule_string.remove_suffix(rule_string.length() - (last_non_space + 1));
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

std::optional<RuleParser::Result> RuleParser::ParseContentInjectionRule(
    std::string_view rule_string,
    size_t first_separator) {
  size_t second_separator = rule_string.find('#', first_separator + 1);

  if (second_separator == std::string_view::npos) {
    return std::nullopt;
  }
  std::string_view body = rule_string.substr(second_separator + 1);

  size_t position = first_separator + 1;
  ContentInjectionRuleCore core;

  if (rule_string[position] == '@') {
    core.is_allow_rule = true;
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
    } else if (core.is_allow_rule) {
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
    // If we haven't reached the second separator at this point, this is not a
    // content injection rule.
    return std::nullopt;
  }

  if (!ParseDomains(rule_string.substr(0, first_separator), ",", true,
                    core.included_domains, core.excluded_domains))
    return Result::kError;
  if (result == Result::kScriptletInjectionRule &&
      core.included_domains.empty())
    return Result::kError;

  switch (result) {
    case Result::kCosmeticRule: {
      if (!ParseCosmeticRule(body, std::move(core)))
        result = Result::kError;
      break;
    }
    case Result::kScriptletInjectionRule: {
      if (!ParseScriptletInjectionRule(body, std::move(core)))
        result = Result::kError;
      break;
    }
    default:
      break;
  }

  return result;
}

bool RuleParser::ParseCosmeticRule(std::string_view body,
                                   ContentInjectionRuleCore rule_core) {
  // Rules should consist of a list of selectors. No actual CSS rules allowed.
  if (body.empty() || body.find('{') != std::string_view::npos ||
      body.find('}') != std::string_view::npos)
    return false;

  // The easylist uses has-text, even though this is not a valid selector and we
  // don't yet have an implementation for it in cosmetic rules.
  if (body.find(":has-text") != std::string_view::npos) {
    return false;
  }

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
  rule.original_rule_text = rule_string;
  if (base::StartsWith(rule_string, "@@")) {
    rule.decision = RequestFilterRule::kPass;
    rule_string.remove_prefix(2);
  }

  // The pattern part of regex rules starts and ends with '/'. Since
  // those rules can contain a '$' as an end-of-string marker, we only try to
  // find a '$' marking the beginning of the options section if the pattern
  // doesn't look like a whole-line regex.
  std::optional<size_t> options_start = GetOptionsStart(rule_string);

  if (!options_start) {
    return kUnsupported;
  }

  std::string_view options_string;
  if (*options_start != std::string_view::npos)
    options_string = rule_string.substr(*options_start);

  // Even if the options string is empty, there is some common setup code
  // that we want to run.
  bool can_strict_block = false;
  Result result =
      ParseRequestFilterRuleOptions(options_string, rule, can_strict_block);
  if (result != kRequestFilterRule)
    return result;

  std::string_view pattern = rule_string.substr(0, *options_start);

  if (base::StartsWith(pattern, "/") && base::EndsWith(pattern, "/") &&
      pattern.length() > 1) {
    pattern.remove_prefix(1);
    pattern.remove_suffix(1);
    // No need to compile this rule if we can't handle the pattern.
    if (!re2::RE2(pattern).ok()) {
      return kUnsupported;
    }
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
  // u-block also removes a single ^ separator preceded by a * wildcard here,
  // for optimization purposes, but we don't need that optimization and it seem
  // like it would lead pattern ends in the form ^*^* to be misinterpreted as
  // only the last ^ is expected to match the string end.
  while (base::EndsWith(pattern, "*")) {
    pattern.remove_suffix(1);
  }

  for (base::i18n::UTF8CharIterator iter(pattern); !iter.end();
       iter.Advance()) {
    int32_t code_point = iter.get();
    // Reject these characters in the pattern to match ublock
    if (base::IsAsciiWhitespace(code_point) ||
        base::IsUnicodeWhitespace(code_point) ||
        base::IsUnicodeControl(code_point) || code_point == 0x00AD ||
        code_point == 0x061C ||
        (code_point >= 0x200B && code_point <= 0x200F) ||
        code_point == 0xFEFF ||
        (code_point >= 0xFFF9 && code_point <= 0xFFFC)) {
      return kError;
    }
  }

  if (pattern.find_first_of(base::kWhitespaceASCII) != std::string_view::npos) {
    return kError;
  }

  if (pattern.size() <= 1 && *options_start == std::string_view::npos) {
    // A rule consisting of a single character and no option is likely a mistake
    return kError;
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

  // This would basically be a noop rule. Ignore it.
  if (rule.host && !rule.pattern.starts_with(*rule.host))
    return kError;

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
    maybe_pure_host = false;
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
      rule.host = validation_url.host();
      if (source_settings_.pure_host_is_document_block && can_strict_block) {
        rule.explicit_types.set(RequestFilterRule::kDocument);
      }
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

bool RuleParser::MaybeAddPureHostRule(std::string_view maybe_hostname,
                                      std::string_view original_rule_text) {
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
  rule.original_rule_text = original_rule_text;
  rule.anchor_type.set(RequestFilterRule::kAnchorHost);
  rule.host = maybe_hostname;
  rule.resource_types.set();
  rule.pattern_type = RequestFilterRule::kPlain;
  rule.pattern = maybe_hostname;
  rule.pattern.append("^");
  if (source_settings_.pure_host_is_document_block) {
    rule.explicit_types.set(RequestFilterRule::kDocument);
  }
  parse_result_->request_filter_rules.push_back(std::move(rule));

  return true;
}

std::optional<RuleParser::Result> RuleParser::ParseHostsFileOrNakedHost(
    std::string_view rule_string) {
  std::string_view original_rule_text = rule_string;
  size_t first_space = rule_string.find_first_of(" \t");
  if (first_space == std::string_view::npos) {
    if (source_settings_.naked_hostname_is_pure_host &&
        MaybeAddPureHostRule(rule_string, original_rule_text)) {
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
    result = MaybeAddPureHostRule(hostname, original_rule_text)
                 ? kRequestFilterRule
                 : result;
  }

  return result;
}

RuleParser::Result RuleParser::ParseRequestFilterRuleOptions(
    std::string_view options,
    RequestFilterRule& rule,
    bool& can_strict_block) {
  if (!options.empty()) {
    DCHECK_EQ('$', options[0]);
    options.remove_prefix(1);
  }

  std::vector<ParsedOption> parsed_options_reversed;
  if (!options.empty() &&
      ParseRequestFilterRuleOptionRecursive(
          options, &parsed_options_reversed) != OptionParseResult::kSuccess) {
    return kError;
  }

  bool add_implicit_types = true;

  enum class Party {
    kNone,
    kFirst,
    kThird,
  };

  bool first_party = false;
  bool third_party = false;
  Party strict_party = Party::kNone;

  std::bitset<RequestFilterRule::kTypeCount> types_set;
  std::bitset<RequestFilterRule::kTypeCount> types_unset;
  std::bitset<RequestFilterRule::kExplicitTypeCount> explicit_types_set;
  std::bitset<RequestFilterRule::kExplicitTypeCount> explicit_types_unset;
  std::bitset<RequestFilterRule::kActivationCount> activations_set;
  std::bitset<RequestFilterRule::kActivationCount> activations_unset;
  for (auto parsed_option : base::Reversed(parsed_options_reversed)) {
    auto type_option = kTypeStringMap.find(parsed_option.name);
    if (type_option != kTypeStringMap.end()) {
      if (parsed_option.value) {
        return kError;
      }
      if (parsed_option.invert)
        types_unset.set(type_option->second);
      else
        types_set.set(type_option->second);
      continue;
    }

    auto explicit_type_option = kExplicitTypeStringMap.find(parsed_option.name);
    if (explicit_type_option != kExplicitTypeStringMap.end()) {
      if (parsed_option.value) {
        return kError;
      }
      if (parsed_option.invert)
        explicit_types_unset.set(explicit_type_option->second);
      else
        explicit_types_set.set(explicit_type_option->second);
      // Only add implicit types if we haven't added any otherwise.
      add_implicit_types = false;
      continue;
    }

    auto activation_option = kActivationStringMap.find(parsed_option.name);
    if (activation_option != kActivationStringMap.end()) {
      if (parsed_option.value) {
        return kError;
      }
      if (parsed_option.invert)
        activations_unset.set(activation_option->second);
      else
        activations_set.set(activation_option->second);
      // Rules with activation types don't create regular filtering rules by
      // default. Don't add types.
      add_implicit_types = false;
      continue;
    }

    auto other_option = kOptionMap.find(parsed_option.name);
    if (other_option == kOptionMap.end())
      return kUnsupported;

    OptionDefinition option_definition = other_option->second;
    if (!option_definition.allow_invert && parsed_option.invert) {
      return kError;
    }

    if (option_definition.invert) {
      parsed_option.invert = !parsed_option.invert;
    }

    if (option_definition.value == OptionDefinition::kForbidden &&
        parsed_option.value) {
      return kError;
    }
    if (option_definition.value == OptionDefinition::kRequired &&
        !parsed_option.value) {
      return kError;
    }
    if (option_definition.value == OptionDefinition::kRequiredForModify &&
        rule.decision != RequestFilterRule::kPass && !parsed_option.value) {
      return kError;
    }

    OptionType option_type = option_definition.type;

    switch (option_type) {
      case OptionType::kBadFilter:
        rule.bad_filter = true;
        break;
      case OptionType::kAll:
        add_implicit_types = false;
        types_set.set();
        explicit_types_set.set();
        break;

      case OptionType::kDocument:
        add_implicit_types = false;
        if (parsed_option.value) {
          return kError;
        }

        if (parsed_option.invert)
          explicit_types_unset.set(RequestFilterRule::kDocument);
        else
          explicit_types_set.set(RequestFilterRule::kDocument);
        // Block rules are irrelevant for the document activation, since a
        // blocked document doesn't load any resource by definition.
        if (source_settings_.use_whole_document_allow &&
            rule.decision == RequestFilterRule::kPass) {
          if (parsed_option.invert)
            activations_unset.set(RequestFilterRule::kWholeDocument);
          else
            activations_set.set(RequestFilterRule::kWholeDocument);
          break;
        }
        break;

      case OptionType::kThirdParty:
        if (parsed_option.invert) {
          first_party = true;
        } else {
          third_party = true;
        }
        break;

      case OptionType::kStrictThirdParty:
        if (parsed_option.invert) {
          strict_party =
              (strict_party == Party::kThird) ? Party::kNone : Party::kFirst;
        } else {
          strict_party =
              (strict_party == Party::kFirst) ? Party::kNone : Party::kThird;
        }
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
        if (!ParseDomains(*parsed_option.value, "|", true,
                          rule.included_domains, rule.excluded_domains))
          return Result::kError;
        break;

      case OptionType::kRewrite:
        CHECK(parsed_option.value);
        if (!base::StartsWith(*parsed_option.value, kRewritePrefix))
          return kError;
        parsed_option.value =
            parsed_option.value->substr(std::size(kRewritePrefix) - 1);
        if (!SetModifier(rule, RequestFilterRule::kRedirect,
                         parsed_option.value)) {
          return kError;
        }
        break;

      case OptionType::kRedirectRule:
        rule.modify_block = false;
        [[fallthrough]];
      case OptionType::kRedirect:
        if (!parsed_option.value) {
          CHECK(rule.decision == RequestFilterRule::kPass);
          // uBlock makes all redirect allow rules affect only redirect.
          rule.modify_block = false;
        }
        if (!SetModifier(rule, RequestFilterRule::kRedirect,
                         parsed_option.value)) {
          return kError;
        }
        break;

      case OptionType::kCSP:
        // CSP rules don't create regular filtering rules by default. Don't add
        // types
        add_implicit_types = false;
        if (parsed_option.value) {
          for (auto csp : base::SplitStringPiece(*parsed_option.value, ";",
                                                 base::TRIM_WHITESPACE,
                                                 base::SPLIT_WANT_NONEMPTY)) {
            if (base::StartsWith(csp, "report"))
              return kError;
          }
        }
        if (!SetModifier(rule, RequestFilterRule::kCsp, parsed_option.value)) {
          return kError;
        }
        break;

      case OptionType::kHost: {
        if (rule.host)
          return kError;

        if (parsed_option.value->find_first_of("/?") != std::string_view::npos)
          return kError;
        std::string host_str(*parsed_option.value);

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

        CHECK(parsed_option.value);

        std::vector<std::string> params =
            base::SplitString(*parsed_option.value, "|", base::KEEP_WHITESPACE,
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

        CHECK(parsed_option.value);

        base::StringPairs domain_and_query_trigger;
        if (!base::SplitStringIntoKeyValuePairs(*parsed_option.value, '/', '|',
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

  // Enabling WebSocket explicitly for redirect rules is an unsupported, because
  // we cannot redirect WebSocket requests. We allow it to be turned on
  // implicity further down however, because having the bit set on won't have
  // any effect.
  if (rule.modifier == RequestFilterRule::kRedirect &&
      (rule.resource_types.test(RequestFilterRule::kWebSocket))) {
    return kUnsupported;
  }

  rule.activation_types = activations_set & ~activations_unset;
  rule.explicit_types = explicit_types_set & ~explicit_types_unset;

  if (rule.activation_types.test(RequestFilterRule::kAttributeAds) &&
      !source_settings_.allow_attribution_tracker_rules) {
    return kUnsupported;
  }

  can_strict_block =
      types_set.all() || (add_implicit_types && types_set.none());

  if (types_unset.any()) {
    rule.resource_types = ~types_unset | types_set;
  } else if (types_set.any()) {
    rule.resource_types = types_set;
  } else if (add_implicit_types) {
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

  switch (strict_party) {
    case Party::kNone:
      if (first_party && !third_party) {
        rule.party = RequestFilterRule::kFirstParty;
      } else if (third_party && !first_party) {
        rule.party = RequestFilterRule::kThirdParty;
      }
      break;

    case Party::kFirst:
      if (third_party && !first_party) {
        // This rule wouldn't match anything
        return kError;
      }
      rule.party = RequestFilterRule::kStrictFirstParty;
      break;

    case Party::kThird:
      rule.party = RequestFilterRule::kStrictThirdParty;
      if (first_party && !third_party) {
        rule.party = RequestFilterRule::kFirstPartyAndStrictThirdParty;
      }
      break;
  }

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
}  // namespace adblock_filter
