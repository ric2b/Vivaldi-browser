// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_cosmetic_filter.h"

#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace adblock_filter {
namespace {
bool IsOriginWanted(RuleService* service, RuleGroup group, url::Origin origin) {
  // Allow all requests made by extensions.
  if (origin.scheme() == "chrome-extension")
    return false;

  return !service->GetRuleManager()->IsExemptOfFiltering(group, origin);
}
}  // namespace

// static
void CosmeticFilter::Create(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<mojom::CosmeticFilter> receiver) {
  std::unique_ptr<CosmeticFilter> cosmetic_filter(
      new CosmeticFilter(frame->GetProcess()->GetID(), frame->GetRoutingID()));
  RuleServiceFactory::GetForBrowserContext(
      frame->GetProcess()->GetBrowserContext())
      ->InitializeCosmeticFilter(cosmetic_filter.get());
  mojo::MakeSelfOwnedReceiver(std::move(cosmetic_filter), std::move(receiver));
}

CosmeticFilter::CosmeticFilter(int process_id, int frame_id)
    : process_id_(process_id), frame_id_(frame_id) {}

CosmeticFilter::~CosmeticFilter() = default;

void CosmeticFilter::Initialize(std::array<base::WeakPtr<RulesIndexManager>,
                                           kRuleGroupCount> index_managers) {
  index_managers_.swap(index_managers);
}

void CosmeticFilter::ShouldAllowWebRTC(const ::GURL& document_url,
                                       const std::vector<::GURL>& ice_servers,
                                       ShouldAllowWebRTCCallback callback) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id_, frame_id_);
  if (ice_servers.empty() || !frame || !document_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(true);
    return;
  }

  RuleService* service = RuleServiceFactory::GetForBrowserContext(
      frame->GetProcess()->GetBrowserContext());

  content::RenderFrameHost* parent = frame->GetParent();
  url::Origin document_origin = parent ? parent->GetLastCommittedOrigin()
                                       : url::Origin::Create(document_url);

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    if (!service->IsRuleGroupEnabled(group))
      continue;

    auto& rules_index_manager_ = index_managers_[static_cast<size_t>(group)];

    if (!rules_index_manager_ || !rules_index_manager_->rules_index() ||
        !IsOriginWanted(service, group, document_origin)) {
      continue;
    }

    bool is_third_party = IsThirdParty(document_url, document_origin);

    RulesIndex::ActivationsFound activations =
        rules_index_manager_->rules_index()->FindMatchingActivationsRules(
            document_url, document_origin, is_third_party, frame);

    activations.in_allow_rules |=
        rules_index_manager_->rules_index()
            ->GetActivationsForFrame(
                base::BindRepeating(&IsOriginWanted, service, group),
                frame->GetParent())
            .in_allow_rules;

    if ((activations.in_allow_rules & flat::ActivationType_DOCUMENT) != 0) {
      continue;
    }

    const flat::RequestFilterRule* request_filter_rule;
    for (const auto& ice_server : ice_servers) {
      request_filter_rule =
          rules_index_manager_->rules_index()->FindMatchingBeforeRequestRule(
              ice_server, document_origin, flat::ResourceType_WEBRTC,
              is_third_party,
              (activations.in_allow_rules &
               flat::ActivationType_GENERIC_BLOCK));
      if (request_filter_rule)
        break;
    }

    if (!request_filter_rule ||
        request_filter_rule->options() & flat::OptionFlag_IS_ALLOW_RULE) {
      continue;
    }

    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

}  // namespace adblock_filter