// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/adblock_rule_parser.h"

#include <map>
#include <string>
#include <utility>

#include "base/containers/fixed_flat_map.h"
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
  kThirdParty,
  kMatchCase,
  kDomain,
  kCSP,
  kHost,  // Vivaldi-specific, allows us to handle DDG filter.
  kRewrite,
  kRedirect,
};

constexpr auto kOptionMap =
    base::MakeFixedFlatMap<base::StringPiece, OptionType>(
        {{"third-party", OptionType::kThirdParty},
         {"match-case", OptionType::kMatchCase},
         {"domain", OptionType::kDomain},
         {"host", OptionType::kHost},
         {"csp", OptionType::kCSP},
         {"rewrite", OptionType::kRewrite},
         {"redirect", OptionType::kRedirect}});

struct ActivationTypeDetails {
  int type;
  bool allow_only;
};

constexpr auto kActivationStringMap =
    base::MakeFixedFlatMap<base::StringPiece, ActivationTypeDetails>(
        {{"popup", {RequestFilterRule::kPopup, false}},
         {"document", {RequestFilterRule::kDocument, false}},
         {"elemhide", {RequestFilterRule::kElementHide, true}},
         {"generichide", {RequestFilterRule::kGenericHide, true}},
         {"genericblock", {RequestFilterRule::kGenericBlock, true}}});

bool GetMetadata(base::StringPiece comment,
                 const std::string& tag_name,
                 base::StringPiece* result) {
  if (!base::StartsWith(comment, tag_name))
    return false;

  *result = base::TrimWhitespaceASCII(comment.substr(tag_name.length()),
                                      base::TRIM_LEADING);
  return true;
}
}  // namespace

RuleParser::RuleParser(ParseResult* parse_result, bool allow_abp_snippets)
    : parse_result_(parse_result), allow_abp_snippets_(allow_abp_snippets) {}
RuleParser::~RuleParser() = default;

RuleParser::Result RuleParser::Parse(base::StringPiece rule_string) {
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

  if (rule_string[0] == '[') {
    return kComment;
  }

  if (rule_string[0] == '!') {
    rule_string.remove_prefix(1);

    if (MaybeParseMetadata(
            base::TrimWhitespaceASCII(rule_string, base::TRIM_LEADING)))
      return kMetadata;
    return kComment;
  }

  size_t selector_separator_2 = base::StringPiece::npos;
  size_t maybe_selector_separator = rule_string.find('#');
  if (maybe_selector_separator != base::StringPiece::npos) {
    selector_separator_2 = rule_string.find('#', maybe_selector_separator + 1);
  }

  if (selector_separator_2 != base::StringPiece::npos) {
    ContentInjectionRuleCore content_injection_rule_core;
    base::StringPiece body = rule_string.substr(selector_separator_2 + 1);
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

  RequestFilterRule rule;
  Result result = ParseRequestFilterRule(rule_string, &rule);
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
    base::StringPiece rule_string,
    size_t separator,
    ContentInjectionRuleCore* core) {
  // This assumes we have another '#' separator to look forward to.Under that
  // assumption, the following parsing code is safe until it encounters the
  // second separator.
  DCHECK(rule_string.find('#', separator + 1) != base::StringPiece::npos);

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
    if (!allow_abp_snippets_) {
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
      if (allow_abp_snippets_) {
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
                    &core->included_domains, &core->excluded_domains))
    return Result::kError;
  if (result == Result::kScriptletInjectionRule &&
      core->included_domains.empty())
    return Result::kError;

  return result;
}

bool RuleParser::ParseCosmeticRule(base::StringPiece body,
                                   ContentInjectionRuleCore rule_core) {
  // Rules should consist of a list of selectors. No actual CSS rules allowed.
  if (body.empty() || body.find('{') != base::StringPiece::npos ||
      body.find('}') != base::StringPiece::npos)
    return false;

  CosmeticRule rule;
  rule.selector = std::string(body);
  rule.core = std::move(rule_core);
  parse_result_->cosmetic_rules.push_back(std::move(rule));
  return true;
}

bool RuleParser::ParseScriptletInjectionRule(
    base::StringPiece body,
    ContentInjectionRuleCore rule_core) {
  ScriptletInjectionRule rule;
  rule.core = std::move(rule_core);
  // Use this name to signal an abp snippet filter.
  rule.scriptlet_name = kAbpSnippetsScriptletName;
  base::Value arguments_list(base::Value::Type::LIST);

  for (base::StringPiece injection : base::SplitStringPiece(
           body, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    bool escaped = false;
    bool in_quotes = false;
    bool after_quotes = false;
    bool parsing_code_point = false;
    std::string code_point_str;
    base::Value arguments(base::Value::Type::LIST);
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
        arguments.GetList().Append(std::move(argument));
      }

      if (c != '\'')
        after_quotes = false;
    }

    if (!argument.empty() || after_quotes) {
      arguments.GetList().Append(std::move(argument));
    }

    // Can happen if we have an argument string containing only a '\\' or a '\''
    if (arguments.GetList().size() == 0)
      continue;

    arguments_list.GetList().Append(std::move(arguments));
  }

  std::string serialized_arguments;
  JSONStringValueSerializer(&serialized_arguments).Serialize(arguments_list);
  rule.arguments.push_back(std::move(serialized_arguments));
  parse_result_->scriptlet_injection_rules.push_back(std::move(rule));
  return true;
}

