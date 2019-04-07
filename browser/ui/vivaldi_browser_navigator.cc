// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

using content::WebContents;

namespace vivaldi {

void LoadURLAsPendingEntry(WebContents* target_contents,
                           const GURL& url,
                           NavigateParams* params) {
  content::WebContentsImpl* contentsimpl =
      static_cast<content::WebContentsImpl*>(target_contents);
  content::NavigationControllerImpl* controller =
      &contentsimpl->GetController();

  std::unique_ptr<content::NavigationEntryImpl> entry =
      content::NavigationEntryImpl::FromNavigationEntry(
          controller->CreateNavigationEntry(
              url, params->referrer, params->transition,
              params->is_renderer_initiated, params->extra_headers,
              controller->GetBrowserContext(),
              nullptr /* blob_url_loader_factory */));

  controller->SetPendingEntry(std::move(entry));
  controller->SetNeedsReload();
}

}  // namespace vivaldi
