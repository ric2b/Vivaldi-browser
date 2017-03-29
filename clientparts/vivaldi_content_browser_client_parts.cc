// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "clientparts/vivaldi_content_browser_client_parts.h"

#include "content/public/browser/browser_url_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/search_urls.h"

namespace vivaldi {

// TODO(pettern): Move all the URLs and schemes to one file
const char kVivaldiUIScheme[] = "vivaldi";
const char kVivaldiUINewTabHost[] = "newtab";
const char kVivaldiUINewTabURL[] = "vivaldi://newtab/";
const char kVivaldiNewTabURL[] =
      "chrome-extension://mpognobbkildjkofajifpdfhcoklimli/components/"
      "startpage/startpage.html";

bool HandleVivaldiURLRewrite(GURL* url,
                             content::BrowserContext* browser_context) {
  // TODO(pettern): Add more rewrites
  if (!url->SchemeIs(kVivaldiUIScheme) ||
      url->host() != kVivaldiUINewTabHost)
    return false;

  GURL new_url(kVivaldiNewTabURL);
  if (new_url.is_valid()) {
    *url = new_url;
    return true;
  }
  return false;
}

bool HandleVivaldiURLRewriteReverse(GURL* url,
                                    content::BrowserContext* browser_context) {
  // Do nothing in incognito.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && profile->IsOffTheRecord())
    return false;

  GURL new_tab_url(kVivaldiNewTabURL);
  if (new_tab_url.is_valid() &&
      search::MatchesOriginAndPath(new_tab_url, *url)) {
    *url = GURL(vivaldi::kVivaldiUINewTabURL);
    return true;
  }
  return false;
}
}  // namespace vivaldi

void VivaldiContentBrowserClientParts::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
  // rewrite vivaldi: links to long links, and reverse
  // TODO(pettern): Enable later when the js rewrites are gone
//  handler->AddHandlerPair(&vivaldi::HandleVivaldiURLRewrite,
//                          &vivaldi::HandleVivaldiURLRewriteReverse);
}
