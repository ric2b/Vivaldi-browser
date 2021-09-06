// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef CLIENTPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_PARTS_H_
#define CLIENTPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_PARTS_H_

#include <string>
#include <vector>

#include "chrome/browser/chrome_content_browser_client_parts.h"

namespace content {
class BrowserContext;
class BrowserURLHandler;
class RenderProcessHost;
class RenderViewHost;
class SiteInstance;
}  // namespace content

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
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* web_prefs) override;
};

#endif  // CLIENTPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_PARTS_H_
