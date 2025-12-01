// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/ad_blocker/content/adblock_request_filter.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "components/ad_blocker/content/adblock_document_state.h"
#include "components/ad_blocker/content/adblock_navigation_tracker_impl.h"
#include "components/ad_blocker/content/adblock_rule_service_impl.h"
#include "components/ad_blocker/content/adblock_state_and_logs_impl.h"
#include "components/ad_blocker/content/adblock_tab_state_and_logs_impl.h"
#include "components/ad_blocker/content/index/adblock_rules_index.h"
#include "components/ad_blocker/content/simple_index_base_query.h"
#include "components/ad_blocker/content/utils.h"
#include "components/ad_blocker/core/adblock_resources.h"
#include "components/ad_blocker/public/core/adblock_request_filter_rule_types.h"
#include "components/prefs/pref_service.h"
#include "components/request_filter/filtered_request_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

#include "vivaldi/prefs/vivaldi_gen_prefs.h"

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

ResourceType ResourceTypeFromRequest(
    const vivaldi::FilteredRequestInfo& request,
    bool use_original_destination) {
  if (request.is_webtransport)
    return ResourceType::kWebTransport;
  if (request.request.url.SchemeIsWSOrWSS())
    return ResourceType::kWebSocket;
  if (request.loader_factory_type ==
      content::ContentBrowserClient::URLLoaderFactoryType::kDownload)
    return ResourceType::kOther;
  if (request.request.is_fetch_like_api) {
    // This must be checked before `request.keepalive` check below, because
    // currently Fetch keepAlive is not reported as ping.
    // See https://crbug.com/611453 for more details.
    return ResourceType::kXmlHttpRequest;
  }

  switch (use_original_destination ? request.request.original_destination
                                   : request.request.destination) {
    case network::mojom::RequestDestination::kDocument:
      return ResourceType::kDocument;
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
      return ResourceType::kSubDocument;
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kXslt:
      return ResourceType::kStylesheet;
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
    case network::mojom::RequestDestination::kJson:
      return ResourceType::kScript;
    case network::mojom::RequestDestination::kImage:
      return ResourceType::kImage;
    case network::mojom::RequestDestination::kFont:
      return ResourceType::kFont;
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kSpeculationRules:
      return ResourceType::kOther;
    case network::mojom::RequestDestination::kWebBundle:
      return ResourceType::kWebBundle;
    case network::mojom::RequestDestination::kEmpty:
      if (request.request.keepalive)
        return ResourceType::kPing;
      return ResourceType::kOther;
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kEmbed:
      return ResourceType::kObject;
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
      return ResourceType::kMedia;
    case network::mojom::RequestDestination::kReport:
      NOTREACHED();
  }
}

bool ShouldCollapse(ResourceType resouce_type) {
  return resouce_type == ResourceType::kImage ||
         resouce_type == ResourceType::kMedia ||
         resouce_type == ResourceType::kObject ||
         resouce_type == ResourceType::kSubDocument;
}

class RequestHandler : public SimpleIndexBaseQuery, RulesIndex::RequestQuery {
 public:
  ~RequestHandler() = default;

  RequestHandler(RequestHandler&&) = default;
  RequestHandler& operator=(RequestHandler&&) = default;

  RequestHandler(RuleServiceImpl& rule_service,
                 RuleGroup group,
                 const vivaldi::FilteredRequestInfo* request,
                 bool allow_blocking_documents,
                 bool block_pings,
                 bool is_main_frame,
                 url::Origin document_origin,
                 ResourceType resource_type,
                 ResourceType original_resource_type)
      : SimpleIndexBaseQuery(request->request.url, document_origin),
        rule_service_(rule_service),
        group_(group),
        request_(request),
        is_main_frame_(is_main_frame),
        resource_type_(resource_type),
        original_resource_type_(resource_type),
        allow_blocking_documents_(allow_blocking_documents),
        block_ping_handling_(block_pings &&
                             resource_type == ResourceType::kPing &&
                             IsThirdParty()) {}

