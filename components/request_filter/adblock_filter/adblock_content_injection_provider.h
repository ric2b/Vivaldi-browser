// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_CONTENT_INJECTION_PROVIDER_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_CONTENT_INJECTION_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "components/ad_blocker/adblock_types.h"
#include "components/ad_blocker/adblock_resources.h"
#include "components/content_injection/content_injection_provider.h"

namespace content {
class BrowserContext;
}

namespace adblock_filter {
class RulesIndexManager;

class ContentInjectionProvider : public content_injection::Provider,
                                 public Resources::Observer {
 public:
  ContentInjectionProvider(
      content::BrowserContext* context,
      std::array<RulesIndexManager*, kRuleGroupCount> index_managers,
      Resources* resources);
  ~ContentInjectionProvider() override;
  ContentInjectionProvider(const ContentInjectionProvider&) = delete;
  ContentInjectionProvider& operator=(const ContentInjectionProvider&) = delete;

  // Implementing content_injection::Provider
  content_injection::mojom::InjectionsForFramePtr GetInjectionsForFrame(
      const GURL& url,
      content::RenderFrameHost* frame) override;
  const std::map<std::string, content_injection::StaticInjectionItem>&
  GetStaticContent() override;

  // Implementing Resources::Observer
  void OnResourcesLoaded() override;

 private:
  void BuildStaticContent();

  const raw_ptr<content::BrowserContext> context_;
  std::array<RulesIndexManager*, kRuleGroupCount> index_managers_;
  const raw_ptr<Resources> resources_;

  std::optional<int> javascript_world_id_;
  std::map<std::string, content_injection::StaticInjectionItem> static_content_;
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_CONTENT_INJECTION_PROVIDER_H_
