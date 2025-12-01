// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULES_INDEX_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULES_INDEX_H_

#include <array>
#include <bit>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/ad_blocker/content/adblock_rules_index_manager.h"
#include "components/ad_blocker/content/utils.h"
#include "content/public/browser/child_process_id.h"
#include "content/public/browser/render_process_host_observer.h"
#include "vivaldi/components/ad_blocker/content/flat/adblock_rules_list_generated.h"

namespace content {
class RenderFrameHost;
}

namespace url {
class Origin;
}

namespace adblock_filter {
namespace flat {
struct RulesIndex;
}

class RulesIndex {
 public:
  static constexpr size_t kNGramSize = 5;

  using RulesBufferMap = std::map<uint32_t, const RuleBufferHolder&>;
  using ScriptletInjection = std::pair<std::string, std::vector<std::string>>;
  using AdAttributionMatches = base::RepeatingCallback<bool(
      std::string_view tracker_url_spec,
      std::string_view allowed_domain_and_query_param)>;

  enum ModifierCategory { kBlockedRequest, kAllowedRequest, kHeadersReceived };

  struct RuleAndSource {
    raw_ptr<const flat::RequestFilterRule> rule;
    uint32_t source_id;
  };

  struct ActivationResult {
    struct RuleDetails {
      flat::Decision decision;
      uint32_t source_id;
    };
    bool from_parent = false;

    std::optional<RuleDetails> rule_details;
    int priority = -1;

    bool IsDecision(flat::Decision match_decision) const {
      return rule_details && rule_details->decision == match_decision;
    }
  };
  struct ActivationResults {
    bool document_exception = false;
    base::flat_map<flat::ActivationType, ActivationResult> by_type;

    bool IsDocumentDecision(flat::Decision decision) {
      return document_exception
                 ? decision == flat::Decision_PASS
                 : by_type[flat::ActivationType_DOCUMENT].IsDecision(decision);
    }
  };

  struct FoundModifiers {
    std::map<std::string, RuleAndSource> value_with_decision;
    std::optional<RuleAndSource> pass_all_rule;
    bool found_modify_rules = false;
  };

  using FoundModifiersByType =
      std::array<FoundModifiers, flat::Modifier::Modifier_MAX + 1>;

  struct InjectionData {
    InjectionData();
    InjectionData(InjectionData&& other);
    ~InjectionData();
    InjectionData& operator=(InjectionData&& other);

    std::string stylesheet;
    std::vector<ScriptletInjection> scriptlet_injections;
  };

  static std::unique_ptr<RulesIndex> CreateInstance(
      std::map<uint32_t, const RuleBufferHolder&> rules_buffers,
      std::string rules_index_buffer,
      bool* uses_all_buffers);

  RulesIndex(RulesBufferMap rules_buffers,
             std::string rules_index_buffer,
             const flat::RulesIndex* const rules_index);
  ~RulesIndex();
  RulesIndex(const RulesIndex&) = delete;
  RulesIndex& operator=(const RulesIndex&) = delete;

  ActivationResults FindActivations(
      base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
      const url::Origin& parent_origin,
      const GURL& url);

  std::optional<RuleAndSource> FindMatchingBeforeRequestRule(
      const GURL& url,
      bool must_intersect_host,
      const url::Origin& document_origin,
      flat::ResourceType resource_type,
      const PartyMatcher& party_matcher,
      bool disable_generic_rules,
      std::optional<AdAttributionMatches> ad_attribution_matches);

  FoundModifiersByType FindMatchingModifierRules(
      ModifierCategory category,
      const GURL& url,
      const url::Origin& document_origin,
      flat::ResourceType resource_type,
      const PartyMatcher& party_matcher,
      bool disable_generic_rules);

  std::string GetDefaultStylesheet();

  InjectionData GetInjectionDataForOrigin(const url::Origin& origin,
                                          bool disable_generic_rules);

 private:
  RulesBufferMap rules_buffers_;
  std::string rules_index_buffer_;
  const raw_ptr<const flat::RulesIndex> rules_index_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULES_INDEX_H_
