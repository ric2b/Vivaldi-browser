// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "browser/spoof/vivaldi_spoof_tools.h"

#include <string>

#include "components/google/core/browser/google_util.h"
#include "url/gurl.h"

namespace vivaldi {
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
