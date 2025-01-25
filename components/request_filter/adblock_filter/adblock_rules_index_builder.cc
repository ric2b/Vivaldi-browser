// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rules_index_builder.h"

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/strings/string_split.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/stylesheet_builder.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "components/url_pattern_index/closed_hash_map.h"
#include "components/url_pattern_index/ngram_extractor.h"
#include "components/url_pattern_index/uint64_hasher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"
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

using SourceChecksumOffset = flatbuffers::Offset<flat::SourceChecksum>;
using RuleIdOffset = flatbuffers::Offset<flat::RuleId>;
using PrioritizedRuleListOffset =
    flatbuffers::Offset<flat::PrioritizedRuleList>;
using RulesMapOffset = flatbuffers::Offset<flat::RulesMap>;
using RulesIndexOffset = flatbuffers::Offset<flat::RulesIndex>;

using MutableRulesList =
    base::flat_map<flat::Modifier, std::vector<std::pair<RuleIdOffset, int>>>;
using SourceChecksums = std::vector<SourceChecksumOffset>;

using ContentInjectionTreeNodeOffset =
    flatbuffers::Offset<flat::ContentInjectionRulesNode>;
using ContentInjectionRuleTree = std::vector<ContentInjectionTreeNodeOffset>;

using MutableNGramMap = url_pattern_index::
    ClosedHashMap<NGram, MutableRulesList, NGramHashTableProber>;

static_assert(RulesIndex::kNGramSize <= sizeof(NGram),
              "NGram type is too narrow.");

struct IndexBuildData {
  MutableNGramMap map;
  MutableRulesList fallback;
};

struct RuleId {
  RuleId(uint32_t source_id, uint32_t rule_nr)
      : source_id(source_id), rule_nr(rule_nr) {}
  uint32_t source_id;
  uint32_t rule_nr;
};

struct ContentInjectionRuleForDomain {
  ContentInjectionRuleForDomain(RuleId rule_id, bool allow_for_domain)
      : rule_id(rule_id), allow_for_domain(allow_for_domain) {}
  RuleId rule_id;
  bool allow_for_domain;
};

template <class T>
struct RuleType {};

template <>
struct RuleType<flat::CosmeticRule> {
  static constexpr flat::ContentInjectionRuleType value =
      flat::ContentInjectionRuleType_COSMETIC;
};

template <>
struct RuleType<flat::ScriptletInjectionRule> {
  static constexpr flat::ContentInjectionRuleType value =
      flat::ContentInjectionRuleType_SCRIPTLET_INJECTION;
};

struct ContentInjectionRuleTreeNode {
  std::map<const flat::CosmeticRule*,
           ContentInjectionRuleForDomain,
           ContentInjectionRuleBodyCompare>
      rule_from_cosmetic_rule_body;
  std::map<const flat::ScriptletInjectionRule*,
           ContentInjectionRuleForDomain,
           ContentInjectionRuleBodyCompare>
      rule_from_scriptlet_injection_rule_body;
  std::map<std::string, ContentInjectionRuleTreeNode> subdomains;

  std::map<const flat::CosmeticRule*,
           ContentInjectionRuleForDomain,
           ContentInjectionRuleBodyCompare>&
  GetMap(const flat::CosmeticRule* rule) {
    return rule_from_cosmetic_rule_body;
  }

  std::map<const flat::ScriptletInjectionRule*,
           ContentInjectionRuleForDomain,
           ContentInjectionRuleBodyCompare>&
  GetMap(const flat::ScriptletInjectionRule* rule) {
    return rule_from_scriptlet_injection_rule_body;
  }
};

std::string GetNGramSearchString(const flat::RequestFilterRule& rule) {
  if (rule.pattern_type() == flat::PatternType_REGEXP)
    return rule.ngram_search_string()->str();
  if ((rule.options() & flat::OptionFlag_IS_CASE_SENSITIVE))
    return rule.ngram_search_string()->str();
  return rule.pattern()->str();
}

