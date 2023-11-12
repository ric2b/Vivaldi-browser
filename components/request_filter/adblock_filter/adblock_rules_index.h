// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
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
  using RulesBufferMap = std::map<uint32_t, const RuleBufferHolder&>;
  static constexpr size_t kNGramSize = 5;

  struct ActivationsFound {
    ActivationsFound();
    ActivationsFound(const ActivationsFound& other);
    ~ActivationsFound();
    ActivationsFound& operator=(const ActivationsFound& other);

    bool operator==(const ActivationsFound& other) const;

    uint8_t in_block_rules = 0;
    uint8_t in_allow_rules = 0;
  };

  using ScriptletInjection = std::pair<std::string, std::vector<std::string>>;

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
      std::unique_ptr<std::string> rules_index_buffer,
      bool* uses_all_buffers);

  RulesIndex(RulesBufferMap rules_buffers,
             std::unique_ptr<std::string> rules_index_buffer,
             const flat::RulesIndex* const rules_index);
  ~RulesIndex() override;
  RulesIndex(const RulesIndex&) = delete;
  RulesIndex& operator=(const RulesIndex&) = delete;

  ActivationsFound FindMatchingActivationsRules(
      const GURL& url,
      const url::Origin& document_origin,
      bool is_third_party,
      content::RenderFrameHost* frame);

  ActivationsFound GetActivationsForFrame(
      base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
      content::RenderFrameHost* frame);

  const flat::RequestFilterRule* FindMatchingBeforeRequestRule(
      const GURL& url,
      const url::Origin& document_origin,
      flat::ResourceType resource_type,
      bool is_third_party,
      bool disable_generic_rules);

  std::vector<const flat::RequestFilterRule*> FindMatchingHeadersReceivedRules(
      const GURL& url,
      const url::Origin& document_origin,
      bool is_third_party,
      bool disable_generic_rules);

  std::string GetDefaultStylesheet();

  InjectionData GetInjectionDataForOrigin(const url::Origin& origin,
                                          bool disable_generic_rules);

  // Implementing content::RenderProcessHostObserver
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

 private:
  struct CachedActivation {
    CachedActivation();
    CachedActivation(const url::Origin& document_origin,
                     const GURL& url,
                     ActivationsFound activations);
    CachedActivation(const CachedActivation& activation);
    ~CachedActivation();

    bool IsForDocument(const url::Origin& document_origin,
                       const GURL& url) const;

    const url::Origin document_origin_;
    const GURL url_;
    const ActivationsFound activations_;
  };

  ActivationsFound GetActivationsForSingleFrame(
      base::RepeatingCallback<bool(url::Origin)> is_origin_wanted,
      content::RenderFrameHost* frame);

  RulesBufferMap rules_buffers_;
  std::string rules_index_buffer_;
  const raw_ptr<const flat::RulesIndex> rules_index_;
  std::map<int, std::map<int, base::circular_deque<CachedActivation>>>
      cached_activations_;
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RULES_INDEX_H_
