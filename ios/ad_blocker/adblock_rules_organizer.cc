// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ios/ad_blocker/adblock_rules_organizer.h"

#include "base/ios/ios_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "ios/ad_blocker/ios_rules_compiler.h"
#include "ios/ad_blocker/utils.h"

namespace adblock_filter {

namespace {
// This is not the maximum amount allowed by webkit. We have been tweaking these
// values to find a sweet spot in terms of performance. WebKit seem to struggle
// processing very large lists instead of many small lists.
constexpr size_t kMaxRules = 15000;

// This restriction isn't imposed by iOS, but since we are going to have a copy
// of all allow rules in every rule list, we better make sure there is
// resaonable space left for other rules.
constexpr size_t kMaxAllowRules = 5000;
constexpr size_t kMaxGenericAllowRules = 500;
constexpr size_t kMaxAllowAndGenericAllowRules =
    kMaxAllowRules + kMaxGenericAllowRules;

class BlockListListMaker {
 public:
  BlockListListMaker(base::Value::List&& allow_rules)
      : max_rules_(kMaxRules), allow_rules_(std::move(allow_rules)) {
    next_list_.reserve(kMaxAllowRules);
  }
  ~BlockListListMaker() {
    DCHECK(block_lists_.empty() && next_list_.empty())
        << "Result should be read before destruction";
  }

  BlockListListMaker(const BlockListListMaker&) = delete;
  BlockListListMaker& operator=(const BlockListListMaker&) = delete;

  void AddRules(base::Value::List&& rules) {
    for (auto& rule : rules) {
      if ((next_list_.size() + allow_rules_.size()) == max_rules_) {
        AddNextList();
      }
      next_list_.Append(std::move(rule));
    }
  }

  void AddRulePairs(base::Value::List&& pairs) {
    for (auto& pair : pairs) {
      DCHECK(pair.is_list());
      DCHECK(pair.GetList().size() <= 2);
      DCHECK(pair.GetList().size() >= 1);
      // Make sure we have space for the pair.
      if ((next_list_.size() + allow_rules_.size()) >
          max_rules_ - pair.GetList().size()) {
        AddNextList();
      }
      next_list_.Append(std::move(pair.GetList()[0]));
      if (pair.GetList().size() == 2) {
        next_list_.Append(std::move(pair.GetList()[1]));
      }
    }
  }

  base::Value::List GetAndReset() {
    if (!next_list_.empty())
      AddNextList();

    return std::move(block_lists_);
  }

 private:
  void AddNextList() {
    for (const auto& allow_rule : allow_rules_) {
      next_list_.Append(allow_rule.Clone());
    }
    std::string serialized_list;
    JSONStringValueSerializer(&serialized_list).Serialize(next_list_);
    block_lists_.Append(serialized_list);
    next_list_.clear();
    next_list_.reserve(max_rules_);
  }

