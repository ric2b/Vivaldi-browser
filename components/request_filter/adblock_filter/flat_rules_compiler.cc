// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/flat_rules_compiler.h"

#include <map>

#include "base/files/file_util.h"
#include "components/ad_blocker/adblock_request_filter_rule.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace adblock_filter {

namespace {
template <typename T>
using FlatOffset = flatbuffers::Offset<T>;

template <typename T>
using FlatVectorOffset = FlatOffset<flatbuffers::Vector<FlatOffset<T>>>;

using FlatStringOffset = FlatOffset<flatbuffers::String>;
using FlatStringListOffset = FlatVectorOffset<flatbuffers::String>;

struct OffsetVectorCompare {
  bool operator()(const std::vector<FlatStringOffset>& a,
                  const std::vector<FlatStringOffset>& b) const {
    auto compare = [](const FlatStringOffset a_offset,
                      const FlatStringOffset b_offset) {
      DCHECK(!a_offset.IsNull());
      DCHECK(!b_offset.IsNull());
      return a_offset.o < b_offset.o;
    };
    // |lexicographical_compare| is how vector::operator< is implemented.
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                        compare);
  }
};
using FlatDomainMap = std::map<std::vector<FlatStringOffset>,
                               FlatStringListOffset,
                               OffsetVectorCompare>;

template <typename T>
FlatStringListOffset SerializeDomainList(
    flatbuffers::FlatBufferBuilder* builder,
    const T& container,
    FlatDomainMap* domain_map) {
  if (container.empty())
    return FlatStringListOffset();

  std::vector<FlatStringOffset> domains;
  domains.reserve(container.size());
  for (const std::string& str : container)
    domains.push_back(builder->CreateSharedString(str));

  auto precedes = [&builder](FlatStringOffset lhs, FlatStringOffset rhs) {
    return CompareDomains(
               ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, lhs)),
               ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, rhs))) <
           0;
  };
  if (domains.empty())
    return FlatStringListOffset();
  std::sort(domains.begin(), domains.end(), precedes);

  // Share domain lists if we've already serialized an exact duplicate. Note
  // that this can share excluded and included domain lists.
  DCHECK(domain_map);
  auto it = domain_map->find(domains);
  if (it == domain_map->end()) {
    auto offset = builder->CreateVector(domains);
    (*domain_map)[domains] = offset;
    return offset;
  }
  return it->second;
}

uint8_t OptionsFromRequestFilterRule(const RequestFilterRule& rule) {
  uint8_t options = 0;
  if (rule.party.test(RequestFilterRule::kFirstParty))
    options |= flat::OptionFlag_FIRST_PARTY;
  if (rule.party.test(RequestFilterRule::kThirdParty))
    options |= flat::OptionFlag_THIRD_PARTY;
  if (rule.is_allow_rule)
    options |= flat::OptionFlag_IS_ALLOW_RULE;
  if (rule.is_case_sensitive)
    options |= flat::OptionFlag_IS_CASE_SENSITIVE;
  if (rule.is_csp_rule)
    options |= flat::OptionFlag_IS_CSP_RULE;
  return options;
}

uint16_t ResourceTypesFromRequestFilterRule(const RequestFilterRule& rule) {
  uint16_t resource_types = 0;
  if (rule.resource_types.test(RequestFilterRule::kStylesheet))
    resource_types |= flat::ResourceType_STYLESHEET;
  if (rule.resource_types.test(RequestFilterRule::kImage))
    resource_types |= flat::ResourceType_IMAGE;
  if (rule.resource_types.test(RequestFilterRule::kObject))
    resource_types |= flat::ResourceType_OBJECT;
  if (rule.resource_types.test(RequestFilterRule::kScript))
    resource_types |= flat::ResourceType_SCRIPT;
  if (rule.resource_types.test(RequestFilterRule::kXmlHttpRequest))
    resource_types |= flat::ResourceType_XMLHTTPREQUEST;
  if (rule.resource_types.test(RequestFilterRule::kSubDocument))
    resource_types |= flat::ResourceType_SUBDOCUMENT;
  if (rule.resource_types.test(RequestFilterRule::kFont))
    resource_types |= flat::ResourceType_FONT;
  if (rule.resource_types.test(RequestFilterRule::kMedia))
    resource_types |= flat::ResourceType_MEDIA;
  if (rule.resource_types.test(RequestFilterRule::kWebSocket))
    resource_types |= flat::ResourceType_WEBSOCKET;
  if (rule.resource_types.test(RequestFilterRule::kWebRTC))
    resource_types |= flat::ResourceType_WEBRTC;
  if (rule.resource_types.test(RequestFilterRule::kPing))
    resource_types |= flat::ResourceType_PING;
  if (rule.resource_types.test(RequestFilterRule::kWebTransport))
    resource_types |= flat::ResourceType_WEBTRANSPORT;
  if (rule.resource_types.test(RequestFilterRule::kOther))
    resource_types |= flat::ResourceType_OTHER;
  return resource_types;
}