void AddRuleToMap(const flat::RequestFilterRule& rule,
                  RuleIdOffset rule_id,
                  bool ignore_modifier,
                  IndexBuildData& build_data) {
  size_t min_list_size = std::numeric_limits<size_t>::max();
  NGram best_ngram = 0;
  std::string pattern = GetNGramSearchString(rule);
  auto ngrams = url_pattern_index::CreateNGramExtractor<
      RulesIndex::kNGramSize, NGram,
      url_pattern_index::NGramCaseExtraction::kCaseSensitive>(
      pattern, [](char c) { return c == '*' || c == '^'; });

  for (uint64_t ngram : ngrams) {
    const MutableRulesList* rules = build_data.map.Get(ngram);
    const size_t list_size = rules ? rules->size() : 0;
    if (list_size < min_list_size) {
      min_list_size = list_size;
      best_ngram = ngram;
      if (list_size == 0)
        break;
    }
  }

  // For activation rules and before request rules, there is no need to take
  // modifiers into account. Group everythin in one list.
  flat::Modifier modifier =
      ignore_modifier ? flat::Modifier_NO_MODIFIER : rule.modifier();

  if (best_ngram) {
    build_data.map[best_ngram][modifier].push_back(
        std::make_pair(rule_id, GetRulePriority(rule)));
  } else {
    build_data.fallback[modifier].push_back(
        std::make_pair(rule_id, GetRulePriority(rule)));
  }
}

RulesMapOffset BuildFlatMap(flatbuffers::FlatBufferBuilder* builder,
                            IndexBuildData& build_data) {
  std::vector<flatbuffers::Offset<flat::NGramToRules>> flat_map(
      build_data.map.table_size());

  flatbuffers::Offset<flat::NGramToRules> empty_slot_offset =
      flat::CreateNGramToRules(*builder);

  auto priority_comparator = [](const std::pair<RuleIdOffset, int>& lhs,
                                const std::pair<RuleIdOffset, int>& rhs) {
    return lhs.second > rhs.second;
  };

  for (size_t i = 0, size = build_data.map.table_size(); i != size; ++i) {
    const uint32_t entry_index = build_data.map.hash_table()[i];
    if (entry_index >= build_data.map.size()) {
      flat_map[i] = empty_slot_offset;
      continue;
    }

    const MutableNGramMap::EntryType& entry =
        build_data.map.entries()[entry_index];

    std::vector<PrioritizedRuleListOffset> rule_list_by_modifier;

    // Retrieve a mutable reference to |entry.second| and sort it in descending
    // order of priority.
    for (auto& [_, rule_list_with_priority] : build_data.map[entry.first]) {
      std::vector<RuleIdOffset> rule_list;
      std::sort(rule_list_with_priority.begin(), rule_list_with_priority.end(),
                priority_comparator);

      for (const auto& rule_with_priority : rule_list_with_priority)
        rule_list.push_back(rule_with_priority.first);
      auto rules_offset = builder->CreateVector(rule_list);
      rule_list_by_modifier.push_back(
          flat::CreatePrioritizedRuleList(*builder, rules_offset));
    }
    auto rule_list_by_modifier_offset =
        builder->CreateVector(rule_list_by_modifier);
    flat_map[i] = flat::CreateNGramToRules(*builder, entry.first,
                                           rule_list_by_modifier_offset);
  }

  auto ngram_index_offset = builder->CreateVector(flat_map);

  std::vector<PrioritizedRuleListOffset> fallback_list_by_modifier;
  for (auto& [_, fallback_list_with_priority] : build_data.fallback) {
    std::sort(fallback_list_with_priority.begin(),
              fallback_list_with_priority.end(), priority_comparator);

    std::vector<RuleIdOffset> fallback_list;
    for (const auto& fallback_with_priority : fallback_list_with_priority)
      fallback_list.push_back(fallback_with_priority.first);

    auto fallback_offset = builder->CreateVector(fallback_list);
    fallback_list_by_modifier.push_back(
        flat::CreatePrioritizedRuleList(*builder, fallback_offset));
  }

  auto fallback_list_by_modifier_offset =
      builder->CreateVector(fallback_list_by_modifier);

  return flat::CreateRulesMap(*builder, RulesIndex::kNGramSize,
                              ngram_index_offset, empty_slot_offset,
                              fallback_list_by_modifier_offset);
}

std::string DoSaveIndex(base::span<const uint8_t> data,
                        const base::FilePath& index_path) {
  base::File output_file(
      index_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!output_file.IsValid())
    return std::string();

  // Write the version header.
  std::string version_header = GetIndexVersionHeader();
  int version_header_size = static_cast<int>(version_header.size());
  if (output_file.WriteAtCurrentPos(
          version_header.data(), version_header_size) != version_header_size) {
    return std::string();
  }

  // Write the flatbuffer ruleset.
  if (!base::IsValueInRangeForNumericType<int>(data.size()))
    return std::string();
  int data_size = static_cast<int>(data.size());
  if (output_file.WriteAtCurrentPos(reinterpret_cast<const char*>(data.data()),
                                    data_size) != data_size) {
    return std::string();
  }

  return CalculateBufferChecksum(data);
}

