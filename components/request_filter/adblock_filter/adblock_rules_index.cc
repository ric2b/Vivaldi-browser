// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rules_index.h"

#include <array>
#include <bitset>
#include <utility>
#include <vector>

#include "adblock_rules_index.h"
#include "base/strings/string_split.h"
#include "components/ad_blocker/parse_utils.h"
#include "components/request_filter/adblock_filter/adblock_rule_pattern_matcher.h"
#include "components/request_filter/adblock_filter/stylesheet_builder.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "components/url_pattern_index/closed_hash_map.h"
#include "components/url_pattern_index/ngram_extractor.h"
#include "components/url_pattern_index/uint64_hasher.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/re2/src/re2/stringpiece.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_index_generated.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace adblock_filter {
namespace {
// The integer type used to represent N-grams.
using NGram = uint64_t;
// The hasher used for hashing N-grams.
using NGramHasher = url_pattern_index::Uint64ToUint32Hasher;
// The hash table probe sequence used both by UrlPatternIndex and its builder.
using NGramHashTableProber =
    url_pattern_index::DefaultProber<NGram, NGramHasher>;

using FlatNGramIndex =
    flatbuffers::Vector<flatbuffers::Offset<flat::NGramToRules>>;

using FlatRulesByModifierList =
    flatbuffers::Vector<flatbuffers::Offset<flat::PrioritizedRuleList>>;
using FlatRuleIdList = flatbuffers::Vector<flatbuffers::Offset<flat::RuleId>>;

using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;
using FlatStringList = flatbuffers::Vector<FlatStringOffset>;

const size_t kMaxActivationCacheSize = 10;

template <class T>
struct ContentInjectionIndexTraversalResult {
  std::set<const T*, ContentInjectionRuleBodyCompare> selected;
  std::set<const T*, ContentInjectionRuleBodyCompare> exceptions;
};

void BuildAbpInjectionData(std::string snippets_arguments,
                           std::string scriptlet_name,
                           RulesIndex::InjectionData& injection_data) {
  if (snippets_arguments.empty()) {
    return;
  }
  DCHECK(snippets_arguments.back() == ',');
  // Remove extra comma
  snippets_arguments.pop_back();
  RulesIndex::ScriptletInjection scriptlet_injection;
  scriptlet_injection.first = std::move(scriptlet_name);
  scriptlet_injection.second.push_back(std::move(snippets_arguments));
  injection_data.scriptlet_injections.push_back(std::move(scriptlet_injection));
}

struct ContentInjectionIndexTraversalResults {
  ContentInjectionIndexTraversalResult<flat::CosmeticRule> cosmetic_rules;
  ContentInjectionIndexTraversalResult<flat::ScriptletInjectionRule>
      scriptlet_injection_rules;