RuleParser::Result RuleParser::ParseRequestFilterRule(
    base::StringPiece rule_string,
    RequestFilterRule* rule) {
  // TODO(julien): Add optional support for plain hostnames

  if (base::StartsWith(rule_string, "@@")) {
    rule->is_allow_rule = true;
    rule_string.remove_prefix(2);
  }

  // The pattern part of regex rules starts and ends with '/'. Since
  // those rules can contain a '$' as an end-of-string marker, we only try to
  // find a '$' marking the beginning of the options section if the pattern
  // doesn't look like a whole-line regex
  size_t options_start = base::StringPiece::npos;
  if (rule_string[0] != '/' || rule_string.back() != '/') {
    options_start = rule_string.rfind('$');
  }
  if (options_start != base::StringPiece::npos && options_start != 0 &&
      rule_string[options_start - 1] == '$') {
    // AdGuard HTML filtering rules use $$ as separator
    return kUnsupported;
  }

  base::StringPiece options_string;
  if (options_start != base::StringPiece::npos)
    options_string = rule_string.substr(options_start);

  // Even if the options string is empty, there is some common setup code
  // that we want to run.
  Result result = ParseRequestFilterRuleOptions(options_string, rule);
  if (result != kRequestFilterRule)
    return result;

  base::StringPiece pattern = rule_string.substr(0, options_start);

  if (base::StartsWith(pattern, "/") && base::EndsWith(pattern, "/") &&
      pattern.length() > 1) {
    pattern.remove_prefix(1);
    pattern.remove_suffix(1);
    rule->pattern_type = RequestFilterRule::kRegex;
    rule->pattern = std::string(pattern);
    rule->ngram_search_string = BuildNgramSearchString(pattern);
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
    rule->anchor_type.set(RequestFilterRule::kAnchorHost);
  } else if (base::StartsWith(pattern, "|")) {
    rule->anchor_type.set(RequestFilterRule::kAnchorStart);
    pattern.remove_prefix(1);
  }

  if (base::StartsWith(pattern, "*")) {
    // Starting with a wildcard makes anchoring at the start meaningless
    pattern.remove_prefix(1);
    rule->anchor_type.reset(RequestFilterRule::kAnchorHost);
    rule->anchor_type.reset(RequestFilterRule::kAnchorStart);

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
    rule->anchor_type.set(RequestFilterRule::kAnchorEnd);
  }

  // We had a pattern of the form "|*|", which is equivalent to "*"
  if (pattern.empty()) {
    rule->anchor_type.reset(RequestFilterRule::kAnchorEnd);
  }

  if (base::EndsWith(pattern, "*")) {
    // Ending with a wildcard makes anchoring at the end meaningless
    pattern.remove_suffix(1);
    rule->anchor_type.reset(RequestFilterRule::kAnchorEnd);
    maybe_pure_host = false;
  }

  // Stars at the end don't contribute to the pattern
  while (base::EndsWith(pattern, "*")) {
    pattern.remove_suffix(1);
  }

  if (pattern.find_first_of("*") != base::StringPiece::npos) {
    rule->pattern_type = RequestFilterRule::kWildcarded;
  }

  if (!process_hostname) {
    if (!rule->is_case_sensitive) {
      rule->pattern =
          base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(pattern)));
    } else {
      rule->ngram_search_string =
          base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(pattern)));
      rule->pattern = std::string(pattern);
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
  if (authority_end == base::StringPiece::npos) {
    authority_length = base::StringPiece::npos;
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
      if (!rule->host.empty())
        return kError;
      rule->host = validation_url.host();
      // TODO(julien): Match host-specific rules using a trie, so that
      // pure host rules don't require extra pattern-matching.
      // return kRequestFilterRule;
    }
    canonicalized_pattern += validation_url.host();
    if (validation_url.has_port()) {
      canonicalized_pattern += ":" + validation_url.port();
    }
  } else {
    canonicalized_pattern += potential_authority;
  }

  if (authority_end != base::StringPiece::npos) {
    canonicalized_pattern += std::string(pattern.substr(authority_end));
  }

  if (!rule->is_case_sensitive) {
    rule->pattern = base::UTF16ToUTF8(
        base::i18n::FoldCase(base::UTF8ToUTF16(canonicalized_pattern)));
  } else {
    rule->pattern = canonicalized_pattern;
    rule->ngram_search_string =
        base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(pattern)));
  }

  return kRequestFilterRule;
}

