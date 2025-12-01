// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/index/flat_rules_compiler.h"

#include <map>

#include "base/files/file_util.h"
#include "components/ad_blocker/content/index/index_utils.h"
#include "components/ad_blocker/core/adblock_request_filter_rule.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "vivaldi/components/ad_blocker/content/index/flat/adblock_rules_list_generated.h"

namespace adblock_filter {

namespace {
template <typename T>
using FlatOffset = flatbuffers::Offset<T>;

template <typename T>
using FlatVectorOffset = FlatOffset<flatbuffers::Vector<FlatOffset<T>>>;

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
  if (rule.is_case_sensitive)
    options |= flat::OptionFlag_IS_CASE_SENSITIVE;
  return options;
}

uint16_t MethodFromRequestFilterRule(const RequestFilterRule& rule) {
  uint16_t methods = 0;
  for (RequestMethod method : rule.request_methods) {
    methods |= ([](RequestMethod method) {
      switch (method) {
        case RequestMethod::KConnect:
          return flat::Method_CONNECT;
        case RequestMethod::kDelete:
          return flat::Method_DELETE;
        case RequestMethod::kGet:
          return flat::Method_GET;
        case RequestMethod::kHead:
          return flat::Method_HEAD;
        case RequestMethod::kOptions:
          return flat::Method_OPTIONS;
        case RequestMethod::kPatch:
          return flat::Method_PATCH;
        case RequestMethod::kPost:
          return flat::Method_POST;
        case RequestMethod::kPut:
          return flat::Method_PUT;
        case RequestMethod::kOther:
          return flat::Method::Method_OTHER;
      }
    })(method);
  }

  return methods;
}

uint32_t ResourceTypesFromRequestFilterRule(const RequestFilterRule& rule) {
  uint32_t resource_types = 0;
  for (ResourceType type : rule.resource_types) {
    resource_types |= ToFlatResourceType(type);
  }
  for (ResourceType type : rule.explicit_types) {
    resource_types |= ToFlatResourceType(type);
  }

  return resource_types;
}

flat::Decision DecisionFromRequestFilterRule(const RequestFilterRule& rule) {
  switch (rule.decision) {
    case RuleDecision::kModify:
      return flat::Decision_MODIFY;
    case RuleDecision::kPass:
      return flat::Decision_PASS;
    case RuleDecision::kModifyImportant:
      return flat::Decision_MODIFY_IMPORTANT;
  }
}

flat::Modifier ModifierFromRequestFilterModifier(
    const RequestFilterRule& rule) {
  switch (rule.modifier) {
    case ModifierType::kNoModifier:
      return flat::Modifier_NO_MODIFIER;
    case ModifierType::kRedirect:
      return flat::Modifier_REDIRECT;
    case ModifierType::kCsp:
      return flat::Modifier_CSP;
    case ModifierType::kAdQueryTrigger:
      return flat::Modifier_AD_QUERY_TRIGGER;
  }
}

flat::Party PartyFromRequestFilterParty(const RequestFilterRule& rule) {
  if (!rule.party) {
    return flat::Party_ALL;
  }
  switch (*rule.party) {
    case Party::kFirst:
      return flat::Party_FIRST;
    case Party::kThird:
      return flat::Party_THIRD;
    case Party::kStrictFirst:
      return flat::Party_STRICT_FIRST;
    case Party::kStrictThird:
      return flat::Party_STRICT_THIRD;
    case Party::kFirstAndStrictThird:
      return flat::Party_FIRST_AND_STRICT_THIRD;
  }
}

uint8_t ActivationTypesFromRequestFilterRule(const RequestFilterRule& rule) {
  uint8_t activation_types = 0;
  if (rule.activation_types.Has(ActivationType::kWholeDocument))
    activation_types |= flat::ActivationType_DOCUMENT;
  if (rule.activation_types.Has(ActivationType::kSpecificHide))
    activation_types |= flat::ActivationType_SPECIFIC_HIDE;
  if (rule.activation_types.Has(ActivationType::kGenericHide))
    activation_types |= flat::ActivationType_GENERIC_HIDE;
  if (rule.activation_types.Has(ActivationType::kGenericBlock))
    activation_types |= flat::ActivationType_GENERIC_BLOCK;
  if (rule.activation_types.Has(ActivationType::kAttributeAds))
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
    std::vector<FlatOffset<flat::RequestFilterRule>>* bad_rules_offsets,
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

  FlatStringOffset original_rule_text =
      builder->CreateSharedString(rule.original_rule_text);

  auto rule_offset = flat::CreateRequestFilterRule(
      *builder, DecisionFromRequestFilterRule(rule),
      OptionsFromRequestFilterRule(rule), PartyFromRequestFilterParty(rule),
      MethodFromRequestFilterRule(rule),
      ResourceTypesFromRequestFilterRule(rule),
      ActivationTypesFromRequestFilterRule(rule),
      PatternTypeFromRequestFilterRule(rule),
      AnchorTypeFromRequestFilterRule(rule), host_offset,
      ad_domains_and_query_triggers, domains_included_offset,
      domains_excluded_offset, ModifierFromRequestFilterModifier(rule),
      modifier_value_offset, pattern_offset, ngram_search_string_offset,
      original_rule_text);

  if (rule.bad_filter) {
    bad_rules_offsets->push_back(rule_offset);
  } else {
    rules_offsets->push_back(rule_offset);
  }
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
  if (!base::CreateDirectoryAndGetError(output_path.DirName(), nullptr))
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
                      const RuleSourceSettings& source_settings,
                      const base::FilePath& output_path,
                      std::string& checksum) {
  flatbuffers::FlatBufferBuilder builder;
  std::vector<FlatOffset<flat::RequestFilterRule>> request_filter_rules_offsets;
  std::vector<FlatOffset<flat::RequestFilterRule>>
      bad_request_filter_rules_offsets;
  FlatStringOffsetMap string_offset_map;
  for (const auto& request_filter_rule : parse_result.request_filter_rules) {
    AddRuleToBuffer(&builder, request_filter_rule,
                    &request_filter_rules_offsets,
                    &bad_request_filter_rules_offsets, &string_offset_map);
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
      builder.CreateVector(bad_request_filter_rules_offsets),
      builder.CreateVector(cosmetic_rules_offsets),
      builder.CreateVector(scriptlet_injection_rules_offsets));

  flat::FinishRulesListBuffer(builder, root_offset);

  return SaveRulesList(
      output_path, base::span(builder.GetBufferPointer(), builder.GetSize()),
      checksum);
}
}  // namespace adblock_filter