uint8_t ActivationTypesFromRequestFilterRule(const RequestFilterRule& rule) {
  uint8_t activation_types = 0;
  if (rule.activation_types.test(RequestFilterRule::kPopup))
    activation_types |= flat::ActivationType_POPUP;
  if (rule.activation_types.test(RequestFilterRule::kDocument))
    activation_types |= flat::ActivationType_DOCUMENT;
  if (rule.activation_types.test(RequestFilterRule::kElementHide))
    activation_types |= flat::ActivationType_ELEMENT_HIDE;
  if (rule.activation_types.test(RequestFilterRule::kGenericHide))
    activation_types |= flat::ActivationType_GENERIC_HIDE;
  if (rule.activation_types.test(RequestFilterRule::kGenericBlock))
    activation_types |= flat::ActivationType_GENERIC_BLOCK;
  return activation_types;
}

flat::PatternType PatternTypeFromRequestFilterRule(
    const RequestFilterRule& rule) {
  switch (rule.pattern_type) {
    case RequestFilterRule::kPlain:
      return flat::PatternType_PLAIN;
    case RequestFilterRule::kWildcarded:
      return flat::PatternType_WILDCARDED;
    case RequestFilterRule::kRegex:
      return flat::PatternType_REGEXP;
  }
}

uint8_t AnchorTypeFromRequestFilterRule(const RequestFilterRule& rule) {
  uint8_t anchor_type = 0;
  if (rule.anchor_type.test(RequestFilterRule::kAnchorStart))
    anchor_type |= flat::AnchorType_START;
  if (rule.anchor_type.test(RequestFilterRule::kAnchorEnd))
    anchor_type |= flat::AnchorType_END;
  if (rule.anchor_type.test(RequestFilterRule::kAnchorHost))
    anchor_type |= flat::AnchorType_HOST;
  return anchor_type;
}

void AddRuleToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const RequestFilterRule& rule,
    std::vector<FlatOffset<flat::RequestFilterRule>>* rules_offsets,
    FlatDomainMap* domain_map) {
  FlatStringListOffset domains_included_offset =
      SerializeDomainList(builder, rule.included_domains, domain_map);
  FlatStringListOffset domains_excluded_offset =
      SerializeDomainList(builder, rule.excluded_domains, domain_map);

  FlatStringOffset pattern_offset = builder->CreateSharedString(rule.pattern);

  FlatStringOffset ngram_search_string_offset =
      builder->CreateSharedString(rule.ngram_search_string);

  FlatStringOffset host_offset = builder->CreateSharedString(rule.host);
  FlatStringOffset redirect_offset = builder->CreateSharedString(rule.redirect);
  FlatStringOffset csp_offset = builder->CreateSharedString(rule.csp);

  rules_offsets->push_back(flat::CreateRequestFilterRule(
      *builder, OptionsFromRequestFilterRule(rule),
      ResourceTypesFromRequestFilterRule(rule),
      ActivationTypesFromRequestFilterRule(rule),
      PatternTypeFromRequestFilterRule(rule),
      AnchorTypeFromRequestFilterRule(rule), host_offset,
      domains_included_offset, domains_excluded_offset, redirect_offset,
      csp_offset, pattern_offset, ngram_search_string_offset));
}

FlatOffset<flat::ContentInjectionRuleCore> AddContentInjectionRuleCoreToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const ContentInjectionRuleCore& core,
    FlatDomainMap* domain_map) {
  FlatStringListOffset domains_included_offset =
      SerializeDomainList(builder, core.included_domains, domain_map);
  FlatStringListOffset domains_excluded_offset =
      SerializeDomainList(builder, core.excluded_domains, domain_map);
  return flat::CreateContentInjectionRuleCore(*builder, core.is_allow_rule,
                                              domains_included_offset,
                                              domains_excluded_offset);
}

