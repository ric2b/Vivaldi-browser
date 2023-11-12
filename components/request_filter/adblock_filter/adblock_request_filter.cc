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
#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"
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
      return flat::ResourceType_SCRIPT;
    case network::mojom::RequestDestination::kImage:
      return flat::ResourceType_IMAGE;
    case network::mojom::RequestDestination::kFont:
      return flat::ResourceType_FONT;
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kWebBundle:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kDictionary:
      return flat::ResourceType_OTHER;
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
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kReport:
      NOTREACHED();
      return flat::ResourceType_OTHER;
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
    base::WeakPtr<BlockedUrlsReporter> blocked_urls_reporter,
    base::WeakPtr<Resources> resources)
    : vivaldi::RequestFilter(vivaldi::RequestFilter::kAdBlock,
                             RuleGroupToPriority(rules_index_manager->group())),
      rules_index_manager_(std::move(rules_index_manager)),
      blocked_urls_reporter_(std::move(blocked_urls_reporter)),
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
      !IsRequestWanted(request->request.url) ||
      !IsOriginWanted(browser_context, rules_index_manager_->group(),
                      document_origin)) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  bool is_third_party = IsThirdParty(request->request.url, document_origin);

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  RulesIndex::ActivationsFound activations =
      rules_index_manager_->rules_index()->GetActivationsForFrame(
          base::BindRepeating(&IsOriginWanted, browser_context,
                              rules_index_manager_->group()),
          (is_frame && frame) ? frame->GetParent() : frame);

  if (is_frame && frame) {
    RulesIndex::ActivationsFound request_activations =
        rules_index_manager_->rules_index()->FindMatchingActivationsRules(
            request->request.url, document_origin, is_third_party, frame);
    request_activations.in_allow_rules |= activations.in_allow_rules;
    request_activations.in_block_rules |= activations.in_block_rules;
    activations = request_activations;
  }

  if ((is_main_frame &&
       (activations.in_block_rules & flat::ActivationType_DOCUMENT) != 0)) {
    std::move(callback).Run(true, false, GURL());
    return true;
  }

  if (is_main_frame ||
      (activations.in_allow_rules & flat::ActivationType_DOCUMENT) != 0) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  const flat::ResourceType resource_type = ResourceTypeFromRequest(*request);
  const flat::RequestFilterRule* rule =
      rules_index_manager_->rules_index()->FindMatchingBeforeRequestRule(
          request->request.url, document_origin, resource_type, is_third_party,
          (activations.in_allow_rules & flat::ActivationType_GENERIC_BLOCK));

  if (!rule || rule->options() & flat::OptionFlag_IS_ALLOW_RULE) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  if (blocked_urls_reporter_ && frame)
    blocked_urls_reporter_->OnUrlBlocked(rules_index_manager_->group(),
                                         document_origin, request->request.url,
                                         frame);

  if (rule->redirect() && rule->redirect()->size() && resources_) {
    absl::optional<std::string> resource(
        resources_->GetRedirect(rule->redirect()->c_str(), resource_type));
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
      !IsRequestWanted(request->request.url) ||
      !IsOriginWanted(browser_context, rules_index_manager_->group(),
                      document_origin)) {
    std::move(callback).Run(false, GURL(),
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

  RulesIndex::ActivationsFound activations =
      rules_index_manager_->rules_index()->GetActivationsForFrame(
          base::BindRepeating(&IsOriginWanted, browser_context,
                              rules_index_manager_->group()),
          (is_frame && frame) ? frame->GetParent() : frame);

  if (is_frame && frame) {
    RulesIndex::ActivationsFound request_activations =
        rules_index_manager_->rules_index()->FindMatchingActivationsRules(
            request->request.url, document_origin, is_third_party, frame);
    request_activations.in_allow_rules |= activations.in_allow_rules;
    activations = request_activations;
  }

  if ((activations.in_allow_rules & flat::ActivationType_DOCUMENT) != 0) {
    std::move(callback).Run(false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  std::vector<const flat::RequestFilterRule*> rules =
      rules_index_manager_->rules_index()->FindMatchingHeadersReceivedRules(
          request->request.url, document_origin, is_third_party,
          (activations.in_allow_rules & flat::ActivationType_GENERIC_BLOCK));

  if (rules.size() == 1 && IsFullCSPAllowRule(*rules[0])) {
    std::move(callback).Run(false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  std::set<std::string> added_headers;
  std::vector<std::string> un_added_headers;

  for (auto* rule : rules) {
    if ((rule->options() & flat::OptionFlag_IS_ALLOW_RULE) == 0) {
      added_headers.insert(rule->csp()->str());
    } else {
      un_added_headers.push_back(rule->csp()->str());
    }
  }

  for (const auto& un_added_header : un_added_headers) {
    added_headers.erase(un_added_header);
  }

  vivaldi::RequestFilter::ResponseHeaderChanges response_header_changes;
  for (const auto& added_header : added_headers) {
    response_header_changes.headers_to_add.push_back(
        std::make_pair("Content-Security-Policy", added_header));
  }

  std::move(callback).Run(false, GURL(), std::move(response_header_changes));
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