  RulesIndex::InjectionData ToInjectionData() {
    RulesIndex::InjectionData injection_data;
    injection_data.stylesheet = BuildStyleSheet(cosmetic_rules.selected);

    std::string abp_snippets_main_arguments;
    std::string abp_snippets_isolated_arguments;
    for (const flat::ScriptletInjectionRule* rule :
         scriptlet_injection_rules.selected) {
      if (rule->scriptlet_name()->string_view() ==
          kAbpSnippetsMainScriptletName) {
        DCHECK(rule->arguments()->size() == 1);
        // The ABP snippet arguments were purposefully left with a trailing
        // comma at the parsing stage. We can just concatenate them here.
        abp_snippets_main_arguments += rule->arguments()->Get(0)->str();
      } else if (rule->scriptlet_name()->string_view() ==
                 kAbpSnippetsIsolatedScriptletName) {
        DCHECK(rule->arguments()->size() == 1);
        abp_snippets_isolated_arguments += rule->arguments()->Get(0)->str();
      } else {
        RulesIndex::ScriptletInjection scriptlet_injection;
        scriptlet_injection.first = rule->scriptlet_name()->str();
        for (auto* argument : *(rule->arguments()))
          scriptlet_injection.second.push_back(argument->str());
        injection_data.scriptlet_injections.push_back(
            std::move(scriptlet_injection));
      }
    }

    BuildAbpInjectionData(std::move(abp_snippets_main_arguments),
                          kAbpSnippetsMainScriptletName, injection_data);
    BuildAbpInjectionData(std::move(abp_snippets_isolated_arguments),
                          kAbpSnippetsIsolatedScriptletName, injection_data);

    return injection_data;
  }
};

// Returns whether the request matches the third-party of the specified
// filtering `rule`.
bool DoesRulePartyMatch(const flat::RequestFilterRule& rule,
                        bool is_third_party) {
  if (is_third_party && !(rule.options() & flat::OptionFlag_THIRD_PARTY)) {
    return false;
  }
  if (!is_third_party && !(rule.options() & flat::OptionFlag_FIRST_PARTY)) {
    return false;
  }

  return true;
}
// Returns whether the request matches flags of the specified filtering `rule`.
// Takes into account:
//  - `resource_type` of the requested resource, if not *_NONE.
//  - Whether the resource `is_third_party` w.r.t. its embedding document.
bool DoesRuleFlagsMatch(const flat::RequestFilterRule& rule,
                        flat::ResourceType resource_type,
                        bool is_third_party) {
  DCHECK(resource_type != flat::ResourceType_NONE);

  if (resource_type != flat::ResourceType_ANY &&
      !(rule.resource_types() & resource_type)) {
    return false;
  }

  return DoesRulePartyMatch(rule, is_third_party);
}

bool DoesUrlMatchRulePattern(const flat::RequestFilterRule& rule,
                             const RulePatternMatcher::UrlInfo& url) {
  if (rule.host() && rule.host()->size()) {
    std::string_view host = url.spec().substr(url.host().begin, url.host().len);
    if (!base::EndsWith(host, rule.host()->str()))
      return false;
    host.remove_suffix(rule.host()->size());
    if (!host.empty() && host.back() != '.')
      return false;
  }

  if (rule.pattern_type() == flat::PatternType_REGEXP) {
    std::string_view text =
        ((rule.options() & flat::OptionFlag_IS_CASE_SENSITIVE) != 0)
            ? url.spec()
            : url.fold_case_spec();

    return RE2::PartialMatch(
        re2::StringPiece(text.data(), text.size()),
        re2::StringPiece(rule.pattern()->data(), rule.pattern()->size()));
  } else {
    return RulePatternMatcher(rule).MatchesUrl(url);
  }
}

// Returns the size of the longest (sub-)domain of `origin` matching one of the
// `domains` in the list.
//
// The `domains` should be sorted in descending order of their length, and
// ascending alphabetical order within the groups of same-length domains.
size_t GetLongestMatchingSubdomain(const url::Origin& origin,
                                   const FlatStringList& domains) {
  // If the `domains` list is short, then the simple strategy is usually faster.
  if (domains.size() <= 5) {
    for (auto* domain : domains) {
      const std::string_view domain_piece = ToStringPiece(domain);
      if (origin.DomainIs(domain_piece))
        return domain_piece.size();
    }
    return 0;
  }
  // Otherwise look for each subdomain of the `origin` using binary search.

  DCHECK(!origin.opaque());
  std::string_view canonicalized_host(origin.host());
  if (canonicalized_host.empty())
    return 0;

  // If the host name ends with a dot, then ignore it.
  if (canonicalized_host.back() == '.')
    canonicalized_host.remove_suffix(1);

  // The `left` bound of the search is shared between iterations, because
  // subdomains are considered in decreasing order of their lengths, therefore
  // each consecutive lower_bound will be at least as far as the previous.
  flatbuffers::uoffset_t left = 0;
  for (size_t position = 0;; ++position) {
    const std::string_view subdomain = canonicalized_host.substr(position);

    flatbuffers::uoffset_t right = domains.size();
    while (left + 1 < right) {
      auto middle = left + (right - left) / 2;
      DCHECK_LT(middle, domains.size());
      if (SizePrioritizedStringCompare(ToStringPiece(domains[middle]),
                                       subdomain) <= 0)
        left = middle;
      else
        right = middle;
    }

    DCHECK_LT(left, domains.size());
    if (ToStringPiece(domains[left]) == subdomain)
      return subdomain.size();

    position = canonicalized_host.find('.', position);
    if (position == std::string_view::npos)
      break;
  }

  return 0;
}

// Returns whether the `origin` matches the domain list of the `rule`. A match
// means that the longest domain in `domains` that `origin` is a sub-domain of
// is not an exception OR all the `domains` are exceptions and neither matches
// the `origin`. Thus, domain filters with more domain components trump filters
// with fewer domain components, i.e. the more specific a filter is, the higher
// the priority.
//
// A rule whose domain list is empty or contains only negative domains is still
// considered a "generic" rule. Therefore, if `disable_generic_rules` is set,
// this function will always return false for such rules if they are simple
// modify rules.
bool DoesOriginMatchDomainList(const url::Origin& origin,
                               const flat::RequestFilterRule& rule,
                               bool disable_generic_rules) {
  const bool is_generic = !rule.domains_included();
  DCHECK(is_generic || rule.domains_included()->size());
  if (disable_generic_rules && is_generic &&
      rule.decision() == flat::Decision_MODIFY)
    return false;

  // Unique `origin` matches lists of exception domains only.
  if (origin.opaque())
    return is_generic;

  size_t longest_matching_included_domain_length = 1;
  if (!is_generic) {
    longest_matching_included_domain_length =
        GetLongestMatchingSubdomain(origin, *rule.domains_included());
  }
  if (longest_matching_included_domain_length && rule.domains_excluded()) {
    return GetLongestMatchingSubdomain(origin, *rule.domains_excluded()) <
           longest_matching_included_domain_length;
  }
  return !!longest_matching_included_domain_length;
}

RulesIndex::RuleAndSource GetRequestFilterRuleAndSourceFromId(
    const RulesIndex::RulesBufferMap& rule_buffers,
    const flat::RuleId& rule_id) {
  auto rule_buffer = rule_buffers.find(rule_id.source_id());
  DCHECK(rule_buffer != rule_buffers.end());
  return RulesIndex::RuleAndSource{
      rule_buffer->second.rules_list()->request_filter_rules_list()->Get(
          rule_id.rule_nr()),
      rule_id.source_id()};
}

const flat::CosmeticRule* GetCosmeticRuleFromId(
    const RulesIndex::RulesBufferMap& rule_buffers,
    const flat::RuleId& rule_id) {
  auto rule_buffer = rule_buffers.find(rule_id.source_id());
  DCHECK(rule_buffer != rule_buffers.end());
  return rule_buffer->second.rules_list()->cosmetic_rules_list()->Get(
      rule_id.rule_nr());
}

const flat::ScriptletInjectionRule* GetScriptletInjectionRuleFromId(
    const RulesIndex::RulesBufferMap& rule_buffers,
    const flat::RuleId& rule_id) {
  auto rule_buffer = rule_buffers.find(rule_id.source_id());
  DCHECK(rule_buffer != rule_buffers.end());
  return rule_buffer->second.rules_list()
      ->scriptlet_injection_rules_list()
      ->Get(rule_id.rule_nr());
}

bool AddActivationsFromRule(RulesIndex::ActivationResults& activations,
                            RulesIndex::RuleAndSource rule_and_source) {
  bool result = false;
  for (uint8_t i = 1; i < flat::ActivationType_ANY; i <<= 1) {
    if ((rule_and_source.rule->activation_types() & i) == 0)
      continue;

    auto& existing = activations[flat::ActivationType(i)];

    // At this stage, we have not yet checked for anything else than matches,
    // so the type can't be anything else.
    CHECK(existing.type == RulesIndex::ActivationResult::MATCH);

    if (existing.rule_and_source &&
        GetRulePriority(*existing.rule_and_source->rule) >
            GetRulePriority(*rule_and_source.rule)) {
      continue;
    }
    existing.rule_and_source = rule_and_source;
    result = true;
  }

  return result;
}

void GetActivationsFromCandidates(
    const FlatRulesByModifierList* candidates,
    const RulesIndex::RulesBufferMap& rule_buffers,
    const RulePatternMatcher::UrlInfo& url,
    const url::Origin& document_origin,
    bool is_third_party,
    RulesIndex::ActivationResults* activations) {
  // This is used for activations. All rules are expected to be grouped
  // together, regardless of modifier.
  CHECK(candidates->size() == 1);

  for (const flat::RuleId* rule_id : *candidates->Get(0)->rules()) {
    DCHECK_NE(rule_id, nullptr);
    RulesIndex::RuleAndSource rule_and_source =
        GetRequestFilterRuleAndSourceFromId(rule_buffers, *rule_id);
    const flat::RequestFilterRule& rule = *rule_and_source.rule;

    RulesIndex::ActivationResults modified_activations = *activations;
    // Avoid expensive tests if the rule wouldn't change anything.
    if (!AddActivationsFromRule(modified_activations, rule_and_source)) {
      continue;
    }

    if (!DoesRulePartyMatch(rule, is_third_party)) {
      continue;
    }

    if (!DoesOriginMatchDomainList(document_origin, rule, false)) {
      continue;
    }

    if (!DoesUrlMatchRulePattern(rule, url)) {
      continue;
    }

    std::swap(*activations, modified_activations);
  }

  return;
}

// `sorted_candidates` is sorted by GetRulePriority. This returns the first
// matching rule in `sorted_candidates` or null if no rule matches.
std::optional<RulesIndex::RuleAndSource> FindMatchAmongCandidates(
    const FlatRulesByModifierList* sorted_candidates_by_modifier,
    const RulesIndex::RulesBufferMap& rule_buffers,
    const RulePatternMatcher::UrlInfo& url,
    const url::Origin& document_origin,
    flat::ResourceType resource_type,
    bool is_third_party,
    bool disable_generic_rules,
    RulesIndex::AdAttributionMatches ad_attribution_matches,
    int current_rule_priority) {
  // This is used for request blocking. All rules are expected to be grouped
  // together, regardless of modifier.
  CHECK(sorted_candidates_by_modifier->size() == 1);

  const FlatRuleIdList* sorted_candidates =
      sorted_candidates_by_modifier->Get(0)->rules();

  DCHECK(std::is_sorted(
      sorted_candidates->begin(), sorted_candidates->end(),
      [rule_buffers](const flat::RuleId* lhs, const flat::RuleId* rhs) {
        DCHECK(lhs);
        DCHECK(rhs);
        return GetRulePriority(
                   *GetRequestFilterRuleAndSourceFromId(rule_buffers, *lhs)
                        .rule) >
               GetRulePriority(
                   *GetRequestFilterRuleAndSourceFromId(rule_buffers, *rhs)
                        .rule);
      }));

  for (const flat::RuleId* rule_id : *sorted_candidates) {
    DCHECK_NE(rule_id, nullptr);
    RulesIndex::RuleAndSource rule_and_source =
        GetRequestFilterRuleAndSourceFromId(rule_buffers, *rule_id);
    const flat::RequestFilterRule& rule = *rule_and_source.rule;
    if (current_rule_priority >= GetRulePriority(*rule_and_source.rule))
      return std::nullopt;

    if (!DoesRuleFlagsMatch(rule, resource_type, is_third_party)) {
      continue;
    }

    if (!DoesOriginMatchDomainList(document_origin, rule,
                                   disable_generic_rules)) {
      continue;
    }

    if (!DoesUrlMatchRulePattern(rule, url)) {
      continue;
    }

    if (rule.ad_domains_and_query_triggers()) {
      bool query_and_trigger_match = false;
      for (const auto* ad_domain_and_query_trigger :
           *rule.ad_domains_and_query_triggers()) {
        query_and_trigger_match = ad_attribution_matches.Run(
            url.fold_case_spec(), ad_domain_and_query_trigger->string_view());
        if (query_and_trigger_match) {
          break;
        }
      }

      if (!query_and_trigger_match) {
        continue;
      }
    }

    return rule_and_source;
  }

  return std::nullopt;
}

// `sorted_candidates` is sorted with by GetRulePriority. The modifier value of
// matching rules are stored in `result` based on modifier type. For a given
// modifier value, only the rule with highest priority is stored for a given
// modifier value gets stored. If a pass all rule is encountered, it is stored
// separately and no further tule gets entered for that type.
void FindModifierRulesMatchesCandidates(
    const FlatRulesByModifierList* sorted_candidates_by_modifier,
    const RulesIndex::RulesBufferMap& rule_buffers,
    const RulePatternMatcher::UrlInfo& url,
    const url::Origin& document_origin,
    flat::ResourceType resource_type,
    bool is_third_party,
    bool disable_generic_rules,
    RulesIndex::FoundModifiersByType& result) {
  for (const flat::PrioritizedRuleList* sorted_candidates_list :
       *sorted_candidates_by_modifier) {
    const FlatRuleIdList* sorted_candidates = sorted_candidates_list->rules();

    DCHECK(std::is_sorted(
        sorted_candidates->begin(), sorted_candidates->end(),
        [rule_buffers](const flat::RuleId* lhs, const flat::RuleId* rhs) {
          DCHECK(lhs);
          DCHECK(rhs);
          return GetRulePriority(
                     *GetRequestFilterRuleAndSourceFromId(rule_buffers, *lhs)
                          .rule) >
                 GetRulePriority(
                     *GetRequestFilterRuleAndSourceFromId(rule_buffers, *rhs)
                          .rule);
        }));

    for (const flat::RuleId* rule_id : *sorted_candidates) {
      CHECK_NE(rule_id, nullptr);
      RulesIndex::RuleAndSource rule_and_source =
          GetRequestFilterRuleAndSourceFromId(rule_buffers, *rule_id);
      const flat::RequestFilterRule& rule = *rule_and_source.rule;

      CHECK(rule.modifier() != flat::Modifier_NO_MODIFIER);

      RulesIndex::FoundModifiers& found = result[rule.modifier()];
      if (rule.decision() == flat::Decision_MODIFY) {
        found.found_modify_rules = true;
      }
      if (found.pass_all_rule &&
          rule_and_source.rule->decision() <= flat::Decision_PASS) {
        break;
      }

      if (!IsFullModifierPassRule(rule)) {
        for (const auto* modifer_value : *rule.modifier_values()) {
          auto existing = found.value_with_decision.find(modifer_value->str());
          if (existing != found.value_with_decision.end() &&
              GetRulePriority(*existing->second.rule) >
                  GetRulePriority(*rule_and_source.rule)) {
            continue;
          }
        }
      }

      if (!DoesRuleFlagsMatch(rule, resource_type, is_third_party)) {
        continue;
      }

      if (!DoesOriginMatchDomainList(document_origin, rule,
                                     disable_generic_rules)) {
        continue;
      }

      if (!DoesUrlMatchRulePattern(rule, url)) {
        continue;
      }

      if (IsFullModifierPassRule(rule)) {
        found.pass_all_rule = rule_and_source;
        std::erase_if(found.value_with_decision,
                      [](const auto& value_with_decision) {
                        return value_with_decision.second.rule->decision() !=
                               flat::Decision_MODIFY_IMPORTANT;
                      });
      } else {
        for (const auto* modifier_value : *rule.modifier_values()) {
          found.value_with_decision[modifier_value->str()] = rule_and_source;
        }
      }
    }
  }
}

void FindMatchingRuleInMap(
    std::string_view url_spec,
    const flat::RulesMap* const rule_map,
    base::FunctionRef<bool(const FlatRulesByModifierList*)> callback) {
  const FlatNGramIndex* hash_table = rule_map->ngram_index();
  const flat::NGramToRules* empty_slot = rule_map->ngram_index_empty_slot();
  DCHECK_NE(hash_table, nullptr);

  NGramHashTableProber prober;

  auto ngrams = url_pattern_index::CreateNGramExtractor<
      RulesIndex::kNGramSize, uint64_t,
      url_pattern_index::NGramCaseExtraction::kCaseSensitive>(
      url_spec, [](char) { return false; });

  for (uint64_t ngram : ngrams) {
    const uint32_t slot_index = prober.FindSlot(
        ngram, hash_table->size(),
        [hash_table, empty_slot](NGram ngram, uint32_t slot_index) {
          const flat::NGramToRules* entry = hash_table->Get(slot_index);
          DCHECK_NE(entry, nullptr);
          return entry == empty_slot || entry->ngram() == ngram;
        });
    DCHECK_LT(slot_index, hash_table->size());

    const flat::NGramToRules* entry = hash_table->Get(slot_index);
    if (entry == empty_slot)
      continue;
    if (callback(entry->rules_by_modifier()))
      return;
  }

  if (rule_map->fallback_rules_by_modifier()->size() != 0) {
    callback(rule_map->fallback_rules_by_modifier());
  }
}

std::optional<uint32_t> GetSubdomainNodeIndex(
    std::string_view domain_piece,
    const flatbuffers::Vector<
        flatbuffers::Offset<flat::ContentInjectionRulesNode>>* tree,
    const flat::ContentInjectionRulesNode* node) {
  if (!node->subdomains())
    return std::nullopt;

  auto compare = [](const flatbuffers::String* lhs,
                    const std::string_view& rhs) {
    std::string lhs_str = lhs->str();
    return std::lexicographical_compare(lhs_str.begin(), lhs_str.end(),
                                        rhs.begin(), rhs.end());
  };

  const auto& subdomain =
      std::lower_bound(node->subdomains()->begin(), node->subdomains()->end(),
                       domain_piece, compare);
  if (subdomain == node->subdomains()->end())
    return std::nullopt;

  std::string subdomain_str = subdomain->str();
  if (!std::equal(subdomain_str.begin(), subdomain_str.end(),
                  domain_piece.begin(), domain_piece.end()))
    return std::nullopt;

  DCHECK((subdomain - node->subdomains()->begin()) +
             node->first_child_node_index() <
         tree->size());

  return (subdomain - node->subdomains()->begin()) +
         node->first_child_node_index();
}

void GetSelectorsForDomain(
    const RulesIndex::RulesBufferMap& rules_buffers,
    std::vector<std::string_view>::const_reverse_iterator domain_piece,
    std::vector<std::string_view>::const_reverse_iterator domain_end,
    ContentInjectionIndexTraversalResults* results,
    const flatbuffers::Vector<
        flatbuffers::Offset<flat::ContentInjectionRulesNode>>* tree,
    const flat::ContentInjectionRulesNode* node) {
  for (const auto* rule_for_domain : *node->rules()) {
    switch (rule_for_domain->rule_type()) {
      case flat::ContentInjectionRuleType_COSMETIC: {
        const flat::CosmeticRule* rule =
            GetCosmeticRuleFromId(rules_buffers, *rule_for_domain->rule_id());
        if (rule_for_domain->allow_for_domain()) {
          results->cosmetic_rules.selected.erase(rule);
          results->cosmetic_rules.exceptions.insert(rule);
        } else if (!results->cosmetic_rules.exceptions.count(rule)) {
          results->cosmetic_rules.selected.insert(rule);
        }
        break;
      }
      case flat::ContentInjectionRuleType_SCRIPTLET_INJECTION: {
        const flat::ScriptletInjectionRule* rule =
            GetScriptletInjectionRuleFromId(rules_buffers,
                                            *rule_for_domain->rule_id());
        if (rule_for_domain->allow_for_domain()) {
          results->scriptlet_injection_rules.selected.erase(rule);
          results->scriptlet_injection_rules.exceptions.insert(rule);
        } else if (!results->scriptlet_injection_rules.exceptions.count(rule)) {
          results->scriptlet_injection_rules.selected.insert(rule);
        }
        break;
      }
    }
  }

  if (domain_piece == domain_end)
    return;

  std::optional<uint32_t> subdomain_node_index =
      GetSubdomainNodeIndex(*domain_piece++, tree, node);

  if (!subdomain_node_index)
    return;

  GetSelectorsForDomain(rules_buffers, domain_piece, domain_end, results, tree,
                        tree->Get(subdomain_node_index.value()));
  return;
}
}  // namespace

