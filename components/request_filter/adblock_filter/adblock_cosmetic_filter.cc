// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_cosmetic_filter.h"

#include "components/ad_blocker/adblock_rule_manager.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_impl.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "content/public/browser/render_frame_host.h"
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
    if (!rule_service_->IsRuleGroupEnabled(group))
      continue;

    RulesIndex* rules_index = rule_service_->GetRuleIndex(group);

    if (!rules_index ||
        !IsOriginWanted(rule_service_.get(), group, document_origin)) {
      continue;
    }

    PartyMatcher party_matcher = GetPartyMatcher(document_url, document_origin);

    RulesIndex::ActivationResults activations =
        rules_index->GetActivationsForFrame(
            base::BindRepeating(&IsOriginWanted, rule_service_.get(), group),
            frame, document_url);

    if (activations.GetDocumentDecision().value_or(flat::Decision_MODIFY) ==
        flat::Decision_PASS) {
      continue;
    }

    std::optional<RulesIndex::RuleAndSource> rule_and_source;
    for (const auto& ice_server : ice_servers) {
      rule_and_source = rules_index->FindMatchingBeforeRequestRule(
          ice_server, document_origin, flat::ResourceType_WEBRTC, party_matcher,
          (activations.by_type[flat::ActivationType_GENERIC_BLOCK]
               .GetDecision()
               .value_or(flat::Decision_MODIFY) == flat::Decision_PASS),
          base::BindRepeating(
              [](std::string_view, std::string_view) { return false; }));
      if (rule_and_source)
        break;
    }

    if (!rule_and_source ||
        rule_and_source->rule->decision() == flat::Decision_PASS) {
      continue;
    }

    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(true);
}

}  // namespace adblock_filter