  RulesIndex* GetIndex() const { return rule_service_->GetRuleIndex(group_); }

  bool IsExemptFramelessRequest() const {
    // For requests happening outside of frames, we won't get cached activation
    // results, so let's just check this directly instead.
    return !GetFrame() && !block_ping_handling_ &&
           rule_service_->GetRuleManager()->IsExemptOfFiltering(group_,
                                                                GetOrigin());
  }

  void HandleBeforeRequest(
      vivaldi::RequestFilter::BeforeRequestCallback callback) && {
    Init();
    CHECK(activations_);
    RulesIndex* index = GetIndex();
    content::RenderFrameHost* frame = GetFrame();

    // Even if we are to allow the whole document, we keep handling rules as
    // usual, in case we encounter some ad attribution rules.
    const bool allow_whole_document =
        activations_->IsDocumentDecision(RuleDecision::kPass);

    const std::optional<RequestFilterRuleStub>& rule_stub =
        (!allow_whole_document &&
         (!is_main_frame_ || allow_blocking_documents_))
            ? index->FindMatchingBeforeRequestRule(
                  *this, false /*must_intersect_host*/, resource_type_,
                  rule_service_->GetStateAndLogsImpl()
                      .GetAdAttributionMatchParams(frame))
            : std::nullopt;

    CHECK(!rule_stub || rule_stub->modify_block);

    if ((!rule_stub && !block_ping_handling_) ||
        (rule_stub && rule_stub->decision == RuleDecision::kPass)) {
      if (is_main_frame_) {
        const RulesIndex::FoundModifiersByType& modifiers_by_type =
            index->FindMatchingModifierRules(
                RulesIndex::ModifierCategory::kAllowedRequest, *this,
                resource_type_);
        const RulesIndex::FoundModifiers& ad_query_trigger_results =
            modifiers_by_type[ModifierType::kAdQueryTrigger];
        std::vector<std::string> ad_query_triggers;
        for (const auto& [ad_query_trigger, ad_query_trigger_rule] :
             ad_query_trigger_results.value_with_decision) {
          if (ad_query_trigger_rule.decision != RuleDecision::kPass) {
            ad_query_triggers.push_back(ad_query_trigger);
          }
        }
        if (!ad_query_triggers.empty() && frame) {
          rule_service_->GetStateAndLogsImpl().SetTabAdQueryTriggers(
              request_->request.url, std::move(ad_query_triggers), frame);
        }
      }

      if (rule_stub && rule_stub->is_attribution_allow_rule) {
        rule_service_->GetStateAndLogsImpl().OnMatchedAttributionTracker(
            frame, GetUrl());
        std::move(callback).Run(vivaldi::RequestFilter::kPreventCancel, false,
                                GURL());
        return;
      }

      std::move(callback).Run(vivaldi::RequestFilter::kAllow, false, GURL());
      return;
    }

    if (frame && rule_stub)
      rule_service_->GetStateAndLogsImpl().OnUrlBlocked(group_, GetOrigin(),
                                                        GetUrl(), frame);

    const RulesIndex::FoundModifiersByType& modifiers =
        index->FindMatchingModifierRules(
            RulesIndex::ModifierCategory::kBlockedRequest, *this,
            resource_type_);

    const RulesIndex::FoundModifiers& redirects =
        modifiers[ModifierType::kRedirect];

    if (!redirects.value_with_decision.empty() &&
        RegularResourceTypes::All().Has(resource_type_)) {
      const auto redirect = std::max_element(
          redirects.value_with_decision.begin(),
          redirects.value_with_decision.end(), [this](auto& lhs, auto& rhs) {
            if (!rule_service_->GetResources().GetRedirect(lhs.first,
                                                           resource_type_)) {
              return true;
            }
            if (!rule_service_->GetResources().GetRedirect(rhs.first,
                                                           resource_type_)) {
              return false;
            }

            return lhs.second.priority < rhs.second.priority;
          });

      std::optional<std::string> resource(
          rule_service_->GetResources().GetRedirect(redirect->first,
                                                    resource_type_));
      if (resource) {
        std::move(callback).Run(vivaldi::RequestFilter::kAllow, false,
                                GURL(resource.value()));
        return;
      }
    }

    if (request_->navigation_id) {
      rule_service_->GetStateAndLogsImpl()
          .GetNavigationTrackerFromNavigationId(*request_->navigation_id)
          ->OnBlockedByRule(group_, *rule_stub);
    }

    std::move(callback).Run(vivaldi::RequestFilter::kCancel,
                            ShouldCollapse(resource_type_), GURL());
  }

