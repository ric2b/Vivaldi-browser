// Copyright (c) 2019-2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_
#define EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_

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
      const content::MainFunctionParams& parameters) override;

#if !defined(OS_ANDROID)
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;

  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
#endif

  std::string GetStoragePartitionIdForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;

};

#endif  // EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_
