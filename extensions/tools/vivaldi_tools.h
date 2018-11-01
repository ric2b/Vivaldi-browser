// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
#define EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_

#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

class Browser;

namespace vivaldi {

// Find first available Vivaldi browser.
Browser* FindVivaldiBrowser();

namespace spoof {
// If |request| is a request to WhatsApp, this function enforces
// a Vivaldi-free useragent
void ForceWhatsappMode(const net::URLRequest* request,
                       net::HttpRequestHeaders* headers);

}  // namespace spoof
}  // namespace vivaldi

#endif  // EXTENSIONS_TOOLS_VIVALDI_TOOLS_H_