// static
std::unique_ptr<RulesIndex> RulesIndex::CreateInstance(
    RulesBufferMap rules_buffers,
    std::string rules_index_buffer,
    bool* uses_all_buffers) {
  const flat::RulesIndex* const rules_index =
      flat::GetRulesIndex(rules_index_buffer.data());

  *uses_all_buffers = false;

  // Check that the index we got matches the rules for which it was built.
  for (const auto* checksum : *rules_index->sources_checksum()) {
    auto rule_buffer = rules_buffers.find(checksum->id());
    if (rule_buffer == rules_buffers.end())
      return nullptr;
    if (rule_buffer->second.checksum() != checksum->checksum()->str())
      return nullptr;
  }

  *uses_all_buffers =
      rules_index->sources_checksum()->size() == rules_buffers.size();

  return std::make_unique<RulesIndex>(
      std::move(rules_buffers), std::move(rules_index_buffer), rules_index);
}

RulesIndex::RulesIndex(
    std::map<uint32_t, const RuleBufferHolder&> rules_buffers,
    std::string rules_index_buffer,
    const flat::RulesIndex* const rules_index)
    : rules_buffers_(std::move(rules_buffers)),
      rules_index_buffer_(std::move(rules_index_buffer)),
      rules_index_(rules_index) {}

