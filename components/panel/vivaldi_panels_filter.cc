// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/panel/vivaldi_panels_filter.h"
#include "chromium/extensions/common/constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/helper/vivaldi_tab_utils.h"
#include "url/gurl.h"

namespace {

bool IsPanelSensitiveUrl(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs("vivaldi")) {
    return true;
  }
  return false;
}

}  // namespace

VivaldiPanelsThrottle::VivaldiPanelsThrottle(content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

VivaldiPanelsThrottle::~VivaldiPanelsThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
VivaldiPanelsThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();

  bool incognito = false;
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  CHECK(web_contents);
  incognito = web_contents->GetBrowserContext()->IsOffTheRecord();
  if (incognito && IsPanelSensitiveUrl(url)) {
    auto page_type = ::vivaldi::GetVivaldiPanelType(web_contents);
    if (!::vivaldi::IsPage(page_type)) {
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }
  }

  return content::NavigationThrottle::PROCEED;
}

const char* VivaldiPanelsThrottle::GetNameForLogging() {
  return "VivaldiPanelsThrottle";
}

bool VivaldiPanelsThrottle::IsRelevant(content::NavigationHandle* handle) {
  return true;
}
