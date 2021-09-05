// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/adblock_request_filter.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "components/request_filter/adblock_filter/adblock_rule_service.h"
#include "components/request_filter/adblock_filter/adblock_rule_service_factory.h"
#include "components/request_filter/adblock_filter/adblock_rules_index.h"
#include "components/request_filter/adblock_filter/blocked_urls_reporter.h"
#include "components/request_filter/adblock_filter/utils.h"
#include "components/request_filter/filtered_request_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace adblock_filter {

namespace {
const std::array<std::string, flat::RedirectResource_MAX + 1>
    kRedirectResources = {
        "",                              // No Redirect
        "data:text/plain,",              // blank-text
        "data:text/css,",                // blank-css
        "data:application/javascript,",  // blank-js
        // blank-html
        "data:text/html,<!DOCTYPE html><html><head></head><body></body></html>",
        // blank-mp3
        "data:audio/"
        "mpeg;base64,"
        "SUQzBAAAAAAAI1RTU0UAAAAPAAADTGF2ZjU4LjIwLjEwMAAAAAAAAAAAAAAA//"
        "tUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAASW5mbwAAAA8AAAAGAAADAAB"
        "gYGBgYGBgYGBgYGBgYGBggICAgICAgICAgICAgICAgICgoKCgoKCgoKCgoKCgoKCgwMDA"
        "wMDAwMDAwMDAwMDAwMDg4ODg4ODg4ODg4ODg4ODg4P////////////////////"
        "8AAAAATGF2YzU4LjM1AAAAAAAAAAAAAAAAJAYAAAAAAAAAAwDVxttG//"
        "sUZAAP8AAAaQAAAAgAAA0gAAABAAABpAAAACAAADSAAAAETEFNRTMuMTAwVVVVVVVVVVV"
        "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV//"
        "sUZB4P8AAAaQAAAAgAAA0gAAABAAABpAAAACAAADSAAAAEVVVVVVVVVVVVVVVVVVVVVVV"
        "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV//"
        "sUZDwP8AAAaQAAAAgAAA0gAAABAAABpAAAACAAADSAAAAEVVVVVVVVVVVVVVVVVVVVVVV"
        "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV//"
        "sUZFoP8AAAaQAAAAgAAA0gAAABAAABpAAAACAAADSAAAAEVVVVVVVVVVVVVVVVVVVVVVV"
        "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV//"
        "sUZHgP8AAAaQAAAAgAAA0gAAABAAABpAAAACAAADSAAAAEVVVVVVVVVVVVVVVVVVVVVVV"
        "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV//"
        "sUZJYP8AAAaQAAAAgAAA0gAAABAAABpAAAACAAADSAAAAEVVVVVVVVVVVVVVVVVVVVVVV"
        "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        // blank-mp4
        "data:video/"
        "mp4;base64,"
        "AAAAIGZ0eXBpc29tAAACAGlzb21pc28yYXZjMW1wNDEAAAAIZnJlZQAAAAhtZGF0AAAA1"
        "m1vb3YAAABsbXZoZAAAAAAAAAAAAAAAAAAAA+"
        "gAAAAAAAEAAAEAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAA"
        "AQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAABidWR0YQAAAFptZXRhAAAA"
        "AAAAACFoZGxyAAAAAAAAAABtZGlyYXBwbAAAAAAAAAAAAAAAAC1pbHN0AAAAJal0b28AA"
        "AAdZGF0YQAAAAEAAAAATGF2ZjU3LjQxLjEwMA==",
        // 1x1-transparent-gif
        "data:image/gif;base64,R0lGODlhAQABAIABAAAAAP///"
        "yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==",
        // 2x2-transparent-png
        "data:image/"
        "png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAC0lEQVQI12NgQAcAABIAA"
        "e+JVKQAAAAASUVORK5CYII=",
        // 3x2-transparent-png
        "data:image/"
        "png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAYAAACddGYaAAAAC0lEQVQI12NgwAUAABoAA"
        "SRETuUAAAAASUVORK5CYII=",
        // 32x32-transparent-png
        "data:image/"
        "png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAAGklEQVRYw+"
        "3BAQEAAACCIP+vbkhAAQAAAO8GECAAAZf3V9cAAAAASUVORK5CYII="};

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
  if (request.request.url.SchemeIsWSOrWSS())
    return flat::ResourceType_WEBSOCKET;
  else if (request.loader_factory_type ==
           content::ContentBrowserClient::URLLoaderFactoryType::kDownload)
    return flat::ResourceType_OTHER;

  switch (
      static_cast<blink::mojom::ResourceType>(request.request.resource_type)) {
    case blink::mojom::ResourceType::kSubFrame:
    case blink::mojom::ResourceType::kNavigationPreloadSubFrame:
      return flat::ResourceType_SUBDOCUMENT;
    case blink::mojom::ResourceType::kStylesheet:
      return flat::ResourceType_STYLESHEET;
    case blink::mojom::ResourceType::kScript:
    case blink::mojom::ResourceType::kWorker:
    case blink::mojom::ResourceType::kSharedWorker:
    case blink::mojom::ResourceType::kServiceWorker:
      return flat::ResourceType_SCRIPT;
    case blink::mojom::ResourceType::kImage:
    case blink::mojom::ResourceType::kFavicon:
      return flat::ResourceType_IMAGE;
    case blink::mojom::ResourceType::kFontResource:
      return flat::ResourceType_FONT;
    case blink::mojom::ResourceType::kSubResource:
    case blink::mojom::ResourceType::kPrefetch:
      return flat::ResourceType_OTHER;
    case blink::mojom::ResourceType::kObject:
    case blink::mojom::ResourceType::kPluginResource:
      return flat::ResourceType_OBJECT;
    case blink::mojom::ResourceType::kMedia:
      return flat::ResourceType_MEDIA;
    case blink::mojom::ResourceType::kXhr:
      return flat::ResourceType_XMLHTTPREQUEST;
    case blink::mojom::ResourceType::kPing:
      return flat::ResourceType_PING;
    case blink::mojom::ResourceType::kMainFrame:
    case blink::mojom::ResourceType::kNavigationPreloadMainFrame:
    case blink::mojom::ResourceType::kCspReport:
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
  return !service->IsExemptOfFiltering(group, origin);
}
}  // namespace