RuleParser::Result RuleParser::ParseRequestFilterRuleOptions(
    base::StringPiece options,
    RequestFilterRule* rule) {
  if (!options.empty()) {
    DCHECK_EQ('$', options[0]);
    options.remove_prefix(1);
  }

  std::bitset<RequestFilterRule::kTypeCount> types_set;
  std::bitset<RequestFilterRule::kTypeCount> types_unset;
  std::bitset<RequestFilterRule::kActivationCount> activations_set;
  std::bitset<RequestFilterRule::kActivationCount> activations_unset;
  for (base::StringPiece option : base::SplitStringPiece(
           options, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    bool invert = false;
    if (base::StartsWith(option, "~", base::CompareCase::SENSITIVE)) {
      option.remove_prefix(1);
      invert = true;
    }

    size_t option_name_end = option.find('=');
    base::StringPiece option_name = option.substr(0, option_name_end);

    auto* type_option = kTypeStringMap.find(option_name);
    if (type_option != kTypeStringMap.end()) {
      if (invert)
        types_unset.set(type_option->second);
      else
        types_set.set(type_option->second);
      continue;
    }

    auto* activation_option = kActivationStringMap.find(option_name);
    if (activation_option != kActivationStringMap.end()) {
      if (activation_option->second.allow_only && !rule->is_allow_rule)
        return kError;
      if (invert)
        activations_unset.set(activation_option->second.type);
      else
        activations_set.set(activation_option->second.type);
      continue;
    }

    auto* other_option = kOptionMap.find(option_name);
    if (other_option == kOptionMap.end())
      return kUnsupported;

    OptionType option_type = other_option->second;

    if (option_type == OptionType::kThirdParty) {
      rule->party.set(invert ? RequestFilterRule::kFirstParty
                             : RequestFilterRule::kThirdParty);
      continue;
    }

    if (invert) {
      return kError;
    }

    if (option_type == OptionType::kMatchCase) {
      rule->is_case_sensitive = true;
      continue;
    }

    if (option_type == OptionType::kCSP)
      rule->is_csp_rule = true;

    if (option_name_end == base::StringPiece::npos) {
      if (rule->is_csp_rule && rule->is_allow_rule) {
        // Allow CSP rules without a value for CSP are legal.
        continue;
      }
      return kError;
    }

    base::StringPiece option_value = option.substr(option_name_end + 1);

    switch (option_type) {
      case OptionType::kDomain:
        if (!ParseDomains(option_value, "|", &rule->included_domains,
                          &rule->excluded_domains))
          return Result::kError;
        break;

      case OptionType::kRewrite:
        if (!rule->redirect.empty())
          return kError;
        if (!base::StartsWith(option_value, kRewritePrefix))
          return kError;
        option_value.remove_prefix(std::size(kRewritePrefix) - 1);
        rule->redirect = std::string(option_value);
        break;

      case OptionType::kRedirect:
        if (!rule->redirect.empty())
          return kError;
        rule->redirect = std::string(option_value);
        break;

      case OptionType::kCSP:
        for (auto csp :
             base::SplitStringPiece(option_value, ";", base::TRIM_WHITESPACE,
                                    base::SPLIT_WANT_NONEMPTY)) {
          if (base::StartsWith(csp, "base-uri") ||
              base::StartsWith(csp, "referrer") ||
              base::StartsWith(csp, "report") ||
              base::StartsWith(csp, "upgrade-insecure-requests"))
            return kError;
        }
        rule->csp = std::string(option_value);
        break;

      case OptionType::kHost: {
        if (!rule->host.empty())
          return kError;

        if (option_value.find_first_of("/?") != base::StringPiece::npos)
          return kError;
        std::string host_str(option_value);

        // This should result in a valid URL with only a host part.
        GURL validation_url(std::string("https://") + host_str);
        if (!validation_url.is_valid() || validation_url.has_port() ||
            validation_url.has_username())
          return kError;

        rule->host = host_str;
        break;
      }

      default:
        // Was already handled
        NOTREACHED();
        break;
    }
  }

  rule->activation_types = activations_set & ~activations_unset;
  if (types_unset.any()) {
    rule->resource_types = ~types_unset | types_set;
  } else if (types_set.any()) {
    rule->resource_types = types_set;
  } else if (activations_set.none() && activations_unset.none() &&
             !rule->is_csp_rule) {
    // Rules with activation types and csp rules don't create regular
    // filtering rules by default. Any other rules without a resource type
    // should match all resources.
    rule->resource_types.set();
  }

  if (rule->resource_types.none() && rule->activation_types.none() &&
      !rule->is_csp_rule) {
    // This rule wouldn't match anything.
    return kError;
  }

  if (rule->party.none())
    rule->party.set();

  return kRequestFilterRule;
}

bool RuleParser::MaybeParseMetadata(base::StringPiece comment) {
  base::StringPiece metadata;
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

bool RuleParser::ParseDomains(base::StringPiece domain_string,
                              std::string separator,
                              std::vector<std::string>* included_domains,
                              std::vector<std::string>* excluded_domains) {
  for (auto domain :
       base::SplitStringPiece(domain_string, separator, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    bool excluded = domain[0] == '~';
    if (excluded)
      domain.remove_prefix(1);
    std::string domain_str(domain);

    if (domain.find_first_of("/?") != base::StringPiece::npos)
      return false;

    // This should result in a valid URL with only a host part.
    GURL validation_url(std::string("https://") + domain_str);
    if (!validation_url.is_valid() || validation_url.has_port() ||
        validation_url.has_username())
      return false;

    if (excluded)
      excluded_domains->push_back(validation_url.host());
    else
      included_domains->push_back(validation_url.host());
  }
  return true;
}
}  // namespace adblock_filter
