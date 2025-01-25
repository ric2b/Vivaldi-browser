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
using FlatStringOffsetMap = std::map<std::vector<FlatStringOffset>,
                                     FlatStringListOffset,
                                     OffsetVectorCompare>;

template <typename T>
FlatStringListOffset SerializeStringList(
    flatbuffers::FlatBufferBuilder* builder,
    const T& container,
    FlatStringOffsetMap* string_offset_map) {
  if (container.empty())
    return FlatStringListOffset();

  std::vector<FlatStringOffset> strings;
  strings.reserve(container.size());
  for (const std::string& str : container)
    strings.push_back(builder->CreateSharedString(str));

  auto precedes = [&builder](FlatStringOffset lhs, FlatStringOffset rhs) {
    return SizePrioritizedStringCompare(
               ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, lhs)),
               ToStringPiece(flatbuffers::GetTemporaryPointer(*builder, rhs))) <
           0;
  };
  if (strings.empty())
    return FlatStringListOffset();
  std::sort(strings.begin(), strings.end(), precedes);

  // Share string lists if we've already serialized an exact duplicate. Note
  // that this can share excluded and included domain lists, and modifier lists.
  DCHECK(string_offset_map);
  auto it = string_offset_map->find(strings);
  if (it == string_offset_map->end()) {
    auto offset = builder->CreateVector(strings);
    (*string_offset_map)[strings] = offset;
    return offset;
  }
  return it->second;
}

uint8_t OptionsFromRequestFilterRule(const RequestFilterRule& rule) {
  uint8_t options = 0;
  if (rule.modify_block)
    options |= flat::OptionFlag_MODIFY_BLOCK;
  if (rule.party.test(RequestFilterRule::kFirstParty))
    options |= flat::OptionFlag_FIRST_PARTY;
  if (rule.party.test(RequestFilterRule::kThirdParty))
    options |= flat::OptionFlag_THIRD_PARTY;
  if (rule.is_case_sensitive)
    options |= flat::OptionFlag_IS_CASE_SENSITIVE;
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
  if (rule.explicit_types.test(RequestFilterRule::kDocument))
    resource_types |= flat::ResourceType_DOCUMENT;
  if (rule.explicit_types.test(RequestFilterRule::kPopup))
    resource_types |= flat::ResourceType_POPUP;
  return resource_types;
}

flat::Decision DecisionFromRequestFilterRule(const RequestFilterRule& rule) {
  switch (rule.decision) {
    case RequestFilterRule::kModify:
      return flat::Decision_MODIFY;
    case RequestFilterRule::kPass:
      return flat::Decision_PASS;
    case RequestFilterRule::kModifyImportant:
      return flat::Decision_MODIFY_IMPORTANT;
  }
}

flat::Modifier ModifierFromRequestFilterModifier(
    const RequestFilterRule& rule) {
  switch (rule.modifier) {
    case RequestFilterRule::kNoModifier:
      return flat::Modifier_NO_MODIFIER;
    case RequestFilterRule::kRedirect:
      return flat::Modifier_REDIRECT;
    case RequestFilterRule::kCsp:
      return flat::Modifier_CSP;
    case RequestFilterRule::kAdQueryTrigger:
      return flat::Modifier_AD_QUERY_TRIGGER;
  }
}

