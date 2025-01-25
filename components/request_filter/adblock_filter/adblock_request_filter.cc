// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_request_filter.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "components/ad_blocker/adblock_resources.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_content.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/adblock_tab_handler.h"
#include "components/request_filter/adblock_filter/utils.h"
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
    case network::mojom::RequestDestination::kDocument:
      NOTREACHED();
  }
}

bool ShouldCollapse(flat::ResourceType resouce_type) {
  return resouce_type == flat::ResourceType_IMAGE ||
         resouce_type == flat::ResourceType_MEDIA ||
         resouce_type == flat::ResourceType_OBJECT ||
         resouce_type == flat::ResourceType_SUBDOCUMENT;
}

bool IsRequestWanted(GURL url) {
  return (url.SchemeIs(url::kFtpScheme) || url.SchemeIsHTTPOrHTTPS() ||
          url.SchemeIsWSOrWSS());
}

bool IsOriginWanted(content::BrowserContext* browser_context,
                    RuleGroup group,
                    url::Origin origin) {
  // Allow all requests made by extensions.
  if (origin.scheme() == "chrome-extension")
    return false;

  auto* service = RuleServiceFactory::GetForBrowserContext(browser_context);
  return !service->GetRuleManager()->IsExemptOfFiltering(group, origin);
}
}  // namespace

AdBlockRequestFilter::AdBlockRequestFilter(
    base::WeakPtr<RulesIndexManager> rules_index_manager,
    base::WeakPtr<TabHandler> tab_handler,
    base::WeakPtr<Resources> resources)
    : vivaldi::RequestFilter(vivaldi::RequestFilter::kAdBlock,
                             RuleGroupToPriority(rules_index_manager->group())),
      rules_index_manager_(std::move(rules_index_manager)),
      tab_handler_(std::move(tab_handler)),
      resources_(std::move(resources)) {}

AdBlockRequestFilter::~AdBlockRequestFilter() = default;

bool AdBlockRequestFilter::WantsExtraHeadersForAnyRequest() const {
  return false;
}

bool AdBlockRequestFilter::WantsExtraHeadersForRequest(
    vivaldi::FilteredRequestInfo* request) const {
  return false;
}

bool AdBlockRequestFilter::OnBeforeRequest(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    BeforeRequestCallback callback) {
  auto destination = request->request.destination;

  bool is_main_frame =
      destination == network::mojom::RequestDestination::kDocument;
  bool is_frame = is_main_frame ||
                  destination == network::mojom::RequestDestination::kIframe ||
                  destination == network::mojom::RequestDestination::kFrame;

  url::Origin document_origin;
  if (is_main_frame || !request->request.request_initiator)
    document_origin = url::Origin::Create(request->request.url);
  else
    document_origin = request->request.request_initiator.value();

  // TODO(julien): Add filtering of csp reports
  if (!rules_index_manager_ || !rules_index_manager_->rules_index() ||
      destination == network::mojom::RequestDestination::kReport ||
      !IsRequestWanted(request->request.url)) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  bool is_third_party = IsThirdParty(request->request.url, document_origin);

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  std::optional<GURL> url_for_activations;
  std::optional<url::Origin> origin_for_activations;
  if (is_frame && frame) {
    url_for_activations = request->request.url;
    origin_for_activations = document_origin;
  }

  RulesIndex::ActivationResults activations =
      rules_index_manager_->rules_index()->GetActivationsForFrame(
          base::BindRepeating(&IsOriginWanted, browser_context,
                              rules_index_manager_->group()),
          frame, url_for_activations, origin_for_activations);

  std::optional<flat::Decision> document_decision =
      activations[flat::ActivationType_DOCUMENT].GetDecision();

  if (is_main_frame && document_decision &&
      document_decision != flat::Decision_PASS) {
    std::move(callback).Run(true, false, GURL());
    return true;
  }

  if (is_main_frame ||
      (document_decision && document_decision == flat::Decision_PASS)) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  bool disable_generic_rules =
      activations[flat::ActivationType_GENERIC_BLOCK].GetDecision().value_or(
          flat::Decision_MODIFY) == flat::Decision_PASS;

  const flat::ResourceType resource_type = ResourceTypeFromRequest(*request);
  const flat::RequestFilterRule* rule =
      rules_index_manager_->rules_index()->FindMatchingBeforeRequestRule(
          request->request.url, document_origin, resource_type, is_third_party,
          disable_generic_rules);

  CHECK(!rule || rule->options() & flat::OptionFlag_MODIFY_BLOCK);

  if (!rule || rule->decision() == flat::Decision_PASS) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  if (tab_handler_ && frame)
    tab_handler_->OnUrlBlocked(rules_index_manager_->group(), document_origin,
                               request->request.url, frame);

  RulesIndex::FoundModifiersByType modifiers_by_type =
      rules_index_manager_->rules_index()->FindMatchingRequestModifierRules(
          request->request.url, document_origin, resource_type, is_third_party,
          disable_generic_rules);

  RulesIndex::FoundModifiers& redirects =
      modifiers_by_type[flat::Modifier_REDIRECT];

  if (!redirects.value_with_decision.empty() && resources_) {
    auto redirect = std::max_element(
        redirects.value_with_decision.begin(),
        redirects.value_with_decision.end(),
        [this, resource_type](auto& lhs, auto& rhs) {
          if (!resources_->GetRedirect(lhs.first, resource_type)) {
            return true;
          }
          if (!resources_->GetRedirect(rhs.first, resource_type)) {
            return false;
          }

          return GetRulePriority(*lhs.second) < GetRulePriority(*rhs.second);
        });

    std::optional<std::string> resource(
        resources_->GetRedirect(redirect->first, resource_type));
    if (resource) {
      std::move(callback).Run(false, false, GURL(resource.value()));
      return true;
    }
  }

  std::move(callback).Run(
      true, ShouldCollapse(ResourceTypeFromRequest(*request)), GURL());
  return true;
}