RulesIndex::~RulesIndex() {
  InvalidateActivationCache();
}

RulesIndex::ActivationResults RulesIndex::GetActivationsForFrame(
    base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
    const content::RenderFrameHost* frame,
    std::optional<GURL> url,
    std::optional<url::Origin> document_origin) {
  if (!frame)
    return RulesIndex::ActivationResults{};

  const content::RenderFrameHost* parent = frame->GetParent();

  // Populate url and document origin if they were not provided.
  if (!url) {
    url = frame->GetLastCommittedURL();
  }

  if (url && !url->is_valid()) {  // Nothing to add here if the url isn't valid.
    return RulesIndex::ActivationResults{};
  }

  if (!document_origin) {
    document_origin = parent ? parent->GetLastCommittedOrigin()
                      : url  ? url::Origin::Create(*url)
                             : frame->GetLastCommittedOrigin();
  }

  // See if we already have a cached match.
  if (!cached_activations_.contains(frame->GetProcess()->GetID())) {
    frame->GetProcess()->AddObserver(this);
  }

  auto& cached_activations =
      cached_activations_[frame->GetProcess()->GetID()][frame->GetRoutingID()];

  for (const auto& cached_activation : cached_activations) {
    if (cached_activation.IsForDocument(*document_origin, *url)) {
      return cached_activation.activations_;
    }
  }

  // Get activations local to the frame.
  ActivationResults activations;

  RulePatternMatcher::UrlInfo url_info(*url);
  bool is_third_party = IsThirdParty(*url, *document_origin);

  auto handle_matches =
      [this, &activations, &url_info, document_origin,
       is_third_party](const FlatRulesByModifierList* rule_list) {
        GetActivationsFromCandidates(rule_list, rules_buffers_, url_info,
                                     *document_origin, is_third_party,
                                     &activations);
        return false;
      };

  FindMatchingRuleInMap(url_info.fold_case_spec(),
                        rules_index_->activation_rules_map(), handle_matches);

  // Allow everything if the frame is explicitly allowed by the user.
  if (!is_origin_wanted.Run(url ? url::Origin::Create(*url)
                                : frame->GetLastCommittedOrigin())) {
    activations[flat::ActivationType_DOCUMENT].type =
        ActivationResult::ALWAYS_PASS;
  }

  // Apply relevant activation from parent frames.
  if (parent) {
    auto parent_activations = GetActivationsForFrame(is_origin_wanted, parent);

    for (const auto& [type, parent_activation] : parent_activations) {
      ActivationResult& local_activation = activations[type];
      if ((local_activation.GetDecision().value_or(flat::Decision_MODIFY) ==
           flat::Decision_MODIFY_IMPORTANT) ||
          local_activation.type == ActivationResult::ALWAYS_PASS)
        continue;

      if (parent_activation.type == ActivationResult::ALWAYS_PASS) {
        local_activation.type = parent_activation.type;
        continue;
      }

      CHECK(parent_activation.rule_and_source);

      if (!local_activation.rule_and_source ||
          GetRulePriority(*local_activation.rule_and_source->rule) <
              GetRulePriority(*parent_activation.rule_and_source->rule)) {
        local_activation.rule_and_source = parent_activation.rule_and_source;
        local_activation.type = ActivationResult::PARENT;
      }
    }
  }

  // Store result in cache
  while (cached_activations.size() >= kMaxActivationCacheSize)
    cached_activations.pop_back();

  cached_activations.emplace_front(*document_origin, *url, activations);

  return activations;
}

