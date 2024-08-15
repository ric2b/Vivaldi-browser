// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
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

  auto* rfhi = static_cast<content::RenderFrameHostImpl*>(
      target_contents->GetPrimaryMainFrame());

  bool rewrite_virtual_urls = rfhi->frame_tree_node()->IsOutermostMainFrame();

  std::unique_ptr<content::NavigationEntryImpl> entry =
      content::NavigationEntryImpl::FromNavigationEntry(
          controller->CreateNavigationEntry(
              url, params->referrer, params->initiator_origin,
              params->initiator_base_url,
              std::nullopt /* source_site_instance */, params->transition,
              params->is_renderer_initiated, params->extra_headers,
              controller->GetBrowserContext(),
              nullptr /* blob_url_loader_factory */, rewrite_virtual_urls));

  if (params->post_data) {
      entry->SetHasPostData(true);
      entry->SetPostData(params->post_data);
  }

  controller->SetPendingEntry(std::move(entry));
  controller->SetNeedsReload();
}

}  // namespace vivaldi