uint8_t ActivationTypesFromRequestFilterRule(const RequestFilterRule& rule) {
  uint8_t activation_types = 0;
  if (rule.activation_types.test(RequestFilterRule::kWholeDocument))
    activation_types |= flat::ActivationType_DOCUMENT;
  if (rule.activation_types.test(RequestFilterRule::kElementHide))
    activation_types |= flat::ActivationType_ELEMENT_HIDE;
  if (rule.activation_types.test(RequestFilterRule::kGenericHide))
    activation_types |= flat::ActivationType_GENERIC_HIDE;
  if (rule.activation_types.test(RequestFilterRule::kGenericBlock))
    activation_types |= flat::ActivationType_GENERIC_BLOCK;
  if (rule.activation_types.test(RequestFilterRule::kAttributeAds))
    activation_types |= flat::ActivationType_ATTRIBUTE_ADS;
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

FlatStringOffset StringOffsetFromOptionalString(
    flatbuffers::FlatBufferBuilder* builder,
    const std::optional<std::string>& string) {
  if (!string) {
    return FlatStringOffset();
  }
  return builder->CreateSharedString(*string);
}

void AddRuleToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const RequestFilterRule& rule,
    std::vector<FlatOffset<flat::RequestFilterRule>>* rules_offsets,
    FlatStringOffsetMap* string_offset_map) {
  FlatStringListOffset domains_included_offset =
      SerializeStringList(builder, rule.included_domains, string_offset_map);
  FlatStringListOffset domains_excluded_offset =
      SerializeStringList(builder, rule.excluded_domains, string_offset_map);

  FlatStringOffset pattern_offset = builder->CreateSharedString(rule.pattern);

  FlatStringOffset ngram_search_string_offset =
      StringOffsetFromOptionalString(builder, rule.ngram_search_string);

  FlatStringListOffset ad_domains_and_query_triggers = SerializeStringList(
      builder, rule.ad_domains_and_query_triggers, string_offset_map);

  FlatStringOffset host_offset =
      StringOffsetFromOptionalString(builder, rule.host);
  FlatStringListOffset modifier_value_offset =
      SerializeStringList(builder, rule.modifier_values, string_offset_map);

  rules_offsets->push_back(flat::CreateRequestFilterRule(
      *builder, DecisionFromRequestFilterRule(rule),
      OptionsFromRequestFilterRule(rule),
      ResourceTypesFromRequestFilterRule(rule),
      ActivationTypesFromRequestFilterRule(rule),
      PatternTypeFromRequestFilterRule(rule),
      AnchorTypeFromRequestFilterRule(rule), host_offset,
      ad_domains_and_query_triggers, domains_included_offset,
      domains_excluded_offset, ModifierFromRequestFilterModifier(rule),
      modifier_value_offset, pattern_offset, ngram_search_string_offset));
}

FlatOffset<flat::ContentInjectionRuleCore> AddContentInjectionRuleCoreToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const ContentInjectionRuleCore& core,
    FlatStringOffsetMap* string_offser_map) {
  FlatStringListOffset domains_included_offset =
      SerializeStringList(builder, core.included_domains, string_offser_map);
  FlatStringListOffset domains_excluded_offset =
      SerializeStringList(builder, core.excluded_domains, string_offser_map);
  return flat::CreateContentInjectionRuleCore(*builder, core.is_allow_rule,
                                              domains_included_offset,
                                              domains_excluded_offset);
}

void AddRuleToBuffer(flatbuffers::FlatBufferBuilder* builder,
                     const CosmeticRule& rule,
                     std::vector<FlatOffset<flat::CosmeticRule>>* rules_offsets,
                     FlatStringOffsetMap* string_offset_map) {
  FlatStringOffset selector_offset = builder->CreateSharedString(rule.selector);
  FlatOffset<flat::ContentInjectionRuleCore> rule_core_offset =
      AddContentInjectionRuleCoreToBuffer(builder, rule.core,
                                          string_offset_map);
  rules_offsets->push_back(
      flat::CreateCosmeticRule(*builder, rule_core_offset, selector_offset));
}

void AddRuleToBuffer(
    flatbuffers::FlatBufferBuilder* builder,
    const ScriptletInjectionRule& rule,
    std::vector<FlatOffset<flat::ScriptletInjectionRule>>* rules_offsets,
    FlatStringOffsetMap* string_offset_map) {
  FlatStringOffset scriptlet_name_offset =
      builder->CreateSharedString(rule.scriptlet_name);
  std::vector<FlatStringOffset> argument_offsets;
  for (const auto& argument : rule.arguments) {
    argument_offsets.push_back(builder->CreateSharedString(argument));
  }
  FlatOffset<flat::ContentInjectionRuleCore> rule_core_offset =
      AddContentInjectionRuleCoreToBuffer(builder, rule.core,
                                          string_offset_map);
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
  FlatStringOffsetMap string_offset_map;
  for (const auto& request_filter_rule : parse_result.request_filter_rules) {
    AddRuleToBuffer(&builder, request_filter_rule,
                    &request_filter_rules_offsets, &string_offset_map);
  }
  std::vector<FlatOffset<flat::CosmeticRule>> cosmetic_rules_offsets;
  for (const auto& cosmetic_rule : parse_result.cosmetic_rules) {
    AddRuleToBuffer(&builder, cosmetic_rule, &cosmetic_rules_offsets,
                    &string_offset_map);
  }

  std::vector<FlatOffset<flat::ScriptletInjectionRule>>
      scriptlet_injection_rules_offsets;
  for (const auto& scriptlet_injection_rule :
       parse_result.scriptlet_injection_rules) {
    AddRuleToBuffer(&builder, scriptlet_injection_rule,
                    &scriptlet_injection_rules_offsets, &string_offset_map);
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