bool AdBlockRequestFilter::OnBeforeSendHeaders(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpRequestHeaders* headers,
    BeforeSendHeadersCallback callback) {
  std::move(callback).Run(false,
                          vivaldi::RequestFilter::RequestHeaderChanges());
  return true;
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

  if (!rules_index_manager_ || !rules_index_manager_->rules_index() ||
      destination == network::mojom::RequestDestination::kReport ||
      !IsRequestWanted(request->request.url)) {
    std::move(callback).Run(false, false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  bool is_third_party = IsThirdParty(request->request.url, document_origin);
  bool is_frame =
      destination == network::mojom::RequestDestination::kDocument ||
      destination == network::mojom::RequestDestination::kIframe ||
      destination == network::mojom::RequestDestination::kFrame;

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  std::optional<GURL> url_for_activations;
  std::optional<url::Origin> origin_for_activations;
  if (is_frame && frame) {
    url_for_activations = request->request.url;
    origin_for_activations = document_origin;
  }

  RulesIndex::ActivationResults activations =
      rules_index_manager_->rules_index()->GetActivationsForFrame(
          base::BindRepeating(&IsOriginWanted, browser_context,
                              rules_index_manager_->group()),
          frame, url_for_activations, origin_for_activations);

  if (activations[flat::ActivationType_DOCUMENT].GetDecision().value_or(
          flat::Decision_MODIFY) == flat::Decision_PASS) {
    std::move(callback).Run(false, false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  RulesIndex::FoundModifiersByType modifiers_by_type =
      rules_index_manager_->rules_index()->FindMatchingHeadersReceivedRules(
          request->request.url, document_origin, flat::ResourceType_ANY,
          is_third_party,
          (activations[flat::ActivationType_GENERIC_BLOCK]
               .GetDecision()
               .value_or(flat::Decision_MODIFY) == flat::Decision_PASS));

  RulesIndex::FoundModifiers& csp = modifiers_by_type[flat::Modifier_CSP];

  if (csp.value_with_decision.empty()) {
    std::move(callback).Run(false, false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  std::set<std::string> added_headers;

  for (const auto& [value, rule] : csp.value_with_decision) {
    if (rule->decision() == flat::Decision_PASS) {
      continue;
    }
    added_headers.insert(value);
  }

  vivaldi::RequestFilter::ResponseHeaderChanges response_header_changes;
  for (const auto& added_header : added_headers) {
    response_header_changes.headers_to_add.push_back(
        std::make_pair("Content-Security-Policy", added_header));
  }

  std::move(callback).Run(false, false, GURL(),
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
