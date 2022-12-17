// Copyright (c) 2019-2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_
#define EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_

#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"

namespace content {
class RenderFrameHost;
}

class VivaldiContentBrowserClient : public ChromeContentBrowserClient {
 public:
  VivaldiContentBrowserClient();

  ~VivaldiContentBrowserClient() override;

  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;

#if !BUILDFLAG(IS_ANDROID)
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;

  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
#endif

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
};

#endif  // EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_