AdBlockRequestFilter::AdBlockRequestFilter(
    RuleGroup group,
    base::WeakPtr<RulesIndexManager> rules_index_manager,
    base::WeakPtr<BlockedUrlsReporter> blocked_urls_reporter)
    : vivaldi::RequestFilter(vivaldi::RequestFilter::kAdBlock,
                             RuleGroupToPriority(group)),
      rules_index_manager_(std::move(rules_index_manager)),
      blocked_urls_reporter_(std::move(blocked_urls_reporter)),
      group_(group) {}

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
  auto resource_type =
      static_cast<blink::mojom::ResourceType>(request->request.resource_type);

  url::Origin document_origin = request->request.request_initiator.value_or(
      url::Origin::Create(request->request.url));

  // TODO(julien): Add filtering of csp reports
  if (!rules_index_manager_ || !rules_index_manager_->rules_index() ||
      resource_type == blink::mojom::ResourceType::kCspReport ||
      !IsRequestWanted(request->request.url) ||
      !IsOriginWanted(browser_context, group_, document_origin)) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  bool is_main_frame =
      resource_type == blink::mojom::ResourceType::kMainFrame ||
      resource_type == blink::mojom::ResourceType::kNavigationPreloadMainFrame;
  bool is_frame =
      is_main_frame || resource_type == blink::mojom::ResourceType::kSubFrame ||
      resource_type == blink::mojom::ResourceType::kNavigationPreloadSubFrame;

  bool is_third_party = IsThirdParty(request->request.url, document_origin);

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  RulesIndex::ActivationsFound activations =
      rules_index_manager_->rules_index()->GetActivationsForFrame(
          base::Bind(&IsOriginWanted, browser_context, group_),
          (is_frame && frame) ? frame->GetParent() : frame);

  if (is_frame && frame) {
    RulesIndex::ActivationsFound request_activations =
        rules_index_manager_->rules_index()->FindMatchingActivationsRules(
            request->request.url, document_origin, is_third_party, frame);
    request_activations.in_allow_rules |= activations.in_allow_rules;
    activations = request_activations;
  }

  if (is_main_frame ||
      (activations.in_allow_rules & flat::ActivationType_DOCUMENT) != 0) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  const flat::FilterRule* filter_rule =
      rules_index_manager_->rules_index()->FindMatchingBeforeRequestRule(
          request->request.url, document_origin,
          ResourceTypeFromRequest(*request), is_third_party,
          (activations.in_allow_rules & flat::ActivationType_GENERIC_BLOCK));

  if (!filter_rule || filter_rule->options() & flat::OptionFlag_IS_ALLOW_RULE) {
    std::move(callback).Run(false, false, GURL());
    return true;
  }

  if (blocked_urls_reporter_ && frame)
    blocked_urls_reporter_->OnUrlBlocked(group_, request->request.url, frame);

  if (filter_rule->redirect() != flat::RedirectResource_NO_REDIRECT) {
    std::move(callback).Run(false, false,
                            GURL(kRedirectResources[filter_rule->redirect()]));
    return true;
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
  auto resource_type =
      static_cast<blink::mojom::ResourceType>(request->request.resource_type);

  url::Origin document_origin = request->request.request_initiator.value_or(
      url::Origin::Create(request->request.url));

  if (!rules_index_manager_ || !rules_index_manager_->rules_index() ||
      resource_type == blink::mojom::ResourceType::kCspReport ||
      !IsRequestWanted(request->request.url) ||
      !IsOriginWanted(browser_context, group_, document_origin)) {
    std::move(callback).Run(false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  bool is_third_party = IsThirdParty(request->request.url, document_origin);
  bool is_frame =
      resource_type == blink::mojom::ResourceType::kMainFrame ||
      resource_type ==
          blink::mojom::ResourceType::kNavigationPreloadMainFrame ||
      resource_type == blink::mojom::ResourceType::kSubFrame ||
      resource_type == blink::mojom::ResourceType::kNavigationPreloadSubFrame;

  content::RenderFrameHost* frame = content::RenderFrameHost::FromID(
      request->render_process_id, request->render_frame_id);

  RulesIndex::ActivationsFound activations =
      rules_index_manager_->rules_index()->GetActivationsForFrame(
          base::Bind(&IsOriginWanted, browser_context, group_),
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

  std::vector<const flat::FilterRule*> filter_rules =
      rules_index_manager_->rules_index()->FindMatchingHeadersReceivedRules(
          request->request.url, document_origin, is_third_party,
          (activations.in_allow_rules & flat::ActivationType_GENERIC_BLOCK));

  if (filter_rules.size() == 1 && IsFullCSPAllowRule(*filter_rules[0])) {
    std::move(callback).Run(false, GURL(),
                            vivaldi::RequestFilter::ResponseHeaderChanges());
    return true;
  }

  std::set<std::string> added_headers;
  std::vector<std::string> un_added_headers;

  for (auto* rule : filter_rules) {
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
