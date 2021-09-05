// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_content_injection_provider.h"

#include "components/request_filter/adblock_filter/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace adblock_filter {
namespace {
bool IsOriginWanted(RuleService* service, RuleGroup group, url::Origin origin) {
  // Allow all requests made by extensions.
  if (origin.scheme() == "chrome-extension")
    return false;

  return !service->IsExemptOfFiltering(group, origin);
}
}  // namespace

ContentInjectionProvider::ContentInjectionProvider(
    RulesIndexManager* index_manager)
    : index_manager_(index_manager) {}

ContentInjectionProvider::~ContentInjectionProvider() = default;

content_injection::mojom::InjectionsForFramePtr
ContentInjectionProvider::GetInjectionsForFrame(
    const GURL& url,
    content::RenderFrameHost* frame) {
  auto result = content_injection::mojom::InjectionsForFrame::New();

  RuleService* service = RuleServiceFactory::GetForBrowserContext(
      frame->GetProcess()->GetBrowserContext());

  if (!url.SchemeIsHTTPOrHTTPS())
    return result;

  content::RenderFrameHost* parent = frame->GetParent();
  url::Origin document_origin =
      parent ? parent->GetLastCommittedOrigin() : url::Origin::Create(url);

  if (!index_manager_->rules_index() ||
      !service->IsRuleGroupEnabled(index_manager_->group()) ||
      !IsOriginWanted(service, index_manager_->group(), document_origin))
    return result;

  RulesIndex::ActivationsFound activations =
      index_manager_->rules_index()->FindMatchingActivationsRules(
          url, document_origin, IsThirdParty(url, document_origin), frame);

  activations.in_allow_rules |=
      index_manager_->rules_index()
          ->GetActivationsForFrame(
              base::Bind(&IsOriginWanted, service, index_manager_->group()),
              frame->GetParent())
          .in_allow_rules;

  if ((activations.in_allow_rules & (flat::ActivationType_DOCUMENT |
                                     flat::ActivationType_ELEMENT_HIDE)) != 0) {
    return result;
  }

  std::string stylesheet;
  if ((activations.in_allow_rules & flat::ActivationType_GENERIC_HIDE) != 0) {
    stylesheet += index_manager_->rules_index()
                      ->GetInjectionDataForOrigin(document_origin, true)
                      .stylesheet;
  } else {
    stylesheet += index_manager_->rules_index()->GetDefaultStylesheet() +
                  index_manager_->rules_index()
                      ->GetInjectionDataForOrigin(document_origin, false)
                      .stylesheet;
  }

  if (!stylesheet.empty()) {
    auto dynamic_injection =
        content_injection::mojom::DynamicInjectionItem::New();
    dynamic_injection->content = std::move(stylesheet);
    dynamic_injection->metadata =
        content_injection::mojom::InjectionItemMetadata::New();
    dynamic_injection->metadata->run_time =
        content_injection::mojom::ItemRunTime::kDocumentStart;
    dynamic_injection->metadata->type =
        content_injection::mojom::ItemType::kCSS;
    dynamic_injection->metadata->stylesheet_origin =
        content_injection::mojom::StylesheetOrigin::kUser;

    result->dynamic_injections.push_back(std::move(dynamic_injection));
  }
  return result;
}

const std::map<std::string, content_injection::StaticInjectionItem>&
ContentInjectionProvider::GetStaticContent() {
  return static_content_;
}

}  // namespace adblock_filter