void SaveIndex(std::unique_ptr<flatbuffers::FlatBufferBuilder> index_builder,
               const base::FilePath& index_path,
               IndexSavedCallback index_saved_callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          std::move(index_saved_callback),
          DoSaveIndex(base::make_span(index_builder->GetBufferPointer(),
                                      index_builder->GetSize()),
                      index_path)));
}

template <class T>
void AddRuleToContentInjectionRulesTreeNode(
    const T* rule,
    const RuleId& rule_id,
    bool allow,
    ContentInjectionRuleTreeNode& node) {
  auto& map = node.GetMap(rule);
  // If we have two rules for the same body+domain combination, allow rules take
  // precedence. Otherwise, avoid duplicates.
  const auto& existing_rule = map.find(rule);
  if (existing_rule != map.end()) {
    if (!allow || existing_rule->second.allow_for_domain)
      return;
    map.erase(existing_rule);
  }

  map.insert({rule, ContentInjectionRuleForDomain(rule_id, allow)});
}

template <class T>
void AddRuleToContentInjectionRuleTreeNodeSubdomain(
    std::vector<std::string_view>::const_reverse_iterator domain_piece,
    std::vector<std::string_view>::const_reverse_iterator domain_end,
    const T* rule,
    const RuleId& rule_id,
    bool allow,
    ContentInjectionRuleTreeNode& node) {
  if (domain_piece == domain_end) {
    AddRuleToContentInjectionRulesTreeNode(rule, rule_id, allow, node);
    return;
  }

  std::string domain_piece_str = std::string(*(domain_piece++));
  AddRuleToContentInjectionRuleTreeNodeSubdomain(
      domain_piece, domain_end, rule, rule_id, allow,
      node.subdomains[domain_piece_str]);
}

