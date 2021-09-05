// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rule_parser.h"

#include <map>
#include <string>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/request_filter/adblock_filter/parse_utils.h"

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

const std::map<base::StringPiece, OptionType> kOptionMap{
    {"third-party", OptionType::kThirdParty},
    {"match-case", OptionType::kMatchCase},
    {"domain", OptionType::kDomain},
    {"host", OptionType::kHost},
    {"csp", OptionType::kCSP},
    {"rewrite", OptionType::kRewrite},
    {"redirect", OptionType::kRedirect}};

struct ActivationTypeDetails {
  int type;
  bool allow_only;
};

const std::map<base::StringPiece, ActivationTypeDetails> kActivationStringMap{
    {"popup", {FilterRule::kPopup, false}},
    {"document", {FilterRule::kDocument, false}},
    {"elemhide", {FilterRule::kElementHide, true}},
    {"generichide", {FilterRule::kGenericHide, true}},
    {"genericblock", {FilterRule::kGenericBlock, true}}};

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

RuleParser::RuleParser(ParseResult* parse_result)
    : parse_result_(parse_result) {}
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

  size_t maybe_selector_separator = rule_string.find('#');
  if (maybe_selector_separator != base::StringPiece::npos) {
    CosmeticRule rule;
    Result result =
        MaybeParseCosmeticRule(rule_string, maybe_selector_separator, &rule);
    switch (result) {
      case Result::kCosmeticRule:
        parse_result_->cosmetic_rules.push_back(std::move(rule));
        return result;
      case Result::kFilterRule:
        break;
      default:
        return result;
    }
  }

  FilterRule rule;
  Result result = ParseFilterRule(rule_string, &rule);
  if (result != kFilterRule)
    return result;

  parse_result_->filter_rules.push_back(std::move(rule));
  return result;
}

// Element hiding rules: hostname##element
// Element hiding allow rules: hostname#@#element
// Some blockers use special rules using one of those formats
// - hostname#?#element
// - hostname#$#element
// - hostname#%#element
// or a combination of @ and those symbols.
// Those are meant to handle custom selectors or additional functionality from
// those blockers. We don't support any of those custom selectors or
// functionalities for the time being.

// We return kFilterRule to indicate that this is not after all a cosmetic rule.
RuleParser::Result RuleParser::MaybeParseCosmeticRule(
    base::StringPiece rule_string,
    size_t separator,
    CosmeticRule* rule) {
  size_t separator2 = rule_string.find('#', separator + 1);

  if (separator2 == base::StringPiece::npos || separator2 - separator > 4)
    return Result::kFilterRule;

  for (size_t position = separator + 1; position < separator2; position++) {
    switch (rule_string[position]) {
      case '@':
        if (position == separator + 1) {
          rule->is_allow_rule = true;
        } else {
          return Result::kError;
        }
        break;
      case '$':
      case '?':
        return Result::kUnsupported;
      default:
        return Result::kFilterRule;
    }
  }

  rule->selector = std::string(rule_string.substr(separator2 + 1));
  // Rules should consist of a list of selectors. No actual CSS rules allowed.
  if (rule->selector.empty() ||
      rule->selector.find_first_of('{') != base::StringPiece::npos ||
      rule->selector.find_first_of('}') != base::StringPiece::npos)
    return Result::kError;

  if (!ParseDomains(rule_string.substr(0, separator), ",",
                    &rule->included_domains, &rule->excluded_domains))
    return Result::kError;
  return Result::kCosmeticRule;
}