  const size_t max_rules_;
  base::Value::List block_lists_;
  base::Value::List next_list_;
  base::Value::List allow_rules_;
};

void MergeRules(const base::Value::Dict& from, base::Value::Dict& into) {
  for (const auto [key, value] : from) {
    if (const base::Value::Dict* from_dict = value.GetIfDict()) {
      if (base::Value::Dict* into_dict = into.FindDict(key)) {
        MergeRules(*from_dict, *into_dict);
        continue;
      }
      DCHECK(!into.contains(key));
    }

    if (const base::Value::List* from_list = value.GetIfList()) {
      if (base::Value::List* into_list = into.FindList(key)) {
        into_list->reserve(into_list->size() + from_list->size());
        for (const auto& item : *from_list) {
          into_list->Append(item.Clone());
        }
        continue;
      }
      DCHECK(!into.contains(key));
    }

    into.Set(key, value.Clone());
  }
}

}  // namespace

CompiledRules::CompiledRules(base::Value rules, std::string checksum)
    : rules_(std::move(rules)), checksum_(std::move(checksum)) {}
CompiledRules::~CompiledRules() = default;

base::Value OrganizeRules(
    std::map<uint32_t, scoped_refptr<CompiledRules>> all_compiled_rules,
    base::Value exception_rule) {
  base::Value::List all_network_allow_rules;
  all_network_allow_rules.reserve(kMaxAllowRules);
  base::Value::List all_network_allow_and_generic_allow_rules;
  all_network_allow_and_generic_allow_rules.reserve(
      kMaxAllowAndGenericAllowRules);
  // This is essentially elemhide allow rules
  base::Value::List all_cosmetic_allow_rules;
  all_cosmetic_allow_rules.reserve(kMaxGenericAllowRules);
  // This is essentially generichide allow rules
  base::Value::List all_cosmetic_allow_and_generic_allow_rules;
  all_cosmetic_allow_and_generic_allow_rules.reserve(kMaxGenericAllowRules);

  base::Value::Dict merged_scriptlet_rules;

  base::Value::Dict metadata;

  for (const auto& [id, compiled_rules] : all_compiled_rules) {
    // Record this to ensure we can find out if the organized rules set still
    // matches the original compiled rules lists-
    metadata.EnsureDict(rules_json::kListChecksums)
        ->Set(base::NumberToString(id), compiled_rules->checksum());

    DCHECK(compiled_rules->rules().is_dict());
    const base::Value::Dict& rules = compiled_rules->rules().GetDict();
    const base::Value::Dict* network_rules =
        rules.FindDict(rules_json::kNetworkRules);
    if (network_rules) {
      const base::Value::List* network_allow_rules =
          network_rules->FindList(rules_json::kAllowRules);
      if (network_allow_rules) {
        for (const auto& rule : *network_allow_rules) {
          all_network_allow_rules.Append(rule.Clone());
          all_network_allow_and_generic_allow_rules.Append(rule.Clone());
        }
      }
      const base::Value::List* network_generic_allow_rules =
          network_rules->FindList(rules_json::kGenericAllowRules);
      if (network_generic_allow_rules) {
        for (const auto& rule : *network_generic_allow_rules)
          all_network_allow_and_generic_allow_rules.Append(rule.Clone());
      }
    }

    const base::Value::Dict* cosmetic_rules =
        rules.FindDict(rules_json::kCosmeticRules);
    if (cosmetic_rules) {
      const base::Value::List* cosmetic_allow_rules =
          cosmetic_rules->FindList(rules_json::kAllowRules);
      if (cosmetic_allow_rules) {
        for (const auto& rule : *cosmetic_allow_rules)
          all_cosmetic_allow_rules.Append(rule.Clone());
      }
      const base::Value::List* cosmetic_generic_allow_rules =
          cosmetic_rules->FindList(rules_json::kGenericAllowRules);
      if (cosmetic_generic_allow_rules) {
        for (const auto& rule : *cosmetic_generic_allow_rules)
          all_cosmetic_allow_and_generic_allow_rules.Append(rule.Clone());
      }
    }

    const base::Value::Dict* scriptlet_rules =
        rules.FindDict(rules_json::kScriptletRules);
    if (scriptlet_rules)
      MergeRules(*scriptlet_rules, merged_scriptlet_rules);
  }

  if (all_network_allow_rules.size() > kMaxAllowRules ||
      all_network_allow_and_generic_allow_rules.size() >
          kMaxAllowAndGenericAllowRules ||
      all_cosmetic_allow_rules.size() > kMaxAllowRules ||
      all_cosmetic_allow_and_generic_allow_rules.size() >
          kMaxAllowAndGenericAllowRules) {
    return base::Value();
  }

  if (exception_rule.is_dict()) {
    all_network_allow_rules.Append(exception_rule.Clone());
    all_network_allow_and_generic_allow_rules.Append(exception_rule.Clone());
    all_cosmetic_allow_rules.Append(exception_rule.Clone());
    all_cosmetic_allow_and_generic_allow_rules.Append(exception_rule.Clone());
    std::string serialized_exception;
    if (!JSONStringValueSerializer(&serialized_exception)
             .Serialize(exception_rule))
      NOTREACHED();
    metadata.Set(rules_json::kExceptionRule,
                 CalculateBufferChecksum(serialized_exception));
  }

  BlockListListMaker network_specific_block_lists_maker(
      std::move(all_network_allow_rules));
  BlockListListMaker network_generic_block_lists_maker(
      std::move(all_network_allow_and_generic_allow_rules));
  BlockListListMaker network_block_important_lists_maker(base::Value::List{});
  std::map<std::string, base::Value::Dict> rules_for_selectors;

  for (const auto& [id, compiled_rules] : all_compiled_rules) {
    DCHECK(compiled_rules->rules().is_dict());
    const base::Value::Dict& rules = compiled_rules->rules().GetDict();
    const base::Value::Dict* network_rules =
        rules.FindDict(rules_json::kNetworkRules);
    if (network_rules) {
      const base::Value::List* block_allow_pairs =
          network_rules->FindList(rules_json::kBlockAllowPairs);
      if (block_allow_pairs) {
        network_specific_block_lists_maker.AddRulePairs(
            block_allow_pairs->Clone());
      }
      const base::Value::Dict* block_rules =
          network_rules->FindDict(rules_json::kBlockRules);
      if (block_rules) {
        const base::Value::List* specific_block_rules =
            block_rules->FindList(rules_json::kSpecific);
        if (specific_block_rules) {
          network_specific_block_lists_maker.AddRules(
              specific_block_rules->Clone());
        }
        const base::Value::List* generic_block_rules =
            block_rules->FindList(rules_json::kGeneric);
        if (generic_block_rules) {
          network_generic_block_lists_maker.AddRules(
              generic_block_rules->Clone());
        }
      }
      const base::Value::List* block_important_rules =
          network_rules->FindList(rules_json::kBlockImportantRules);
      if (block_important_rules) {
        network_block_important_lists_maker.AddRules(
            block_important_rules->Clone());
      }
    }

    const base::Value::Dict* cosmetic_rules =
        rules.FindDict(rules_json::kCosmeticRules);
    if (cosmetic_rules) {
      const base::Value::Dict* cosmetic_rules_selectors =
          cosmetic_rules->FindDict(rules_json::kSelector);
      if (cosmetic_rules_selectors) {
        for (const auto [selector, rules_for_selector] :
             *cosmetic_rules_selectors)
          MergeRules(rules_for_selector.GetDict(),
                     rules_for_selectors[selector]);
      }
    }
  }

  base::Value::List ios_content_blocker_rules;
  for (auto* maker :
       {&network_specific_block_lists_maker, &network_generic_block_lists_maker,
        &network_block_important_lists_maker}) {
    base::Value::List lists = maker->GetAndReset();
    for (auto& list : lists) {
      ios_content_blocker_rules.Append(std::move(list));
    }
  }

  BlockListListMaker cosmetic_specific_block_lists_maker(
      all_cosmetic_allow_rules.Clone());
  BlockListListMaker cosmetic_generic_block_list_maker(
      all_cosmetic_allow_and_generic_allow_rules.Clone());
  std::unique_ptr<BlockListListMaker>
      selector_specific_cosmetic_specific_block_lists_maker;
  std::unique_ptr<BlockListListMaker>
      selector_specific_cosmetic_generic_block_lists_maker;

  for (auto& [selector, rules] : rules_for_selectors) {
    base::Value::List* allow_rules = rules.FindList(rules_json::kAllowRules);
    base::Value::Dict* block_rules = rules.FindDict(rules_json::kBlockRules);
    if (!block_rules)
      continue;

    BlockListListMaker* specific_block_lists_maker_to_use =
        &cosmetic_specific_block_lists_maker;
    BlockListListMaker* generic_block_lists_maker_to_use =
        &cosmetic_generic_block_list_maker;
    if (allow_rules) {
      if (allow_rules->size() > kMaxAllowRules) {
        return base::Value();
      }
      base::Value::List specific_allow_rules(allow_rules->Clone());
      for (const auto& rule : all_cosmetic_allow_rules)
        specific_allow_rules.Append(rule.Clone());
      selector_specific_cosmetic_specific_block_lists_maker =
          std::make_unique<BlockListListMaker>(std::move(specific_allow_rules));
      specific_block_lists_maker_to_use =
          selector_specific_cosmetic_specific_block_lists_maker.get();

      base::Value::List specific_and_generic_allow_rules(
          std::move(*allow_rules));
      for (const auto& rule : all_cosmetic_allow_and_generic_allow_rules)
        specific_and_generic_allow_rules.Append(rule.Clone());
      selector_specific_cosmetic_generic_block_lists_maker =
          std::make_unique<BlockListListMaker>(
              std::move(specific_and_generic_allow_rules));
      generic_block_lists_maker_to_use =
          selector_specific_cosmetic_generic_block_lists_maker.get();
    }
    base::Value::List* block_allow_pairs =
        rules.FindList(rules_json::kBlockAllowPairs);
    if (block_allow_pairs) {
      specific_block_lists_maker_to_use->AddRulePairs(
          std::move(*block_allow_pairs));
    }
    base::Value::List* specific_block_rules =
        block_rules->FindList(rules_json::kSpecific);
    if (specific_block_rules) {
      specific_block_lists_maker_to_use->AddRules(
          std::move(*specific_block_rules));
    }
    base::Value::List* generic_block_rules =
        block_rules->FindList(rules_json::kGeneric);
    if (generic_block_rules) {
      generic_block_lists_maker_to_use->AddRules(
          std::move(*generic_block_rules));
    }
    if (allow_rules) {
      for (auto* maker :
           {selector_specific_cosmetic_specific_block_lists_maker.get(),
            selector_specific_cosmetic_generic_block_lists_maker.get()}) {
        base::Value::List lists = maker->GetAndReset();
        for (auto& list : lists) {
          ios_content_blocker_rules.Append(std::move(list));
        }
      }
    }
  }

  for (auto* maker : {&cosmetic_specific_block_lists_maker,
                      &cosmetic_generic_block_list_maker}) {
    base::Value::List lists = maker->GetAndReset();
    for (auto& list : lists) {
      ios_content_blocker_rules.Append(std::move(list));
    }
  }

  base::Value::Dict non_ios_rules_and_metadata;
  non_ios_rules_and_metadata.Set(rules_json::kVersion,
                                 GetOrganizedRulesVersionNumber());
  non_ios_rules_and_metadata.Set(rules_json::kMetadata, std::move(metadata));
  non_ios_rules_and_metadata.Set(rules_json::kScriptletRules,
                                 std::move(merged_scriptlet_rules));

  base::Value::Dict result;
  result.Set(rules_json::kNonIosRulesAndMetadata,
             std::move(non_ios_rules_and_metadata));
  result.Set(rules_json::kIosContentBlockerRules,
             std::move(ios_content_blocker_rules));

  return base::Value(std::move(result));
}
}  // namespace adblock_filter