template <class T>
void AddRuleToContentInjectionRulesTree(const T* rule,
                                        const RuleId& rule_id,
                                        ContentInjectionRuleTreeNode& root) {
  flat::ContentInjectionRuleType rule_type = RuleType<T>::value;
  // Rules without included domains are generic
  if (!rule->core()->domains_included()) {
    AddRuleToContentInjectionRulesTreeNode(rule, rule_id,
                                           rule->core()->is_allow_rule(), root);
  }

  if (rule->core()->domains_excluded()) {
    // Excluded domains for scriptlet injection allow rules should be discarded
    // at parsing time.
    DCHECK(rule_type != flat::ContentInjectionRuleType_SCRIPTLET_INJECTION ||
           !rule->core()->is_allow_rule());
    for (const auto* domain : *rule->core()->domains_excluded()) {
      std::string domain_str(domain->str());  // NOTE(jarle): Needed to fix
                                              // VB-65773.
      const auto domain_pieces = base::SplitStringPiece(
          domain_str, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      AddRuleToContentInjectionRuleTreeNodeSubdomain(
          domain_pieces.rbegin(), domain_pieces.rend(), rule, rule_id,
          !rule->core()->is_allow_rule(), root);
    }
  }

  if (rule->core()->domains_included()) {
    for (const auto* domain : *rule->core()->domains_included()) {
      std::string domain_str(domain->str());  // NOTE(jarle): Needed to fix
                                              // VB-65773.
      const auto domain_pieces = base::SplitStringPiece(
          domain_str, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      AddRuleToContentInjectionRuleTreeNodeSubdomain(
          domain_pieces.rbegin(), domain_pieces.rend(), rule, rule_id,
          rule->core()->is_allow_rule(), root);
    }
  }
}

template <class T>
void AddRuleIdsToList(
    flatbuffers::FlatBufferBuilder* builder,
    const std::map<const T*,
                   ContentInjectionRuleForDomain,
                   ContentInjectionRuleBodyCompare>& ids_map,
    std::vector<flatbuffers::Offset<flat::ContentInjectionRuleForDomain>>&
        rules_for_domain) {
  for (const auto& rule : ids_map) {
    RuleIdOffset rule_id = flat::CreateRuleId(
        *builder, rule.second.rule_id.source_id, rule.second.rule_id.rule_nr);
    rules_for_domain.push_back(flat::CreateContentInjectionRuleForDomain(
        *builder, rule_id, RuleType<T>::value, rule.second.allow_for_domain));
  }
}

void AddNodeToFlatContentInjectionRuleTree(
    flatbuffers::FlatBufferBuilder* builder,
    const ContentInjectionRuleTreeNode& node,
    std::optional<size_t> first_child_node_index,
    ContentInjectionRuleTree& tree) {
  std::vector<flatbuffers::Offset<flat::ContentInjectionRuleForDomain>>
      rules_for_domain;

  std::vector<flatbuffers::Offset<flatbuffers::String>> subdomains;

  AddRuleIdsToList(builder, node.rule_from_cosmetic_rule_body,
                   rules_for_domain);
  AddRuleIdsToList(builder, node.rule_from_scriptlet_injection_rule_body,
                   rules_for_domain);

  DCHECK(first_child_node_index || node.subdomains.empty());

  for (const auto& subdomain : node.subdomains) {
    subdomains.push_back(builder->CreateSharedString(subdomain.first));
  }

  auto rules_for_domain_offset = builder->CreateVector(rules_for_domain);
  auto subdomains_offset = builder->CreateVector(subdomains);

  tree.push_back(flat::CreateContentInjectionRulesNode(
      *builder, rules_for_domain_offset,
      first_child_node_index ? first_child_node_index.value() : UINT32_MAX,
      subdomains_offset));
}

size_t AddNodeDescendantsToFlatContentInjectionRuleTree(
    flatbuffers::FlatBufferBuilder* builder,
    const ContentInjectionRuleTreeNode& node,
    ContentInjectionRuleTree& tree) {
  std::map<const ContentInjectionRuleTreeNode*, std::optional<size_t>>
      first_child_node_index_for_children;
  for (const auto& child : node.subdomains) {
    if (!child.second.subdomains.empty())
      first_child_node_index_for_children.insert(
          {&child.second, AddNodeDescendantsToFlatContentInjectionRuleTree(
                              builder, child.second, tree)});
    else
      first_child_node_index_for_children.insert({&child.second, std::nullopt});
  }

  size_t first_child_node_index = tree.size();

  for (const auto& child : node.subdomains) {
    AddNodeToFlatContentInjectionRuleTree(
        builder, child.second,
        first_child_node_index_for_children.at(&child.second), tree);
  }

  return first_child_node_index;
}

size_t BuildFlatContentInjectionRuleTree(
    flatbuffers::FlatBufferBuilder* builder,
    const ContentInjectionRuleTreeNode& root,
    ContentInjectionRuleTree& tree) {
  size_t first_child_node_index =
      AddNodeDescendantsToFlatContentInjectionRuleTree(builder, root, tree);
  size_t root_node_index = tree.size();
  AddNodeToFlatContentInjectionRuleTree(builder, root, first_child_node_index,
                                        tree);
  return root_node_index;
}

}  // namespace

void BuildAndSaveIndex(
    const std::map<uint32_t, std::unique_ptr<RuleBufferHolder>>& rules_buffers,
    base::SequencedTaskRunner* file_task_runner,
    const base::FilePath& index_path,
    IndexSavedCallback index_saved_callback) {
  SourceChecksums source_checksums;

  IndexBuildData activation_rules;
  IndexBuildData before_request;
  IndexBuildData modify_blocked_request;
  IndexBuildData modify_allowed_request;
  IndexBuildData headers_received;

  std::unique_ptr<flatbuffers::FlatBufferBuilder> builder =
      std::make_unique<flatbuffers::FlatBufferBuilder>();

  // Generic cosmetic block rules that are not cancelled by any other rule on
  // any domain.
  std::map<const flat::CosmeticRule*, RuleId, ContentInjectionRuleBodyCompare>
      default_cosmetic_block_rules;
  // List of all rules with selectors that are potentilly unblocked on some
  // domains, used to build |default_cosmetic_block_rules|.
  std::set<const flat::CosmeticRule*, ContentInjectionRuleBodyCompare>
      cosmetic_allow_selectors;
  ContentInjectionRuleTreeNode content_injection_rules_tree;

  for (const auto& rules_buffer : rules_buffers) {
    source_checksums.push_back(flat::CreateSourceChecksum(
        *builder, rules_buffer.first,
        builder->CreateString(rules_buffer.second->checksum())));
    for (flatbuffers::uoffset_t i = 0;
         i <
         rules_buffer.second->rules_list()->request_filter_rules_list()->size();
         i++) {
      RuleIdOffset rule_id =
          flat::CreateRuleId(*builder, rules_buffer.first, i);
      const auto* rule =
          rules_buffer.second->rules_list()->request_filter_rules_list()->Get(
              i);

      DCHECK((rule->modifier() != flat::Modifier_NO_MODIFIER) ||
             rule->activation_types() != 0 ||
             (rule->options() & flat::OptionFlag_MODIFY_BLOCK));

      if (rule->activation_types() != 0)
        AddRuleToMap(*rule, rule_id, true, activation_rules);

      if (rule->options() & flat::OptionFlag_MODIFY_BLOCK) {
        CHECK(rule->resource_types() != 0);
        AddRuleToMap(*rule, rule_id, true, before_request);
      }

      switch (rule->modifier()) {
        case flat::Modifier_NO_MODIFIER:
          break;

        case flat::Modifier_CSP:
          AddRuleToMap(*rule, rule_id, false, headers_received);
          break;

        case flat::Modifier_REDIRECT:
          AddRuleToMap(*rule, rule_id, false, modify_blocked_request);
          break;

        case flat::Modifier_AD_QUERY_TRIGGER:
          AddRuleToMap(*rule, rule_id, false, modify_allowed_request);
      }
    }

    for (flatbuffers::uoffset_t i = 0;
         i < rules_buffer.second->rules_list()->cosmetic_rules_list()->size();
         i++) {
      RuleId rule_id(rules_buffer.first, i);
      const auto* rule =
          rules_buffer.second->rules_list()->cosmetic_rules_list()->Get(i);

      // Domain exclusions on block rules have the same effect as allow rules.
      if (rule->core()->is_allow_rule() || rule->core()->domains_excluded()) {
        auto matching_block = default_cosmetic_block_rules.find(rule);
        if (matching_block != default_cosmetic_block_rules.end()) {
          AddRuleToContentInjectionRulesTree(matching_block->first,
                                             matching_block->second,
                                             content_injection_rules_tree);
          default_cosmetic_block_rules.erase(matching_block);
        }
        cosmetic_allow_selectors.insert(rule);
      } else if (!rule->core()->is_allow_rule() &&
                 !rule->core()->domains_included() &&
                 !rule->core()->domains_excluded() &&
                 !cosmetic_allow_selectors.count(rule)) {
        default_cosmetic_block_rules.insert({rule, rule_id});
        continue;
      }

      AddRuleToContentInjectionRulesTree(rule, rule_id,
                                         content_injection_rules_tree);
    }

    for (flatbuffers::uoffset_t i = 0;
         i < rules_buffer.second->rules_list()
                 ->scriptlet_injection_rules_list()
                 ->size();
         i++) {
      RuleId rule_id(rules_buffer.first, i);
      const auto* rule = rules_buffer.second->rules_list()
                             ->scriptlet_injection_rules_list()
                             ->Get(i);
      AddRuleToContentInjectionRulesTree(rule, rule_id,
                                         content_injection_rules_tree);
    }
  }

  auto source_checksums_offset =
      builder->CreateVectorOfSortedTables(&source_checksums);
  RulesMapOffset activation_rules_map_offset =
      BuildFlatMap(builder.get(), activation_rules);
  RulesMapOffset before_request_map_offset =
      BuildFlatMap(builder.get(), before_request);
  RulesMapOffset modify_blocked_request_map_offset =
      BuildFlatMap(builder.get(), modify_blocked_request);
  RulesMapOffset modify_allowed_request_map_offset =
      BuildFlatMap(builder.get(), modify_allowed_request);
  RulesMapOffset headers_received_map_offset =
      BuildFlatMap(builder.get(), headers_received);

  auto default_stylesheet_offset =
      builder->CreateString(BuildStyleSheet(default_cosmetic_block_rules));

  ContentInjectionRuleTree flat_content_injection_rules_tree;
  size_t root_index = BuildFlatContentInjectionRuleTree(
      builder.get(), content_injection_rules_tree,
      flat_content_injection_rules_tree);
  CHECK(flat_content_injection_rules_tree.size() > 0);
  auto flat_content_injection_rule_tree_offset =
      builder->CreateVector(flat_content_injection_rules_tree);

  auto rule_index_offset = flat::CreateRulesIndex(
      *builder, source_checksums_offset, activation_rules_map_offset,
      before_request_map_offset, modify_blocked_request_map_offset,
      modify_allowed_request_map_offset, headers_received_map_offset,
      default_stylesheet_offset, root_index,
      flat_content_injection_rule_tree_offset);

  flat::FinishRulesIndexBuffer(*builder, rule_index_offset);

  file_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&SaveIndex, std::move(builder), index_path,
                                std::move(index_saved_callback)));
}
}  // namespace adblock_filter
