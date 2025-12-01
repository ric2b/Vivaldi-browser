// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_request_filter.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "components/ad_blocker/content/adblock_document_activations.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/adblock_rules_index.h"
#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"
#include "components/ad_blocker/content/adblock_tab_state_and_logs_impl.h"
#include "components/ad_blocker/content/utils.h"
#include "components/ad_blocker/core/adblock_resources.h"
#include "components/request_filter/filtered_request_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "ui/base/page_transition_types.h"

namespace adblock_filter {

namespace {
int RuleGroupToPriority(RuleGroup group) {
  switch (group) {
    case RuleGroup::kTrackingRules:
      return 1;
    case RuleGroup::kAdBlockingRules:
      return 0;
  }
}

flat::ResourceType ResourceTypeFromRequest(
    const vivaldi::FilteredRequestInfo& request) {
  if (request.is_webtransport)
    return flat::ResourceType_WEBTRANSPORT;
  if (request.request.url.SchemeIsWSOrWSS())
    return flat::ResourceType_WEBSOCKET;
  if (request.loader_factory_type ==
      content::ContentBrowserClient::URLLoaderFactoryType::kDownload)
    return flat::ResourceType_OTHER;
  if (request.request.is_fetch_like_api) {
    // This must be checked before `request.keepalive` check below, because
    // currently Fetch keepAlive is not reported as ping.
    // See https://crbug.com/611453 for more details.
    return flat::ResourceType_XMLHTTPREQUEST;
  }

  switch (request.request.destination) {
    case network::mojom::RequestDestination::kDocument:
      return flat::ResourceType_DOCUMENT;
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
      return flat::ResourceType_SUBDOCUMENT;
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kXslt:
      return flat::ResourceType_STYLESHEET;
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
    case network::mojom::RequestDestination::kJson:
      return flat::ResourceType_SCRIPT;
    case network::mojom::RequestDestination::kImage:
      return flat::ResourceType_IMAGE;
    case network::mojom::RequestDestination::kFont:
      return flat::ResourceType_FONT;
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kSpeculationRules:
      return flat::ResourceType_OTHER;
    case network::mojom::RequestDestination::kWebBundle:
      return flat::ResourceType_WEBBUNDLE;
    case network::mojom::RequestDestination::kEmpty:
      if (request.request.keepalive)
        return flat::ResourceType_PING;
      return flat::ResourceType_OTHER;
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kEmbed:
      return flat::ResourceType_OBJECT;
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
      return flat::ResourceType_MEDIA;
    case network::mojom::RequestDestination::kReport:
      NOTREACHED();
  }
}

bool ShouldCollapse(flat::ResourceType resouce_type) {
  return resouce_type == flat::ResourceType_IMAGE ||
         resouce_type == flat::ResourceType_MEDIA ||
         resouce_type == flat::ResourceType_OBJECT ||
         resouce_type == flat::ResourceType_SUBDOCUMENT;
}
}  // namespace

AdBlockRequestFilter::AdBlockRequestFilter(
    base::WeakPtr<RuleServiceImpl> rules_service,
    RuleGroup group)
    : vivaldi::RequestFilter(vivaldi::RequestFilter::kAdBlock,
                             RuleGroupToPriority(group)),
      rule_service_(std::move(rules_service)),
      group_(group) {}

AdBlockRequestFilter::~AdBlockRequestFilter() = default;

void AdBlockRequestFilter::OnIndexLoaded() {
  for (auto& pending : pending_) {
    std::move(pending).Run();
  }

  pending_.clear();
}

bool AdBlockRequestFilter::WantsExtraHeadersForAnyRequest() const {
  return false;
}

bool AdBlockRequestFilter::WantsExtraHeadersForRequest(
    vivaldi::FilteredRequestInfo* request) const {
  return false;
}

bool AdBlockRequestFilter::DoesAdAttributionMatch(
    content::RenderFrameHost* frame,
    std::string_view tracker_url_spec,
    std::string_view ad_domain_and_query_trigger) {
  if (!rule_service_ || !frame) {
    return false;
  }
  return rule_service_->GetStateAndLogsImpl().DoesAdAttributionMatch(
      frame, tracker_url_spec, ad_domain_and_query_trigger);
}

bool AdBlockRequestFilter::OnBeforeRequest(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    BeforeRequestCallback callback) {
  auto destination = request->request.destination;

  bool is_main_frame =
      destination == network::mojom::RequestDestination::kDocument;

  url::Origin document_origin;
  if (is_main_frame || !request->request.request_initiator)
    document_origin = url::Origin::Create(request->request.url);
  else
    document_origin = request->request.request_initiator.value();

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  // TODO(julien): Add filtering of csp reports
  if (destination == network::mojom::RequestDestination::kReport ||
      !CanFilterUrl(request->request.url)) {
    return false;
  }

  if (!rule_service_) {
    // If the rule service is gone, we are in the process of shutting down.
    // Blocking all eligible requests at this point should be harmless.
    std::move(callback).Run(RequestFilter::kCancel, false, GURL());
    return true;
  }

  RulesIndex* rules_index = rule_service_->GetRuleIndex(group_);
  if (!rules_index) {
    // Delay handling until we are fully initialized
    // Unretained is Ok, since we own the callback
    pending_.emplace_back(base::BindOnce(
        base::IgnoreResult(&AdBlockRequestFilter::OnBeforeRequest),
        base::Unretained(this), browser_context, request, std::move(callback)));
    return true;
  }

  PartyMatcher party_matcher =
      GetPartyMatcher(request->request.url, document_origin);
  const flat::ResourceType resource_type = ResourceTypeFromRequest(*request);
  bool block_ping_handling = block_pings_ &&
                             resource_type == flat::ResourceType_PING &&
                             party_matcher.Run(flat::Party_THIRD);

  // For requests happening outside of frames, we can't rely on the activation
  // checks to discard requests from an allow-listed origin. Just check it
  // directly instead.
  if (!frame && !block_ping_handling &&
      !IsOriginWanted(rule_service_.get(), group_, document_origin)) {
    std::move(callback).Run(RequestFilter::kAllow, false, GURL());
    return true;
  }

  RulesIndex::ActivationResults activations;
  if (frame) {
    activations = request->navigation_id
                      ? rule_service_->GetStateAndLogsImpl()
                            .CreateTabHelperImpl(frame)
                            ->GetActivationsForLoadingFrame(
                                group_, *request->navigation_id,
                                frame ? frame->GetParent() : nullptr,
                                request->request.url)
                      : DocumentActivations::GetActivations(group_, frame);
  }

  const RulesIndex::ActivationResult& document_activations =
      activations.by_type[flat::ActivationType_DOCUMENT];
  if (document_activations.IsDecision(flat::Decision_PASS) &&
      document_activations.rule_details &&
      rule_service_->GetKnownSourcesHandler()->GetPresetIdForSourceId(
          group_, document_activations.rule_details->source_id) ==
          base::Uuid::ParseLowercase(
              adblock_filter::KnownRuleSourcesHandler::kPartnersListUuid)) {
    block_ping_handling = false;
  }

  // Even if we are to allow the whole document, we keep handling rules as
  // usual, in case we encounter some ad attribution rules.
  bool allow_whole_document =
      activations.IsDocumentDecision(flat::Decision_PASS);

  bool disable_generic_rules =
      activations.by_type[flat::ActivationType_GENERIC_BLOCK].IsDecision(
          flat::Decision_PASS);

  std::optional<RulesIndex::RuleAndSource> rule_and_source;
  if (!allow_whole_document && (!is_main_frame || allow_blocking_documents_)) {
    rule_and_source = rules_index->FindMatchingBeforeRequestRule(
        request->request.url, false /*must_intersect_host*/, document_origin,
        resource_type, party_matcher, disable_generic_rules,
        base::BindRepeating(&AdBlockRequestFilter::DoesAdAttributionMatch,
                            base::Unretained(this), frame));
  }

  CHECK(!rule_and_source ||
        rule_and_source->rule->options() & flat::OptionFlag_MODIFY_BLOCK);

  if ((!rule_and_source && !block_ping_handling) ||
      (rule_and_source &&
       rule_and_source->rule->decision() == flat::Decision_PASS)) {
    RulesIndex::FoundModifiersByType modifiers_by_type =
        rules_index->FindMatchingModifierRules(
            RulesIndex::kAllowedRequest, request->request.url, document_origin,
            resource_type, party_matcher, disable_generic_rules);
    if (is_main_frame) {
      RulesIndex::FoundModifiers& ad_query_trigger_results =
          modifiers_by_type[flat::Modifier_AD_QUERY_TRIGGER];

      std::vector<std::string> ad_query_triggers;
      for (const auto& [ad_query_trigger, ad_query_trigger_rule] :
           ad_query_trigger_results.value_with_decision) {
        if (ad_query_trigger_rule.rule->decision() != flat::Decision_PASS) {
          ad_query_triggers.push_back(ad_query_trigger);
        }
      }
      if (!ad_query_triggers.empty() && frame) {
        rule_service_->GetStateAndLogsImpl().SetTabAdQueryTriggers(
            request->request.url, std::move(ad_query_triggers), frame);
      }
    }

    if (rule_and_source &&
        rule_and_source->rule->ad_domains_and_query_triggers()) {
      std::move(callback).Run(RequestFilter::kPreventCancel, false, GURL());
      return true;
    }

    std::move(callback).Run(RequestFilter::kAllow, false, GURL());
    return true;
  }

  if (frame && rule_and_source)
    rule_service_->GetStateAndLogsImpl().OnUrlBlocked(
        group_, document_origin, request->request.url, frame);

  RulesIndex::FoundModifiersByType modifiers_by_type =
      rules_index->FindMatchingModifierRules(
          RulesIndex::kBlockedRequest, request->request.url, document_origin,
          resource_type, party_matcher, disable_generic_rules);

  RulesIndex::FoundModifiers& redirects =
      modifiers_by_type[flat::Modifier_REDIRECT];

  std::optional<RequestFilterRule::ResourceType> core_resource_type =
      ToCoreResourceType(resource_type);
  if (!redirects.value_with_decision.empty() && core_resource_type) {
    auto redirect =
        std::max_element(redirects.value_with_decision.begin(),
                         redirects.value_with_decision.end(),
                         [this, core_resource_type](auto& lhs, auto& rhs) {
                           if (!rule_service_->GetResources().GetRedirect(
                                   lhs.first, *core_resource_type)) {
                             return true;
                           }
                           if (!rule_service_->GetResources().GetRedirect(
                                   rhs.first, *core_resource_type)) {
                             return false;
                           }

                           return GetRulePriority(*lhs.second.rule) <
                                  GetRulePriority(*rhs.second.rule);
                         });

    std::optional<std::string> resource(
        rule_service_->GetResources().GetRedirect(redirect->first,
                                                  *core_resource_type));
    if (resource) {
      std::move(callback).Run(RequestFilter::kAllow, false,
                              GURL(resource.value()));
      return true;
    }
  }

  if (request->navigation_id && frame) {
    rule_service_->GetStateAndLogsImpl()
        .CreateTabHelperImpl(frame)
        ->OnBlockedNavigation(group_, *rule_and_source,
                              *request->navigation_id);
  }

  std::move(callback).Run(RequestFilter::kCancel,
                          ShouldCollapse(ResourceTypeFromRequest(*request)),
                          GURL());
  return true;
}

bool AdBlockRequestFilter::OnBeforeSendHeaders(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpRequestHeaders* headers,
    BeforeSendHeadersCallback callback) {
  return false;
}

void AdBlockRequestFilter::OnSendHeaders(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpRequestHeaders& headers) {}

bool AdBlockRequestFilter::OnHeadersReceived(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpResponseHeaders* headers,
    HeadersReceivedCallback callback) {
  auto destination = request->request.destination;

  url::Origin document_origin = request->request.request_initiator.value_or(
      url::Origin::Create(request->request.url));

  if (destination == network::mojom::RequestDestination::kReport ||
      !CanFilterUrl(request->request.url)) {
    return false;
  }

  if (!rule_service_) {
    // If the rule service is gone, we are in the process of shutting down.
    // Blocking all eligible requests at this point should be harmless.
    std::move(callback).Run(RequestFilter::kCancel, false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  RulesIndex* rules_index = rule_service_->GetRuleIndex(group_);
  if (!rules_index) {
    // Delay handling until we are fully initialized
    // First Unretained is Ok, since we own the callback
    // Second Unretained is OK, because the headers are not going away,
    // so long as `callback` hasn't been called
    pending_.push_back(base::BindOnce(
        base::IgnoreResult(&AdBlockRequestFilter::OnHeadersReceived),
        base::Unretained(this), browser_context, request,
        base::Unretained(headers), std::move(callback)));
    return true;
  }

  PartyMatcher party_matcher =
      GetPartyMatcher(request->request.url, document_origin);

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  RulesIndex::ActivationResults activations;
  if (frame) {
    activations = request->navigation_id
                      ? rule_service_->GetStateAndLogsImpl()
                            .CreateTabHelperImpl(frame)
                            ->GetActivationsForLoadingFrame(
                                group_, *request->navigation_id,
                                frame ? frame->GetParent() : nullptr,
                                request->request.url)
                      : DocumentActivations::GetActivations(group_, frame);
  }

  if (activations.IsDocumentDecision(flat::Decision_PASS)) {
    std::move(callback).Run(RequestFilter::kAllow, false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  RulesIndex::FoundModifiersByType modifiers_by_type =
      rules_index->FindMatchingModifierRules(
          RulesIndex::kHeadersReceived, request->request.url, document_origin,
          flat::ResourceType_ANY, party_matcher,
          (activations.by_type[flat::ActivationType_GENERIC_BLOCK].IsDecision(
              flat::Decision_PASS)));

  RulesIndex::FoundModifiers& csp = modifiers_by_type[flat::Modifier_CSP];

  if (csp.value_with_decision.empty()) {
    std::move(callback).Run(RequestFilter::kAllow, false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  std::set<std::string> added_headers;

  for (const auto& [value, rule_and_source] : csp.value_with_decision) {
    if (rule_and_source.rule->decision() == flat::Decision_PASS) {
      continue;
    }
    added_headers.insert(value);
  }

  vivaldi::RequestFilter::ResponseHeaderChanges response_header_changes;
  for (const auto& added_header : added_headers) {
    response_header_changes.headers_to_add.push_back(
        std::make_pair("Content-Security-Policy", added_header));
  }

  std::move(callback).Run(RequestFilter::kAllow, false, GURL(),
                          std::move(response_header_changes));
  return true;
}

void AdBlockRequestFilter::OnBeforeRedirect(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const GURL& redirect_url) {}

void AdBlockRequestFilter::OnResponseStarted(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request) {}

void AdBlockRequestFilter::OnCompleted(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request) {}

void AdBlockRequestFilter::OnErrorOccured(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    int net_error) {}
}  // namespace adblock_filter
