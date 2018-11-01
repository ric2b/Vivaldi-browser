// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/tools/vivaldi_tools.h"

#include <string>

#include "chrome/browser/ui/browser_list.h"
#include "components/google/core/browser/google_util.h"
#include "url/gurl.h"

namespace vivaldi {

Browser* FindVivaldiBrowser() {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (browser_list_impl && browser_list_impl->size() > 0)
    return browser_list_impl->get(0);
  return NULL;
}

namespace spoof {
// True if |url| is a valid whatsapp.<TLD> URL.
bool IsWhatsappDomainUrl(const GURL& url) {
  return google_util::IsValidURL(url,
                                 google_util::DISALLOW_NON_STANDARD_PORTS) &&
         google_util::IsValidHostName(url.host_piece(), "whatsapp",
                                      google_util::ALLOW_SUBDOMAIN);
}

void ForceWhatsappMode(const net::URLRequest* request,
                       net::HttpRequestHeaders* headers) {
  if (IsWhatsappDomainUrl(request->url())) {
    std::string useragent;
    if (headers->GetHeader(net::HttpRequestHeaders::kUserAgent, &useragent)) {
      std::size_t found = useragent.find(" Vivaldi/");
      if (found != std::string::npos) {
        useragent.resize(found);
        headers->SetHeader(net::HttpRequestHeaders::kUserAgent, useragent);
      }
    }
  }
}

}  // namespace spoof
}  // namespace vivaldi
