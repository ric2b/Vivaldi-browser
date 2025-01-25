// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_content_injection_provider.h"

#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/content_injection/content_injection_service.h"
#include "components/content_injection/content_injection_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/isolated_world_ids.h"

namespace adblock_filter {
namespace {
constexpr char kContentInjectionPrefix[] = "vivaldi://adblocker/";
constexpr char kJavascriptWorldStableID[] = "adblocker";
constexpr char kJavascriptWorldName[] = "Vivaldi AdBlocker";

bool IsOriginWanted(RuleService* service, RuleGroup group, url::Origin origin) {
  // Allow all requests made by extensions.
  if (origin.scheme() == "chrome-extension")
    return false;

  return !service->GetRuleManager()->IsExemptOfFiltering(group, origin);
}
}  // namespace

ContentInjectionProvider::ContentInjectionProvider(
    content::BrowserContext* context,
    std::array<RulesIndexManager*, kRuleGroupCount> index_managers,
    Resources* resources)
    : context_(context),
      index_managers_(index_managers),
      resources_(resources) {
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

  RuleService* service = RuleServiceFactory::GetForBrowserContext(
      frame->GetProcess()->GetBrowserContext());

  if (!url.SchemeIsHTTPOrHTTPS())
    return result;

  content::RenderFrameHost* parent = frame->GetParent();
  url::Origin document_origin =
      parent ? parent->GetLastCommittedOrigin() : url::Origin::Create(url);

  std::string stylesheet;

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    if (!service->IsRuleGroupEnabled(group))
      continue;

    auto* index_manager = index_managers_[static_cast<size_t>(group)];

    if (!index_manager || !index_manager->rules_index()) {
      continue;
    }

    RulesIndex::ActivationResults activations =
        index_manager->rules_index()->GetActivationsForFrame(
            base::BindRepeating(&IsOriginWanted, service,
                                index_manager->group()),
            frame, url);

    if (activations[flat::ActivationType_DOCUMENT].GetDecision().value_or(
            flat::Decision_MODIFY) == flat::Decision_PASS ||
        activations[flat::ActivationType_ELEMENT_HIDE].GetDecision().value_or(
            flat::Decision_MODIFY) == flat::Decision_PASS) {
      continue;
    }

    RulesIndex::InjectionData injection_data;
    if (activations[flat::ActivationType_GENERIC_HIDE].GetDecision().value_or(
            flat::Decision_MODIFY) == flat::Decision_PASS) {
      injection_data = index_manager->rules_index()->GetInjectionDataForOrigin(
          document_origin, true);
    } else {
      injection_data = index_manager->rules_index()->GetInjectionDataForOrigin(
          document_origin, false);
      stylesheet += index_manager->rules_index()->GetDefaultStylesheet();
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