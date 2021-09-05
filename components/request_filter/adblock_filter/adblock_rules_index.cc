// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_rules_index.h"

#include <utility>
#include <vector>

#include "base/strings/string_split.h"
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
using NGramHasher = url_pattern_index::Uint64Hasher;
// The hash table probe sequence used both by UrlPatternIndex and its builder.
using NGramHashTableProber =
    url_pattern_index::DefaultProber<NGram, NGramHasher>;

using FlatNGramIndex =
    flatbuffers::Vector<flatbuffers::Offset<flat::NGramToRules>>;

using FlatRuleIdList = flatbuffers::Vector<flatbuffers::Offset<flat::RuleId>>;

using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;
using FlatDomains = flatbuffers::Vector<FlatStringOffset>;

const size_t kMaxActivationCacheSize = 4;

// Returns whether the request matches the third-party of the specified
// filtering|rule|.
bool DoesRulePartyMatch(const flat::FilterRule& rule, bool is_third_party) {
  if (is_third_party && !(rule.options() & flat::OptionFlag_THIRD_PARTY)) {
    return false;
  }
  if (!is_third_party && !(rule.options() & flat::OptionFlag_FIRST_PARTY)) {
    return false;
  }

  return true;
}
// Returns whether the request matches flags of the specified filtering|rule|.
// Takes into account:
//  - |resource_type| of the requested resource, if not *_NONE.
//  - Whether the resource |is_third_party| w.r.t. its embedding document.
bool DoesRuleFlagsMatch(const flat::FilterRule& rule,
                        flat::ResourceType resource_type,
                        bool is_third_party) {
  DCHECK(resource_type != flat::ResourceType_NONE);

  if (!(rule.resource_types() & resource_type)) {
    return false;
  }

  return DoesRulePartyMatch(rule, is_third_party);
}

