// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_H_

#include <array>
#include <bit>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "components/request_filter/adblock_filter/adblock_rules_index_manager.h"
#include "content/public/browser/render_process_host_observer.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

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

class RulesIndex : public content::RenderProcessHostObserver {
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
    enum { MATCH, PARENT, ALWAYS_PASS } type = MATCH;

    std::optional<RuleAndSource> rule_and_source;

    std::optional<flat::Decision> GetDecision() {
      if (type == ALWAYS_PASS)
        return flat::Decision_PASS;
      if (rule_and_source)
        return rule_and_source->rule->decision();
      return std::nullopt;
    }
  };
  using ActivationResults =
      base::flat_map<flat::ActivationType, ActivationResult>;

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
  ~RulesIndex() override;
  RulesIndex(const RulesIndex&) = delete;
  RulesIndex& operator=(const RulesIndex&) = delete;

  ActivationResults GetActivationsForFrame(
      base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
      const content::RenderFrameHost* frame,
      std::optional<GURL> url = std::nullopt,
      std::optional<url::Origin> document_origin = std::nullopt);

  std::optional<RuleAndSource> FindMatchingBeforeRequestRule(
      const GURL& url,
      const url::Origin& document_origin,
      flat::ResourceType resource_type,
      bool is_third_party,
      bool disable_generic_rules,
      AdAttributionMatches ad_attribution_matches);

  FoundModifiersByType FindMatchingModifierRules(
      ModifierCategory category,
      const GURL& url,
      const url::Origin& document_origin,
      flat::ResourceType resource_type,
      bool is_third_party,
      bool disable_generic_rules);

  std::string GetDefaultStylesheet();

  InjectionData GetInjectionDataForOrigin(const url::Origin& origin,
                                          bool disable_generic_rules);

  void InvalidateActivationCache();

  // Implementing content::RenderProcessHostObserver
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  struct CachedActivation {
    CachedActivation();
    CachedActivation(const url::Origin& document_origin,
                     const GURL& url,
                     ActivationResults activations);
    CachedActivation(const CachedActivation& activation);
    ~CachedActivation();

    bool IsForDocument(const url::Origin& document_origin,
                       const GURL& url) const;

    const url::Origin document_origin_;
    const GURL url_;
    const ActivationResults activations_;
  };

  RulesBufferMap rules_buffers_;
  std::string rules_index_buffer_;
  const raw_ptr<const flat::RulesIndex> rules_index_;
  std::map<int, std::map<int, base::circular_deque<CachedActivation>>>
      cached_activations_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_H_