  void HandleHeadersReceived(
      const net::HttpResponseHeaders* headers,
      vivaldi::RequestFilter::HeadersReceivedCallback callback) && {
    Init();
    CHECK(activations_);
    RulesIndex* index = GetIndex();

    if (activations_->IsDocumentDecision(RuleDecision::kPass)) {
      std::move(callback).Run(vivaldi::RequestFilter::kAllow, false, GURL(),
                              vivaldi::RequestFilter::ResponseHeaderChanges());
      return;
    }

    const RulesIndex::FoundModifiersByType& modifiers =
        index->FindMatchingModifierRules(
            RulesIndex::ModifierCategory::kHeadersReceived, *this,
            std::nullopt);

    const RulesIndex::FoundModifiers& csp = modifiers[ModifierType::kCsp];

    if (csp.value_with_decision.empty()) {
      std::move(callback).Run(vivaldi::RequestFilter::kAllow, false, GURL(),
                              vivaldi::RequestFilter::ResponseHeaderChanges());
      return;
    }

    std::set<std::string> added_headers;

    for (const auto& [value, rule_stub] : csp.value_with_decision) {
      if (rule_stub.decision == RuleDecision::kPass) {
        continue;
      }
      added_headers.insert(value);
    }

    vivaldi::RequestFilter::ResponseHeaderChanges response_header_changes;
    for (const auto& added_header : added_headers) {
      response_header_changes.headers_to_add.push_back(
          std::make_pair("Content-Security-Policy", added_header));
    }

    std::move(callback).Run(vivaldi::RequestFilter::kAllow, false, GURL(),
                            std::move(response_header_changes));
  }

 private:
  void Init() {
    RulesIndex* index = GetIndex();
    content::RenderFrameHost* frame = GetFrame();
    CHECK(index) << "Should not call this before we have an index";
    const bool is_frame =
        is_main_frame_ || resource_type_ == ResourceType::kSubDocument ||
        (  // Extra step to detect the main frame if the request comes from a
           // service worker. This behavior diverges from uBlock, but is limited
           // enough that no issue is expected.
            request_->request.mode == network::mojom::RequestMode::kNavigate &&
            request_->request.destination ==
                network::mojom::RequestDestination::kEmpty &&
            original_resource_type_ == ResourceType::kSubDocument);

    /*It's possible that the navigation tracker already went away, if the page
     * was closed before receiving the request response. In that case, behave as
     * if this wasn't a navigation. The result probably won't matter.*/
    NavigationTrackerImpl* tracker =
        request_->navigation_id ? rule_service_->GetStateAndLogsImpl()
                                      .GetNavigationTrackerFromNavigationId(
                                          *request_->navigation_id)
                                : nullptr;
    if (tracker) {
      activations_ = &tracker->GetActivations(group_);
    } else if (frame) {
      activations_ = &DocumentState::GetActivations(group_, frame);
    } else if (is_frame) {
      activations_ = &index->FindActivations(*this);
    } else {
      // Can't calculate the activation for a resource request not tied to a
      // frame, since we need the URL of the frame
      activations_ = &kEmptyActivationResults;
    }

    disable_generic_rules_ =
        activations_->by_type[ActivationType::kGenericBlock].IsDecision(
            RuleDecision::kPass);

    if (block_ping_handling_) {
      const ActivationResult& document_activations =
          activations_->by_type[ActivationType::kWholeDocument];
      if (document_activations.IsDecision(RuleDecision::kPass) &&
          document_activations.rule_stub &&
          rule_service_->GetKnownSourcesHandler()->GetPresetIdForSourceId(
              group_, document_activations.rule_stub->rule_source_id) ==
              base::Uuid::ParseLowercase(
                  adblock_filter::KnownRuleSourcesHandler::kPartnersListUuid)) {
        block_ping_handling_ = false;
      }
    }
  }

