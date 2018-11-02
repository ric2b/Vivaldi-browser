// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_SPOOF_VIVALDI_SPOOF_TOOLS_H_
#define BROWSER_SPOOF_VIVALDI_SPOOF_TOOLS_H_

#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace vivaldi {

namespace spoof {
// If |request| is a request to WhatsApp, this function enforces
// a Vivaldi-free useragent
void ForceWhatsappMode(const net::URLRequest* request,
                       net::HttpRequestHeaders* headers);

}  // namespace spoof
}  // namespace vivaldi

#endif  // BROWSER_SPOOF_VIVALDI_SPOOF_TOOLS_H_