bool DoesUrlMatchRulePattern(const flat::FilterRule& rule,
                             const RulePatternMatcher::UrlInfo& url) {
  if (rule.host() && rule.host()->size()) {
    base::StringPiece host =
        url.spec().substr(url.host().begin, url.host().len);
    if (!host.ends_with(rule.host()->str()))
      return false;
    host.remove_suffix(rule.host()->size());
    if (!host.empty() && host.back() != '.')
      return false;
  }

  if (rule.pattern_type() == flat::PatternType_REGEXP) {
    base::StringPiece text =
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

// Returns the size of the longest (sub-)domain of |origin| matching one of the
// |domains| in the list.
//
// The |domains| should be sorted in descending order of their length, and
// ascending alphabetical order within the groups of same-length domains.
size_t GetLongestMatchingSubdomain(const url::Origin& origin,
                                   const FlatDomains& domains) {
  // If the |domains| list is short, then the simple strategy is usually faster.
  if (domains.size() <= 5) {
    for (auto* domain : domains) {
      const base::StringPiece domain_piece = ToStringPiece(domain);
      if (origin.DomainIs(domain_piece))
        return domain_piece.size();
    }
    return 0;
  }
  // Otherwise look for each subdomain of the |origin| using binary search.

  DCHECK(!origin.opaque());
  base::StringPiece canonicalized_host(origin.host());
  if (canonicalized_host.empty())
    return 0;

  // If the host name ends with a dot, then ignore it.
  if (canonicalized_host.back() == '.')
    canonicalized_host.remove_suffix(1);

  // The |left| bound of the search is shared between iterations, because
  // subdomains are considered in decreasing order of their lengths, therefore
  // each consecutive lower_bound will be at least as far as the previous.
  flatbuffers::uoffset_t left = 0;
  for (size_t position = 0;; ++position) {
    const base::StringPiece subdomain = canonicalized_host.substr(position);

    flatbuffers::uoffset_t right = domains.size();
    while (left + 1 < right) {
      auto middle = left + (right - left) / 2;
      DCHECK_LT(middle, domains.size());
      if (CompareDomains(ToStringPiece(domains[middle]), subdomain) <= 0)
        left = middle;
      else
        right = middle;
    }

    DCHECK_LT(left, domains.size());
    if (ToStringPiece(domains[left]) == subdomain)
      return subdomain.size();

    position = canonicalized_host.find('.', position);
    if (position == base::StringPiece::npos)
      break;
  }

  return 0;
}

// Returns whether the |origin| matches the domain list of the |rule|. A match
// means that the longest domain in |domains| that |origin| is a sub-domain of
// is not an exception OR all the |domains| are exceptions and neither matches
// the |origin|. Thus, domain filters with more domain components trump filters
// with fewer domain components, i.e. the more specific a filter is, the higher
// the priority.
//
// A rule whose domain list is empty or contains only negative domains is still
// considered a "generic" rule. Therefore, if |disable_generic_rules| is set,
// this function will always return false for such rules.
bool DoesOriginMatchDomainList(const url::Origin& origin,
                               const flat::FilterRule& rule,
                               bool disable_generic_rules) {
  const bool is_generic = !rule.domains_included();
  DCHECK(is_generic || rule.domains_included()->size());
  if (disable_generic_rules && is_generic)
    return false;

  // Unique |origin| matches lists of exception domains only.
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

const flat::FilterRule* GetRuleFromId(
    const RulesIndex::RulesBufferMap& rule_buffers,
    const flat::RuleId& rule_id) {
  auto rule_buffer = rule_buffers.find(rule_id.source_id());
  DCHECK(rule_buffer != rule_buffers.end());
  return rule_buffer->second.rules_list()->filter_rules_list()->Get(
      rule_id.rule_nr());
}

const flat::CosmeticRule* GetCosmeticRuleFromId(
    const RulesIndex::RulesBufferMap& rule_buffers,
    const flat::RuleId& rule_id) {
  auto rule_buffer = rule_buffers.find(rule_id.source_id());
  DCHECK(rule_buffer != rule_buffers.end());
  return rule_buffer->second.rules_list()->cosmetic_rules_list()->Get(
      rule_id.rule_nr());
}

RulesIndex::ActivationsFound AddActivationsFromRule(
    RulesIndex::ActivationsFound activations,
    const flat::FilterRule& rule) {
  if ((rule.options() & flat::OptionFlag_IS_ALLOW_RULE) == 0)
    activations.in_block_rules &= rule.activation_types();
  else
    activations.in_allow_rules &= rule.activation_types();
  return activations;
}

void GetActivationsFromCandidates(
    const FlatRuleIdList* candidates,
    const RulesIndex::RulesBufferMap& rule_buffers,
    const RulePatternMatcher::UrlInfo& url,
    const url::Origin& document_origin,
    bool is_third_party,
    RulesIndex::ActivationsFound* activations_found) {
  if (!candidates)
    return;

  for (const flat::RuleId* rule_id : *candidates) {
    DCHECK_NE(rule_id, nullptr);
    const flat::FilterRule* rule = GetRuleFromId(rule_buffers, *rule_id);

    RulesIndex::ActivationsFound modified_activations_found =
        AddActivationsFromRule(*activations_found, *rule);

    if (modified_activations_found == *activations_found) {
      continue;
    }

    if (!DoesRulePartyMatch(*rule, is_third_party)) {
      continue;
    }

    if (!DoesOriginMatchDomainList(document_origin, *rule, false)) {
      continue;
    }

    if (!DoesUrlMatchRulePattern(*rule, url)) {
      continue;
    }

    *activations_found = modified_activations_found;
  }

  return;
}

// |sorted_candidates| is sorted with by GetRulePriority. This returns
// the first matching rule in |sorted_candidates| or null if no rule matches.
const flat::FilterRule* FindMatchAmongCandidates(
    const FlatRuleIdList* sorted_candidates,
    const RulesIndex::RulesBufferMap& rule_buffers,
    const RulePatternMatcher::UrlInfo& url,
    const url::Origin& document_origin,
    flat::ResourceType resource_type,
    bool is_third_party,
    bool disable_generic_rules,
    int current_rule_priority) {
  if (!sorted_candidates)
    return nullptr;

  DCHECK(std::is_sorted(
      sorted_candidates->begin(), sorted_candidates->end(),
      [rule_buffers](const flat::RuleId* lhs, const flat::RuleId* rhs) {
        DCHECK(lhs);
        DCHECK(rhs);
        return GetRulePriority(*GetRuleFromId(rule_buffers, *lhs)) >
               GetRulePriority(*GetRuleFromId(rule_buffers, *rhs));
      }));

  for (const flat::RuleId* rule_id : *sorted_candidates) {
    DCHECK_NE(rule_id, nullptr);
    const flat::FilterRule* rule = GetRuleFromId(rule_buffers, *rule_id);
    if (current_rule_priority >= GetRulePriority(*rule))
      return nullptr;

    if (!DoesRuleFlagsMatch(*rule, resource_type, is_third_party)) {
      continue;
    }

    if (!DoesOriginMatchDomainList(document_origin, *rule,
                                   disable_generic_rules)) {
      continue;
    }

    if (!DoesUrlMatchRulePattern(*rule, url)) {
      continue;
    }

    return rule;
  }

  return nullptr;
}

// |sorted_candidates| is sorted with by GetRulePriority. This returns either:
//   - All matching rules in |sorted_candidates that would modify headers.
//   - One matching blanket allow rule that denies all modifications.
//   - An empty list of rules if no match were found.
std::vector<const flat::FilterRule*> FindHeaderRulesMatchesCandidates(
    const FlatRuleIdList* sorted_candidates,
    const RulesIndex::RulesBufferMap& rule_buffers,
    const RulePatternMatcher::UrlInfo& url,
    const url::Origin& document_origin,
    bool is_third_party,
    bool disable_generic_rules) {
  std::vector<const flat::FilterRule*> result;
  if (!sorted_candidates)
    return result;

  DCHECK(std::is_sorted(
      sorted_candidates->begin(), sorted_candidates->end(),
      [rule_buffers](const flat::RuleId* lhs, const flat::RuleId* rhs) {
        DCHECK(lhs);
        DCHECK(rhs);
        return GetRulePriority(*GetRuleFromId(rule_buffers, *lhs)) >
               GetRulePriority(*GetRuleFromId(rule_buffers, *rhs));
      }));

  for (const flat::RuleId* rule_id : *sorted_candidates) {
    DCHECK_NE(rule_id, nullptr);
    const flat::FilterRule* rule = GetRuleFromId(rule_buffers, *rule_id);

    if (!DoesRulePartyMatch(*rule, is_third_party)) {
      continue;
    }

    if (!DoesOriginMatchDomainList(document_origin, *rule,
                                   disable_generic_rules)) {
      continue;
    }

    if (!DoesUrlMatchRulePattern(*rule, url)) {
      continue;
    }

    if (IsFullCSPAllowRule(*rule))
      return std::vector<const flat::FilterRule*>{rule};

    result.push_back(rule);
  }

  return result;
}

void FindMatchingRuleInMap(
    base::StringPiece url_spec,
    const flat::RulesMap* const rule_map,
    std::function<bool(const FlatRuleIdList*)> callback) {
  const FlatNGramIndex* hash_table = rule_map->ngram_index();
  const flat::NGramToRules* empty_slot = rule_map->ngram_index_empty_slot();
  DCHECK_NE(hash_table, nullptr);

  NGramHashTableProber prober;

  auto ngrams = url_pattern_index::CreateNGramExtractor<
      RulesIndex::kNGramSize, uint64_t,
      url_pattern_index::NGramCaseExtraction::kCaseSensitive>(
      url_spec, [](char) { return false; });

  for (uint64_t ngram : ngrams) {
    const size_t slot_index = prober.FindSlot(
        ngram, base::strict_cast<size_t>(hash_table->size()),
        [hash_table, empty_slot](NGram ngram, size_t slot_index) {
          const flat::NGramToRules* entry = hash_table->Get(slot_index);
          DCHECK_NE(entry, nullptr);
          return entry == empty_slot || entry->ngram() == ngram;
        });
    DCHECK_LT(slot_index, hash_table->size());

    const flat::NGramToRules* entry = hash_table->Get(slot_index);
    if (entry == empty_slot)
      continue;
    if (callback(entry->rule_list()))
      return;
  }

  callback(rule_map->fallback_rules());
}

base::Optional<uint32_t> GetSubdomainNodeIndex(
    base::StringPiece domain_piece,
    const flatbuffers::Vector<flatbuffers::Offset<flat::CosmeticRulesNode>>*
        tree,
    const flat::CosmeticRulesNode* node) {
  if (!node->subdomains())
    return base::nullopt;

  auto compare = [](const flatbuffers::String* lhs,
                    const base::StringPiece& rhs) {
    std::string lhs_str = lhs->str();
    return std::lexicographical_compare(lhs_str.begin(), lhs_str.end(),
                                        rhs.begin(), rhs.end());
  };

  const auto& subdomain =
      std::lower_bound(node->subdomains()->begin(), node->subdomains()->end(),
                       domain_piece, compare);
  if (subdomain == node->subdomains()->end())
    return base::nullopt;

  std::string subdomain_str = subdomain->str();
  if (!std::equal(subdomain_str.begin(), subdomain_str.end(),
                  domain_piece.begin(), domain_piece.end()))
    return base::nullopt;

  DCHECK((subdomain - node->subdomains()->begin()) +
             node->first_child_node_index() <
         tree->size());

  return (subdomain - node->subdomains()->begin()) +
         node->first_child_node_index();
}

void GetSelectorsForDomain(
    const RulesIndex::RulesBufferMap& rule_buffers,
    std::vector<base::StringPiece>::const_reverse_iterator domain_piece,
    std::vector<base::StringPiece>::const_reverse_iterator domain_end,
    std::set<std::string>* blocked_selectors,
    std::set<std::string>* allowed_selectors,
    const flatbuffers::Vector<flatbuffers::Offset<flat::CosmeticRulesNode>>*
        tree,
    const flat::CosmeticRulesNode* node) {
  for (const auto* rule_for_domain : *node->rules()) {
    const flat::CosmeticRule* rule =
        GetCosmeticRuleFromId(rule_buffers, *rule_for_domain->rule_id());
    std::string selector = rule->selector()->str();
    if (rule_for_domain->allow_for_domain()) {
      blocked_selectors->erase(selector);
      allowed_selectors->insert(selector);
    } else if (!allowed_selectors->count(selector)) {
      blocked_selectors->insert(selector);
    }
  }

  if (domain_piece == domain_end)
    return;

  base::Optional<uint32_t> subdomain_node_index =
      GetSubdomainNodeIndex(*domain_piece++, tree, node);

  if (!subdomain_node_index)
    return;

  GetSelectorsForDomain(rule_buffers, domain_piece, domain_end,
                        blocked_selectors, allowed_selectors, tree,
                        tree->Get(subdomain_node_index.value()));
  return;
}
}  // namespace

RulesIndex::ActivationsFound::ActivationsFound() = default;
RulesIndex::ActivationsFound::ActivationsFound(const ActivationsFound& other) =
    default;
RulesIndex::ActivationsFound::~ActivationsFound() = default;
bool RulesIndex::ActivationsFound::operator==(const ActivationsFound& other) {
  return in_allow_rules == other.in_allow_rules &&
         in_block_rules == other.in_block_rules;
}

// static
std::unique_ptr<RulesIndex> RulesIndex::CreateInstance(
    RulesBufferMap rules_buffers,
    std::unique_ptr<std::string> rules_index_buffer,
    bool* uses_all_buffers) {
  const flat::RulesIndex* const rules_index =
      flat::GetRulesIndex(rules_index_buffer->data());

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
    std::unique_ptr<std::string> rules_index_buffer,
    const flat::RulesIndex* const rules_index)
    : rules_buffers_(std::move(rules_buffers)),
      rules_index_buffer_(std::move(*rules_index_buffer)),
      rules_index_(rules_index) {}

RulesIndex::~RulesIndex() {
  for (const auto& activations : cached_activations_) {
    content::RenderProcessHost::FromID(activations.first)->RemoveObserver(this);
  }
}

RulesIndex::ActivationsFound RulesIndex::FindMatchingActivationsRules(
    const GURL& url,
    const url::Origin& document_origin,
    bool is_third_party,
    content::RenderFrameHost* frame) {
  DCHECK(frame);
  if (!cached_activations_.count(frame->GetProcess()->GetID())) {
    frame->GetProcess()->AddObserver(this);
  }

  auto& cached_activations =
      cached_activations_[frame->GetProcess()->GetID()][frame->GetRoutingID()];

  for (const auto& cached_activation : cached_activations) {
    if (cached_activation.IsForDocument(document_origin, url)) {
      return cached_activation.activations;
    }
  }

  ActivationsFound activations_found;

  RulePatternMatcher::UrlInfo url_info(url);

  auto handle_matches = [this, &activations_found, &url_info, document_origin,
                         is_third_party](const FlatRuleIdList* rule_list) {
    GetActivationsFromCandidates(rule_list, this->rules_buffers_, url_info,
                                 document_origin, is_third_party,
                                 &activations_found);
    return false;
  };

  FindMatchingRuleInMap(url_info.fold_case_spec(),
                        rules_index_->activation_rules_map(), handle_matches);

  while (cached_activations.size() >= kMaxActivationCacheSize)
    cached_activations.pop_back();

  cached_activations.emplace_front(document_origin, url, activations_found);

  return activations_found;
}

RulesIndex::ActivationsFound RulesIndex::GetActivationsForSingleFrame(
    base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
    content::RenderFrameHost* frame) {
  content::RenderFrameHost* parent = frame->GetParent();
  const GURL& url = frame->GetLastCommittedURL();
  const url::Origin& origin = parent ? parent->GetLastCommittedOrigin()
                                     : frame->GetLastCommittedOrigin();
  ActivationsFound activations_found = FindMatchingActivationsRules(
      url, origin, IsThirdParty(url, origin), frame);
  if (!is_origin_wanted.Run(origin))
    activations_found.in_allow_rules |= flat::ActivationType_DOCUMENT;
  return activations_found;
}

RulesIndex::ActivationsFound RulesIndex::GetActivationsForFrame(
    base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
    content::RenderFrameHost* frame) {
  if (!frame)
    return RulesIndex::ActivationsFound();

  RulesIndex::ActivationsFound activations =
      GetActivationsForSingleFrame(is_origin_wanted, frame);
  frame = frame->GetParent();

  // Allow activation rules should apply to all descendents. Deny activations
  // rules usually just disable the rendering of a document, so no element of
  // that document would end up checking here.
  while (frame) {
    activations.in_allow_rules |=
        GetActivationsForSingleFrame(is_origin_wanted, frame).in_allow_rules;
    frame = frame->GetParent();
  }

  return activations;
}

const flat::FilterRule* RulesIndex::FindMatchingBeforeRequestRule(
    const GURL& url,
    const url::Origin& document_origin,
    flat::ResourceType resource_type,
    bool is_third_party,
    bool disable_generic_rules) {
  const flat::FilterRule* result = nullptr;

  // Ignore URLs that are greater than the max URL length. Since those will be
  // disallowed elsewhere in the loading stack, we can save compute time by
  // avoiding matching here.
  if (!url.is_valid() || url.spec().length() > url::kMaxURLChars) {
    return nullptr;
  }
  RulePatternMatcher::UrlInfo url_info(url);

  if (resource_type == flat::ResourceType_NONE)
    return result;

  auto handle_matches =
      [this, &result, &url_info, document_origin, resource_type, is_third_party,
       disable_generic_rules](const FlatRuleIdList* rule_list) {
        const flat::FilterRule* rule = FindMatchAmongCandidates(
            rule_list, this->rules_buffers_, url_info, document_origin,
            resource_type, is_third_party, disable_generic_rules,
            result ? GetRulePriority(*result) : -1);
        if (!rule)
          return false;

        if (result == nullptr)
          result = rule;
        else if (GetRulePriority(*rule) > GetRulePriority(*result))
          result = rule;
        return GetRulePriority(*result) == GetMaxRulePriority();
      };

  FindMatchingRuleInMap(url_info.fold_case_spec(),
                        rules_index_->before_request_map(), handle_matches);

  return result;
}

std::vector<const flat::FilterRule*>
RulesIndex::FindMatchingHeadersReceivedRules(const GURL& url,
                                             const url::Origin& document_origin,
                                             bool is_third_party,
                                             bool disable_generic_rules) {
  std::vector<const flat::FilterRule*> result;

  RulePatternMatcher::UrlInfo url_info(url);

  auto handle_matches = [this, &result, &url_info, document_origin,
                         is_third_party, disable_generic_rules](
                            const FlatRuleIdList* rule_list) {
    std::vector<const flat::FilterRule*> rules =
        FindHeaderRulesMatchesCandidates(rule_list, this->rules_buffers_,
                                         url_info, document_origin,
                                         is_third_party, disable_generic_rules);

    if (rules.size() == 1 && IsFullCSPAllowRule(*rules[0])) {
      result.swap(rules);
      return true;
    }

    result.insert(result.end(), rules.begin(), rules.end());
    return false;
  };

  FindMatchingRuleInMap(url_info.fold_case_spec(),
                        rules_index_->headers_received_map(), handle_matches);

  return result;
}

std::string RulesIndex::GetDefaultStylesheet() {
  return rules_index_->default_stylesheet()->str();
}

std::string RulesIndex::GetStyleSheetForOrigin(const url::Origin& origin,
                                               bool disable_generic_rules) {
  std::set<std::string> blocked_selectors;
  std::set<std::string> allowed_selectors;

  const auto* tree = rules_index_->cosmetic_rules_tree();

  const flat::CosmeticRulesNode* root =
      tree->Get(rules_index_->cosmetic_rule_tree_root_index());

  for (const auto* rule_for_domain : *root->rules()) {
    const flat::CosmeticRule* rule = GetCosmeticRuleFromId(
        this->rules_buffers_, *rule_for_domain->rule_id());
    if (rule_for_domain->allow_for_domain())
      allowed_selectors.insert(rule->selector()->str());
    else if (!disable_generic_rules)
      blocked_selectors.insert(rule->selector()->str());
  }

  if (origin.host().empty())
    return BuildStyleSheet(blocked_selectors);

  const auto domain_pieces = base::SplitStringPiece(
      origin.host(), ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::Optional<uint32_t> subdomain_node_index = GetSubdomainNodeIndex(
      domain_pieces.back(), rules_index_->cosmetic_rules_tree(), root);

  if (subdomain_node_index)
    GetSelectorsForDomain(this->rules_buffers_, domain_pieces.rbegin() + 1,
                          domain_pieces.rend(), &blocked_selectors,
                          &allowed_selectors, tree,
                          tree->Get(subdomain_node_index.value()));

  return BuildStyleSheet(blocked_selectors);
}

void RulesIndex::RenderProcessHostDestroyed(content::RenderProcessHost* host) {
  cached_activations_.erase(host->GetID());
}

RulesIndex::CachedActivation::CachedActivation() = default;
RulesIndex::CachedActivation::CachedActivation(
    const url::Origin& document_origin,
    const GURL& url,
    ActivationsFound activations)
    : document_origin(document_origin), url(url), activations(activations) {}
RulesIndex::CachedActivation::CachedActivation(
    const CachedActivation& activation) = default;
RulesIndex::CachedActivation::~CachedActivation() = default;
bool RulesIndex::CachedActivation::IsForDocument(
    const url::Origin& document_origin,
    const GURL& url) const {
  DCHECK(url.is_valid());
  // Opaque origins give the same lookup result, so they are equal for this
  // purpose)
  return this->url == url &&
         (this->document_origin == document_origin ||
          (this->document_origin.opaque() && document_origin.opaque()));
}
}  // namespace adblock_filter