  content::RenderFrameHost* GetFrame() const {
    return content::RenderFrameHost::FromID(request_->render_process_id,
                                            request_->render_frame_id);
  }

  std::string_view GetMethod() const override {
    return request_->request.method;
  }

  bool WantsDisableGenericRules() const override {
    return disable_generic_rules_;
  }

  base::raw_ref<RuleServiceImpl> rule_service_;
  RuleGroup group_;
  raw_ptr<const vivaldi::FilteredRequestInfo> request_;
  bool is_main_frame_;
  ResourceType resource_type_;
  ResourceType original_resource_type_;
  bool allow_blocking_documents_;
  bool block_ping_handling_;
  raw_ptr<const ActivationResults> activations_;
  bool disable_generic_rules_ = false;
};

struct CreateHandlerResult {
  enum { kIgnore, kBlock, kHandler } operation;
  std::optional<RequestHandler> handler;
};

static CreateHandlerResult CreateHandler(
    base::WeakPtr<RuleServiceImpl> rule_service,
    RuleGroup group,
    const vivaldi::FilteredRequestInfo* request,
    bool allow_blocking_documents,
    bool block_pings) {
  auto destination = request->request.destination;
  // TODO(julien): Add filtering of csp reports
  if (destination == network::mojom::RequestDestination::kReport ||
      !CanFilterUrl(request->request.url, false)) {
    return {CreateHandlerResult::kIgnore};
  }

  if (!rule_service) {
    return {CreateHandlerResult::kBlock};
  }

  const ResourceType resource_type = ResourceTypeFromRequest(*request, false);
  const ResourceType original_resource_type =
      ResourceTypeFromRequest(*request, true);
  bool is_main_frame =
      resource_type == ResourceType::kDocument ||
      (  // Extra step to detect the main frame if the request comes from a
         // service worker. This behavior diverges from uBlock, but is limited
         // enough that no issue is expected.
          request->request.mode == network::mojom::RequestMode::kNavigate &&
          request->request.destination ==
              network::mojom::RequestDestination::kEmpty &&
          original_resource_type == ResourceType::kDocument);

  url::Origin document_origin;
  if (is_main_frame || !request->request.request_initiator) {
    document_origin = url::Origin::Create(request->request.url);
  } else {
    document_origin = request->request.request_initiator.value();
  }

  RequestHandler handler(
      *rule_service, group, request, allow_blocking_documents, block_pings,
      is_main_frame, document_origin, resource_type, original_resource_type);
  if (handler.IsExemptFramelessRequest()) {
    return {CreateHandlerResult::kIgnore};
  }

  return {CreateHandlerResult::kHandler, std::move(handler)};
}

}  // namespace

AdBlockRequestFilter::AdBlockRequestFilter(
    base::WeakPtr<RuleServiceImpl> rule_service,
    RuleGroup group,
    PrefService* prefs)
    : vivaldi::RequestFilter(vivaldi::RequestFilter::kAdBlock,
                             RuleGroupToPriority(group)),
      rule_service_(std::move(rule_service)),
      group_(group) {
  if (!rule_service_->GetRuleIndex(group)) {
    rule_service_->AddObserver(this);
  }
  pref_change_registrar_.Init(prefs);

  allow_blocking_documents_ =
      prefs->GetBoolean(vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking);
  pref_change_registrar_.Add(
      vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking,
      base::BindRepeating(
          &AdBlockRequestFilter::OnEnableDocumentBlockingChanged,
          weak_factory_.GetWeakPtr()));
  if (group == RuleGroup::kAdBlockingRules) {
    block_pings_ = prefs->GetBoolean(vivaldiprefs::kPrivacyBlockPingsEnabled);
    pref_change_registrar_.Add(
        vivaldiprefs::kPrivacyBlockPingsEnabled,
        base::BindRepeating(&AdBlockRequestFilter::OnPingBlockingChanged,
                            weak_factory_.GetWeakPtr()));
  }
}

