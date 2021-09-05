// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/adblock_filter/interstitial/document_blocked_controller_client.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/web_contents.h"

namespace adblock_filter {
namespace {

std::unique_ptr<security_interstitials::MetricsHelper> GetMetricsHelper(
    const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "document-blocked";

  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}
}  // namespace

DocumentBlockedControllerClient::DocumentBlockedControllerClient(
    content::WebContents* web_contents,
    const GURL& request_url)
    : SecurityInterstitialControllerClient(
          web_contents,
          GetMetricsHelper(request_url),
          Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->GetPrefs(),
          g_browser_process->GetApplicationLocale(),
          GURL(chrome::kChromeUINewTabURL),
          /*settings_page_helper=*/nullptr),
      request_url_(request_url) {}

DocumentBlockedControllerClient::~DocumentBlockedControllerClient() {}

void DocumentBlockedControllerClient::GoBack() {
  if (CanGoBack())
    web_contents_->GetController().GoBack();
}

void DocumentBlockedControllerClient::Proceed() {
  Reload();
}
}  // namespace adblock_filter