std::optional<RulesIndex::RuleAndSource>
RulesIndex::FindMatchingBeforeRequestRule(
    const GURL& url,
    const url::Origin& document_origin,
    flat::ResourceType resource_type,
    bool is_third_party,
    bool disable_generic_rules,
    AdAttributionMatches ad_attribution_matches) {
  std::optional<RulesIndex::RuleAndSource> result = std::nullopt;

  // Ignore URLs that are greater than the max URL length. Since those will be
  // disallowed elsewhere in the loading stack, we can save compute time by
  // avoiding matching here.
  if (!url.is_valid() || url.spec().length() > url::kMaxURLChars) {
    return std::nullopt;
  }
  RulePatternMatcher::UrlInfo url_info(url);

  if (resource_type == flat::ResourceType_NONE)
    return result;

  auto handle_matches = [this, &result, &url_info, document_origin,
                         resource_type, is_third_party, disable_generic_rules,
                         ad_attribution_matches](
                            const FlatRulesByModifierList* rule_list) {
    std::optional<RulesIndex::RuleAndSource> rule_and_source =
        FindMatchAmongCandidates(rule_list, rules_buffers_, url_info,
                                 document_origin, resource_type, is_third_party,
                                 disable_generic_rules, ad_attribution_matches,
                                 result ? GetRulePriority(*result->rule) : -1);
    if (!rule_and_source)
      return false;

    if (!result)
      result = rule_and_source;
    else if (GetRulePriority(*rule_and_source->rule) >
             GetRulePriority(*result->rule))
      result = rule_and_source;
    return GetRulePriority(*result->rule) == GetMaxRulePriority();
  };

  FindMatchingRuleInMap(url_info.fold_case_spec(),
                        rules_index_->before_request_map(), handle_matches);

  return result;
}

