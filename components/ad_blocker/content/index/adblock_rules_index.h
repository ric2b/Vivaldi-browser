// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULES_INDEX_H_
#define COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULES_INDEX_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/enum_array.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/containers/lru_cache.h"
#include "components/ad_blocker/content/index/adblock_rules_index_manager.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"

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
  class BaseQuery {
   public:
    virtual ~BaseQuery();

    virtual const GURL& GetUrl() const = 0;
    virtual const url::Origin& GetOrigin() const = 0;

    virtual bool IsThirdParty() const = 0;
    virtual bool IsStrictThirdParty() const = 0;
  };

  class RequestQuery : public virtual BaseQuery {
   public:
    virtual ~RequestQuery();

    virtual std::string_view GetMethod() const = 0;
    virtual bool WantsDisableGenericRules() const = 0;
  };

  struct AdAttributionMatchParams {
    std::string ad_trigger_;
    std::string ad_click_domain_;

    auto operator<=>(const AdAttributionMatchParams&) const = default;
  };

  static constexpr size_t kNGramSize = 5;

  using RulesBufferMap = std::map<uint32_t, const RuleBufferHolder&>;
  using ScriptletInjection = std::pair<std::string, std::vector<std::string>>;

  enum class ModifierCategory {
    kMin,
    kBlockedRequest = kMin,
    kAllowedRequest,
    kHeadersReceived,
    kMax = kHeadersReceived
  };

  struct FoundModifiers {
    std::map<std::string, RequestFilterRuleStub> value_with_decision;
    std::optional<RequestFilterRuleStub> pass_all_rule;
    bool found_modify_rules = false;
  };

  using FoundModifiersByType = base::EnumArray<FoundModifiers,
                                               ModifierType,
                                               ModifierType::kFirst,
                                               ModifierType::kLast>;

  struct InjectionData {
    InjectionData();
    InjectionData(InjectionData&& other);
    ~InjectionData();
    InjectionData& operator=(InjectionData&& other);

    std::string stylesheet;
    std::vector<ScriptletInjection> scriptlet_injections;
  };

  static std::unique_ptr<RulesIndex> CreateInstance(
      RulesBufferMap rules_buffers,
      std::string rules_index_buffer,
      bool* uses_all_buffers);

  RulesIndex(RulesBufferMap rules_buffers,
             std::string rules_index_buffer,
             const flat::RulesIndex* const rules_index);
  ~RulesIndex();
  RulesIndex(const RulesIndex&) = delete;
  RulesIndex& operator=(const RulesIndex&) = delete;

  const ActivationResults& FindActivations(const BaseQuery& query) const;

  const std::optional<RequestFilterRuleStub>& FindMatchingBeforeRequestRule(
      const RequestQuery& query,
      bool must_intersect_host,
      ResourceType resource_type,
      std::optional<AdAttributionMatchParams> ad_attribution_match_params)
      const;

  const FoundModifiersByType& FindMatchingModifierRules(
      ModifierCategory category,
      const RequestQuery& query,
      std::optional<ResourceType> resource_type) const;

  std::string GetDefaultStylesheet();

  InjectionData GetInjectionDataForOrigin(const url::Origin& origin,
                                          bool disable_specific_rules,
                                          bool disable_generic_rules);

 private:
  using ActivationForURLs = base::LRUCache<GURL, ActivationResults>;

  struct RequestFlagsCacheKey {
    bool disable_generic_rules = false;
    std::string method;
    std::optional<ResourceType> resource_type;

    std::optional<AdAttributionMatchParams> attribution_match_params;

    auto operator<=>(const RequestFlagsCacheKey&) const = default;
  };

  struct MatchedRulesCacheItem {
    // These two optionals have different semantics. The outer one tells whether
    // we have a cache item. The inner one is part of the cache item itself. If
    // the outer one is nullopt, we do not have data for a corresponding request
    // in the cache (we likely only have a modifier match so far). If the second
    // one is nullopt, we have data, but we know no rule is matching it.
    std::optional<std::optional<RequestFilterRuleStub>> blocking_rule;
    base::EnumSet<ModifierCategory,
                  ModifierCategory::kMin,
                  ModifierCategory::kMax>
        categories_in_cache;
    FoundModifiersByType modifiers;
  };

  // Even if we don't need ordering, we use a flat_map here since we expect
  // there will be few entries and it isn't clear how to implement absl::hash
  // for private structs
  using MatchedRulesForFlags =
      base::flat_map<RequestFlagsCacheKey, MatchedRulesCacheItem>;
  using MatchedRulesForUrl = base::LRUCache<GURL, MatchedRulesForFlags>;

  RulesBufferMap rules_buffers_;
  std::string rules_index_buffer_;
  const raw_ptr<const flat::RulesIndex> rules_index_;

  mutable base::LRUCache<url::Origin, ActivationForURLs> activations_cache_;
  mutable base::LRUCache<url::Origin, MatchedRulesForUrl> matched_rules_cache_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_AD_BLOCKER_CONTENT_ADBLOCK_RULES_INDEX_H_
