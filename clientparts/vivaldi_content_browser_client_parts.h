// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef VIVALDI_CONTENT_BROWSER_CLIENT_PARTS_H_
#define VIVALDI_CONTENT_BROWSER_CLIENT_PARTS_H_

#include <string>
#include <vector>

#include "chrome/browser/chrome_content_browser_client_parts.h"

namespace content {
class BrowserContext;
class BrowserURLHandler;
class RenderProcessHost;
class RenderViewHost;
class SiteInstance;
struct WebPreferences;
}

namespace storage {
class FileSystemBackend;
}

class GURL;
class Profile;

// Implements a Vivaldi specific part of ChromeContentBrowserClient.
class VivaldiContentBrowserClientParts
    : public ChromeContentBrowserClientParts {
 public:
  ~VivaldiContentBrowserClientParts() override {}

  void BrowserURLHandlerCreated(content::BrowserURLHandler* handler) override;
};

#endif  // VIVALDI_CONTENT_BROWSER_CLIENT_PARTS_H_