RulesIndex::FoundModifiersByType RulesIndex::FindMatchingModifierRules(
    ModifierCategory category,
    const GURL& url,
    const url::Origin& document_origin,
    flat::ResourceType resource_type,
    bool is_third_party,
    bool disable_generic_rules) {
  const flat::RulesMap* rule_map = [this, category]() {
    switch (category) {
      case kBlockedRequest:
        return rules_index_->blocked_request_modifiers();
      case kAllowedRequest:
        return rules_index_->allowed_request_modifiers();
      case kHeadersReceived:
        return rules_index_->headers_received_map();
    }
  }();

  RulesIndex::FoundModifiersByType result;
  RulePatternMatcher::UrlInfo url_info(url);

  auto handle_matches =
      [this, &result, &url_info, document_origin, resource_type, is_third_party,
       disable_generic_rules](const FlatRulesByModifierList* rule_list) {
        FindModifierRulesMatchesCandidates(
            rule_list, rules_buffers_, url_info, document_origin, resource_type,
            is_third_party, disable_generic_rules, result);

        return false;
      };

  FindMatchingRuleInMap(url_info.fold_case_spec(), rule_map, handle_matches);

  return result;
}

std::string RulesIndex::GetDefaultStylesheet() {
  return rules_index_->default_stylesheet()->str();
}

