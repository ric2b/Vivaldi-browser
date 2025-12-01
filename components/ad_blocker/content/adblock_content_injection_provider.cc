// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_content_injection_provider.h"

#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_navigation_tracker_impl.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/content/simple_index_base_query.h"
#include "components/ad_blocker/content/utils.h"
#include "components/ad_blocker/public/content/adblock_rule_service.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/content_injection/content_injection_service.h"
#include "components/content_injection/content_injection_service_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/isolated_world_ids.h"

namespace adblock_filter {
namespace {
constexpr char kContentInjectionPrefix[] = "vivaldi://adblocker/";
constexpr char kJavascriptWorldStableID[] = "adblocker";
constexpr char kJavascriptWorldName[] = "Vivaldi AdBlocker";
}  // namespace

ContentInjectionProvider::ContentInjectionProvider(
    content::BrowserContext* context,
    RuleServiceImpl* rule_service,
    Resources* resources)
    : context_(context), rule_service_(rule_service), resources_(resources) {
  if (resources_->loaded())
    BuildStaticContent();
  else
    resources->AddObserver(this);
}

ContentInjectionProvider::~ContentInjectionProvider() = default;

content_injection::mojom::InjectionsForFramePtr
ContentInjectionProvider::GetInjectionsForFrame(
    const GURL& url,
    content::RenderFrameHost* frame) {
  auto result = content_injection::mojom::InjectionsForFrame::New();

  if (!url.SchemeIsHTTPOrHTTPS())
    return result;

  content::RenderFrameHost* parent = frame->GetParent();
  url::Origin document_origin =
      parent ? parent->GetLastCommittedOrigin() : url::Origin::Create(url);

  std::string stylesheet;

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    RulesIndex* rule_index = rule_service_->GetRuleIndex(group);
    if (!rule_index) {
      continue;
    }

    const ActivationResults* activations = nullptr;
    if (url == frame->GetLastCommittedURL()) {
      activations = &DocumentState::GetActivations(group, frame);
    } else {
      for (base::SafeRef<content::NavigationHandle> navigation_handle :
           frame->GetPendingCommitCrossDocumentNavigations()) {
        if (navigation_handle->GetURL() == url) {
          activations =
              &NavigationTrackerImpl::GetForNavigationHandle(*navigation_handle)
                   ->GetActivations(group);
        }
      }
    }

    if (!activations) {
      // Somehow, we are getting activations for a document for which the
      // url doesn't match this frame. Unclear how this happens.
      activations = &rule_index->FindActivations(
          SimpleIndexBaseQuery(url, parent ? parent->GetLastCommittedOrigin()
                                           : url::Origin::Create(url)));
    }
    CHECK(activations);

    const bool disable_specific_rules =
        activations->by_type[ActivationType::kSpecificHide].IsDecision(
            RuleDecision::kPass);
    const bool disable_generic_rules =
        activations->by_type[ActivationType::kGenericHide].IsDecision(
            RuleDecision::kPass);
    if (activations->IsDocumentDecision(RuleDecision::kPass) ||
        (disable_generic_rules && disable_specific_rules)) {
      continue;
    }

    RulesIndex::InjectionData injection_data =
        rule_index->GetInjectionDataForOrigin(
            document_origin, disable_specific_rules, disable_generic_rules);
    if (!disable_generic_rules) {
      stylesheet += rule_index->GetDefaultStylesheet();
    }
    stylesheet += injection_data.stylesheet;

    for (auto& injection : injection_data.scriptlet_injections) {
      auto enabled_injection =
          content_injection::mojom::EnabledStaticInjection::New();
      enabled_injection->key =
          std::string(kContentInjectionPrefix) + injection.first;
      // uBO uses 1-based placeholders, but the content-injection implementation
      // takes 0-based placeholders for simplicity.
      // Therefore, we insert an empty placeholder so that the remaining
      // placeholders match the 1-based index.
      // abp and adg both use a single placeholder which we chose to number as
      // 1 to match this scheme.
      enabled_injection->placeholder_replacements.push_back(std::string());
      for (auto& placeholder_replacement : injection.second) {
        enabled_injection->placeholder_replacements.push_back(
            std::move(placeholder_replacement));
      }
      result->static_injections.push_back(std::move(enabled_injection));
    }
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

void ContentInjectionProvider::OnResourcesLoaded() {
  resources_->RemoveObserver(this);
  BuildStaticContent();
}

void ContentInjectionProvider::BuildStaticContent() {
  content_injection::Service* content_injection_service =
      content_injection::ServiceFactory::GetInstance()->GetForBrowserContext(
          context_);

  content_injection::mojom::JavascriptWorldInfoPtr world_info =
      content_injection::mojom::JavascriptWorldInfo::New();
  world_info->stable_id = kJavascriptWorldStableID;
  world_info->name = kJavascriptWorldName;
  javascript_world_id_ = content_injection_service->RegisterWorldForJSInjection(
      std::move(world_info));

  if (javascript_world_id_) {
    std::map<std::string, Resources::InjectableResource> injections =
        resources_->GetInjections();

    for (const auto& injection : injections) {
      auto emplace_result = static_content_.emplace(
          std::make_pair(std::string(kContentInjectionPrefix) + injection.first,
                         content_injection::StaticInjectionItem()));
      DCHECK(emplace_result.second);
      content_injection::StaticInjectionItem& item =
          emplace_result.first->second;
      item.content = injection.second.code;
      item.metadata.type = content_injection::mojom::ItemType::kJS;
      item.metadata.javascript_world_id =
          injection.second.use_main_world ? content::ISOLATED_WORLD_ID_GLOBAL
                                          : javascript_world_id_.value();
      item.metadata.run_time =
          content_injection::mojom::ItemRunTime::kDocumentStart;
    }
  }

  content_injection_service->AddProvider(this);
}

}  // namespace adblock_filter
