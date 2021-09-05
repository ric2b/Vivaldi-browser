// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_CONTENT_BROWSER_CLIENT_H_
#define COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_CONTENT_BROWSER_CLIENT_H_

#include "chrome/browser/chrome_content_browser_client.h"

class VivaldiContentBrowserClient : public ChromeContentBrowserClient {
 public:
  VivaldiContentBrowserClient(StartupData* startup_data = nullptr);

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
};

#endif  // COMPONENTS_ADVERSE_ADBLOCKING_VIVALDI_CONTENT_BROWSER_CLIENT_H_
