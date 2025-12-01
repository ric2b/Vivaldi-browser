// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_cosmetic_filter.h"

#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/content/simple_index_request_query.h"
#include "components/ad_blocker/content/utils.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/ad_blocker/public/core/adblock_rule_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace adblock_filter {

CosmeticFilter::CosmeticFilter(base::WeakPtr<RuleServiceImpl> rule_service,
                               content::ChildProcessId process_id,
                               int frame_id)
    : rule_service_(rule_service),
      process_id_(process_id),
      frame_id_(frame_id) {}

CosmeticFilter::~CosmeticFilter() = default;

void CosmeticFilter::ShouldAllowWebRTC(const ::GURL& document_url,
                                       const std::vector<::GURL>& ice_servers,
                                       ShouldAllowWebRTCCallback callback) {
  if (!rule_service_) {
    return;
  }

  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id_.value(), frame_id_);
  if (ice_servers.empty() || !frame || !document_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(true);
    return;
  }

  content::RenderFrameHost* parent = frame->GetParent();
  url::Origin document_origin = parent ? parent->GetLastCommittedOrigin()
                                       : url::Origin::Create(document_url);

  for (auto group : {RuleGroup::kTrackingRules, RuleGroup::kAdBlockingRules}) {
    RulesIndex* rules_index = rule_service_->GetRuleIndex(group);

    if (!rules_index || rule_service_->GetRuleManager()->IsExemptOfFiltering(
                            group, document_origin)) {
      continue;
    }

    const ActivationResults& activations =
        DocumentState::GetActivations(group, frame);

    if (activations.IsDocumentDecision(RuleDecision::kPass)) {
      continue;
    }

    bool should_allow = true;

    for (const auto& ice_server : ice_servers) {
      if (!CanFilterUrl(ice_server, false)) {
        continue;
      }

      const std::optional<RequestFilterRuleStub> rule_stub =
          rules_index->FindMatchingBeforeRequestRule(
              SimpleIndexRequestQuery(
                  ice_server, document_origin, "" /*method*/,
                  (activations.by_type[ActivationType::kGenericBlock]
                       .IsDecision(RuleDecision::kPass))),
              false /*must_intersect_host*/, ResourceType::kWebRTC,
              std::nullopt);
      if (rule_stub && rule_stub->decision != RuleDecision::kPass) {
        should_allow = false;
        break;
      }
    }

    if (!should_allow) {
      std::move(callback).Run(false);
      return;
    }
  }

  std::move(callback).Run(true);
}

}  // namespace adblock_filter