RuleParser::Result RuleParser::ParseFilterRule(base::StringPiece rule_string,
                                               FilterRule* rule) {
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

  base::StringPiece options_string;
  if (options_start != base::StringPiece::npos)
    options_string = rule_string.substr(options_start);

  // Even if the options string is empty, there is some common setup code
  // that we want to run.
  Result result = ParseFilterRuleOptions(options_string, rule);
  if (result != kFilterRule)
    return result;

  base::StringPiece pattern = rule_string.substr(0, options_start);

  if (base::StartsWith(pattern, "/") && base::EndsWith(pattern, "/") &&
      pattern.length() > 1) {
    pattern.remove_prefix(1);
    pattern.remove_suffix(1);
    rule->pattern_type = FilterRule::kRegex;
    rule->pattern = std::string(pattern);
    rule->ngram_search_string = BuildNgramSearchString(pattern);
    return kFilterRule;
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
    rule->anchor_type.set(FilterRule::kAnchorHost);
  } else if (base::StartsWith(pattern, "|")) {
    rule->anchor_type.set(FilterRule::kAnchorStart);
    pattern.remove_prefix(1);
  }

  if (base::StartsWith(pattern, "*")) {
    // Starting with a wildcard makes anchoring at the start meaningless
    pattern.remove_prefix(1);
    rule->anchor_type.reset(FilterRule::kAnchorHost);
    rule->anchor_type.reset(FilterRule::kAnchorStart);

    // Only try to find a hostname in hostname anchored patterns if the pattern
    // starts with *. or without a wildcard.
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
    rule->anchor_type.set(FilterRule::kAnchorEnd);
  }

  // We had a pattern of the form "|*|", which is equivalent to "*"
  if (pattern.empty()) {
    rule->anchor_type.reset(FilterRule::kAnchorEnd);
  }

  if (base::EndsWith(pattern, "*")) {
    // Ending with a wildcard makes anchoring at the end meaningless
    pattern.remove_suffix(1);
    rule->anchor_type.reset(FilterRule::kAnchorEnd);
    maybe_pure_host = false;
  }

  // Stars at the end don't contribute to the pattern
  while (base::EndsWith(pattern, "*")) {
    pattern.remove_suffix(1);
  }

  if (pattern.find_first_of("*") != base::StringPiece::npos) {
    rule->pattern_type = FilterRule::kWildcarded;
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
    return kFilterRule;
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
      // return kFilterRule;
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

  return kFilterRule;
}

RuleParser::Result RuleParser::ParseFilterRuleOptions(base::StringPiece options,
                                                      FilterRule* rule) {
  if (!options.empty()) {
    DCHECK_EQ('$', options[0]);
    options.remove_prefix(1);
  }

  std::bitset<FilterRule::kTypeCount> types_set;
  std::bitset<FilterRule::kTypeCount> types_unset;
  std::bitset<FilterRule::kActivationCount> activations_set;
  std::bitset<FilterRule::kActivationCount> activations_unset;
  for (base::StringPiece option : base::SplitStringPiece(
           options, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    bool invert = false;
    if (base::StartsWith(option, "~", base::CompareCase::SENSITIVE)) {
      option.remove_prefix(1);
      invert = true;
    }

    size_t option_name_end = option.find('=');
    base::StringPiece option_name = option.substr(0, option_name_end);

    auto type_option = kTypeStringMap.find(option_name);
    if (type_option != kTypeStringMap.end()) {
      if (invert)
        types_unset.set(type_option->second);
      else
        types_set.set(type_option->second);
      continue;
    }

    auto activation_option = kActivationStringMap.find(option_name);
    if (activation_option != kActivationStringMap.end()) {
      if (activation_option->second.allow_only && !rule->is_allow_rule)
        return kError;
      if (invert)
        activations_unset.set(activation_option->second.type);
      else
        activations_set.set(activation_option->second.type);
      continue;
    }

    auto other_option = kOptionMap.find(option_name);
    if (other_option == kOptionMap.end())
      return kUnsupported;

    OptionType option_type = other_option->second;

    if (option_type == OptionType::kThirdParty) {
      rule->party.set(invert ? FilterRule::kFirstParty
                             : FilterRule::kThirdParty);
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
        option_value.remove_prefix(base::size(kRewritePrefix) - 1);
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

  return kFilterRule;
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
      parse_result_->metadata.expires = base::TimeDelta::FromDays(count);
    } else if (expire_data[1].compare("hours") == 0) {
      parse_result_->metadata.expires = base::TimeDelta::FromHours(count);
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
