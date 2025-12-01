// Copyright (c) 2019-2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_
#define EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_

#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "content/public/browser/navigation_throttle_registry.h"

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
  void CreateThrottlesForNavigation(
      content::NavigationThrottleRegistry& registry) override;

  bool CanCommitURL(content::RenderProcessHost* process_host,
                    const GURL& url) override;
#endif

#ifdef VIVALDI_V8_CONTEXT_SNAPSHOT
#if BUILDFLAG(IS_LINUX)
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
#endif
#endif

  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override;
};

#endif  // EXTRAPARTS_VIVALDI_CONTENT_BROWSER_CLIENT_H_