AdBlockRequestFilter::~AdBlockRequestFilter() {
  if (rule_service_) {
    // This is allowed even we are not in the observer list.
    rule_service_->RemoveObserver(this);
  }
}

void AdBlockRequestFilter::OnRulesIndexLoaded(RuleGroup group) {
  if (group != group_) {
    return;
  }

  // Avoid using iterators, as running the callbacks can lead to items being
  // removed and iterators consequently becoming invalid.
  while (!pending_.empty()) {
    std::move(pending_.extract(pending_.begin()).mapped()).Run();
  }

  rule_service_->RemoveObserver(this);
}

bool AdBlockRequestFilter::WantsExtraHeadersForAnyRequest() const {
  return false;
}

bool AdBlockRequestFilter::WantsExtraHeadersForRequest(
    vivaldi::FilteredRequestInfo* request) const {
  return false;
}

void AdBlockRequestFilter::OnEnableDocumentBlockingChanged() {
  allow_blocking_documents_ = pref_change_registrar_.prefs()->GetBoolean(
      vivaldiprefs::kPrivacyAdBlockerEnableDocumentBlocking);
}

void AdBlockRequestFilter::OnPingBlockingChanged() {
  block_pings_ = pref_change_registrar_.prefs()->GetBoolean(
      vivaldiprefs::kPrivacyBlockPingsEnabled);
}

bool AdBlockRequestFilter::OnBeforeRequest(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    BeforeRequestCallback callback) {
  CreateHandlerResult request_handler = CreateHandler(
      rule_service_, group_, request, allow_blocking_documents_, block_pings_);
  switch (request_handler.operation) {
    case CreateHandlerResult::kIgnore:
      return false;
    case CreateHandlerResult::kBlock:
      std::move(callback).Run(RequestFilter::kCancel, false, GURL());
      return true;
    case CreateHandlerResult::kHandler:
      CHECK(request_handler.handler);
      break;
  }

  if (!request_handler.handler->GetIndex()) {
    // Delay handling until we are fully initialized
    // Unretained is Ok, since we own the callback
    pending_.emplace(
        request->id,
        base::BindOnce(
            [](RequestHandler handler, BeforeRequestCallback callback) {
              std::move(handler).HandleBeforeRequest(std::move(callback));
            },
            std::move(*request_handler.handler), std::move(callback)));
    return true;
  }

  std::move(*request_handler.handler).HandleBeforeRequest(std::move(callback));
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
  CreateHandlerResult request_handler = CreateHandler(
      rule_service_, group_, request, allow_blocking_documents_, false);
  switch (request_handler.operation) {
    case CreateHandlerResult::kIgnore:
      return false;
    case CreateHandlerResult::kBlock:
      std::move(callback).Run(RequestFilter::kCancel, false, GURL(),
                              vivaldi::RequestFilter::ResponseHeaderChanges());
      return true;
    case CreateHandlerResult::kHandler:
      CHECK(request_handler.handler);
      break;
  }

  if (!request_handler.handler->GetIndex()) {
    // Delay handling until we are fully initialized
    // Unretained is Ok, since we own the callback
    pending_.emplace(
        request->id,
        base::BindOnce(
            [](RequestHandler handler, const net::HttpResponseHeaders* headers,
               HeadersReceivedCallback callback) {
              std::move(handler).HandleHeadersReceived(headers,
                                                       std::move(callback));
            },
            std::move(*request_handler.handler), base::Unretained(headers),
            std::move(callback)));
    return true;
  }

  std::move(*request_handler.handler)
      .HandleHeadersReceived(headers, std::move(callback));
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

void AdBlockRequestFilter::OnRequestWillBeDestroyed(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request) {
  pending_.erase(request->id);
}

}  // namespace adblock_filter