RulesIndex::InjectionData::InjectionData() = default;
RulesIndex::InjectionData::InjectionData(InjectionData&& other) = default;
RulesIndex::InjectionData::~InjectionData() = default;
RulesIndex::InjectionData& RulesIndex::InjectionData::operator=(
    InjectionData&& other) = default;

RulesIndex::InjectionData RulesIndex::GetInjectionDataForOrigin(
    const url::Origin& origin,
    bool disable_generic_rules) {
  ContentInjectionIndexTraversalResults results;

  const auto* tree = rules_index_->content_injection_rules_tree();

  const flat::ContentInjectionRulesNode* root =
      tree->Get(rules_index_->content_injection_rule_tree_root_index());

  for (const auto* rule_for_domain : *root->rules()) {
    switch (rule_for_domain->rule_type()) {
      case flat::ContentInjectionRuleType_COSMETIC: {
        const flat::CosmeticRule* rule =
            GetCosmeticRuleFromId(rules_buffers_, *rule_for_domain->rule_id());
        if (rule_for_domain->allow_for_domain())
          results.cosmetic_rules.exceptions.insert(rule);
        else if (!disable_generic_rules)
          results.cosmetic_rules.selected.insert(rule);
        break;
      }
      case flat::ContentInjectionRuleType_SCRIPTLET_INJECTION: {
        const flat::ScriptletInjectionRule* rule =
            GetScriptletInjectionRuleFromId(rules_buffers_,
                                            *rule_for_domain->rule_id());
        if (rule_for_domain->allow_for_domain())
          results.scriptlet_injection_rules.exceptions.insert(rule);
        else if (!disable_generic_rules)
          results.scriptlet_injection_rules.selected.insert(rule);
        break;
      }
    }
  }

  if (origin.host().empty())
    return results.ToInjectionData();

  const auto domain_pieces = base::SplitStringPiece(
      origin.host(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::optional<uint32_t> subdomain_node_index = GetSubdomainNodeIndex(
      domain_pieces.back(), rules_index_->content_injection_rules_tree(), root);

  if (subdomain_node_index)
    GetSelectorsForDomain(rules_buffers_, domain_pieces.rbegin() + 1,
                          domain_pieces.rend(), &results, tree,
                          tree->Get(subdomain_node_index.value()));

  return results.ToInjectionData();
}

void RulesIndex::InvalidateActivationCache() {
  for (auto& [host, _] : cached_activations_) {
    content::RenderProcessHost::FromID(host)->RemoveObserver(this);
  }

  cached_activations_.clear();
}

void RulesIndex::RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  cached_activations_.erase(host->GetID());

  host->RemoveObserver(this);
}

RulesIndex::CachedActivation::CachedActivation() = default;
RulesIndex::CachedActivation::CachedActivation(
    const url::Origin& document_origin,
    const GURL& url,
    ActivationResults activations)
    : document_origin_(document_origin), url_(url), activations_(activations) {}
RulesIndex::CachedActivation::CachedActivation(
    const CachedActivation& activation) = default;
RulesIndex::CachedActivation::~CachedActivation() = default;
bool RulesIndex::CachedActivation::IsForDocument(
    const url::Origin& document_origin,
    const GURL& url) const {
  DCHECK(url.is_valid());
  // Opaque origins give the same lookup result, so they are equal for this
  // purpose)
  return url_ == url &&
         (document_origin_ == document_origin ||
          (document_origin_.opaque() && document_origin.opaque()));
}
}  // namespace adblock_filter
