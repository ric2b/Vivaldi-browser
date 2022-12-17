// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include "components/ping_block/ping_block.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace vivaldi {

PingBlockerFilter::PingBlockerFilter()
    : vivaldi::RequestFilter(vivaldi::RequestFilter::kPingBlock, 1) {}

bool PingBlockerFilter::WantsExtraHeadersForAnyRequest() const {
  return false;
}

bool PingBlockerFilter::WantsExtraHeadersForRequest(
    vivaldi::FilteredRequestInfo* request) const {
  return false;
}

bool PingBlockerFilter::OnBeforeRequest(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    BeforeRequestCallback callback) {
  bool accept = true;
  if (request->request.resource_type ==
      static_cast<int>(blink::mojom::ResourceType::kPing)) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    PrefService* prefs = profile->GetPrefs();
    if (!prefs->GetBoolean(prefs::kEnableHyperlinkAuditing))
      accept = false;
  }
  if (accept) {
    return false;
  }

  std::move(callback).Run(true, false, GURL());
  return true;
}

bool PingBlockerFilter::OnBeforeSendHeaders(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpRequestHeaders* headers,
    BeforeSendHeadersCallback callback) {
  return false;
}

void PingBlockerFilter::OnSendHeaders(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpRequestHeaders& headers) {}

bool PingBlockerFilter::OnHeadersReceived(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const net::HttpResponseHeaders* headers,
    HeadersReceivedCallback callback) {
  return false;
}

void PingBlockerFilter::OnBeforeRedirect(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    const GURL& redirect_url) {}

void PingBlockerFilter::OnResponseStarted(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request) {}

void PingBlockerFilter::OnCompleted(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request) {}

void PingBlockerFilter::OnErrorOccured(
    content::BrowserContext* browser_context,
    const vivaldi::FilteredRequestInfo* request,
    int net_error) {}

}  // namespace vivaldi