void AddRuleToBuffer(flatbuffers::FlatBufferBuilder* builder,
                     const CosmeticRule& rule,
                     std::vector<FlatOffset<flat::CosmeticRule>>* rules_offsets,
                     FlatDomainMap* domain_map) {
  FlatStringOffset selector_offset = builder->CreateSharedString(rule.selector);
  FlatOffset<flat::ContentInjectionRuleCore> rule_core_offset =
      AddContentInjectionRuleCoreToBuffer(builder, rule.core, domain_map);
  rules_offsets->push_back(
      flat::CreateCosmeticRule(*builder, rule_core_offset, selector_offset));
}

void AddRuleToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const ScriptletInjectionRule& rule,
    std::vector<FlatOffset<flat::ScriptletInjectionRule>>* rules_offsets,
    FlatDomainMap* domain_map) {
  FlatStringOffset scriptlet_name_offset =
      builder->CreateSharedString(rule.scriptlet_name);
  std::vector<FlatStringOffset> argument_offsets;
  for (const auto& argument : rule.arguments) {
    argument_offsets.push_back(builder->CreateSharedString(argument));
  }
  FlatOffset<flat::ContentInjectionRuleCore> rule_core_offset =
      AddContentInjectionRuleCoreToBuffer(builder, rule.core, domain_map);
  rules_offsets->push_back(flat::CreateScriptletInjectionRule(
      *builder, rule_core_offset, scriptlet_name_offset,
      builder->CreateVector(argument_offsets)));
}

bool SaveRulesList(const base::FilePath& output_path,
                   base::span<const uint8_t> data,
                   std::string& checksum) {
  if (!base::CreateDirectory(output_path.DirName()))
    return false;

  base::File output_file(
      output_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!output_file.IsValid())
    return false;

  // Write the version header.
  std::string version_header = GetRulesListVersionHeader();
  int version_header_size = static_cast<int>(version_header.size());
  if (output_file.WriteAtCurrentPos(
          version_header.data(), version_header_size) != version_header_size) {
    return false;
  }

  // Write the flatbuffer ruleset.
  if (!base::IsValueInRangeForNumericType<int>(data.size()))
    return false;
  int data_size = static_cast<int>(data.size());
  if (output_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data.data()),
                                    data_size) != data_size) {
    return false;
  }

  checksum = CalculateBufferChecksum(data);

  return true;
}
}  // namespace

bool CompileFlatRules(const ParseResult& parse_result,
                      const base::FilePath& output_path,
                      std::string& checksum) {
  flatbuffers::FlatBufferBuilder builder;
  std::vector<FlatOffset<flat::RequestFilterRule>> request_filter_rules_offsets;
  FlatDomainMap domain_map;
  for (const auto& request_filter_rule : parse_result.request_filter_rules) {
    AddRuleToBuffer(&builder, request_filter_rule,
                    &request_filter_rules_offsets, &domain_map);
  }
  std::vector<FlatOffset<flat::CosmeticRule>> cosmetic_rules_offsets;
  for (const auto& cosmetic_rule : parse_result.cosmetic_rules) {
    AddRuleToBuffer(&builder, cosmetic_rule, &cosmetic_rules_offsets,
                    &domain_map);
  }

  std::vector<FlatOffset<flat::ScriptletInjectionRule>>
      scriptlet_injection_rules_offsets;
  for (const auto& scriptlet_injection_rule :
       parse_result.scriptlet_injection_rules) {
    AddRuleToBuffer(&builder, scriptlet_injection_rule,
                    &scriptlet_injection_rules_offsets, &domain_map);
  }

  FlatOffset<flat::RulesList> root_offset = flat::CreateRulesList(
      builder, builder.CreateVector(request_filter_rules_offsets),
      builder.CreateVector(cosmetic_rules_offsets),
      builder.CreateVector(scriptlet_injection_rules_offsets));

  flat::FinishRulesListBuffer(builder, root_offset);

  return SaveRulesList(
      output_path,
      base::make_span(builder.GetBufferPointer(), builder.GetSize()), checksum);
}
}  // namespace adblock_filter