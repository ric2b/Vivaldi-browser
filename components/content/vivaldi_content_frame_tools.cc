// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.

#include "components/content/vivaldi_content_frame_tools.h"

#include "app/vivaldi_constants.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "url/origin.h"

namespace vivaldi {

bool IsFindInPageDisabled(content::RenderFrameHost* rfh) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (rfh) {
    // Is this the PDF extension UI?
    url::Origin origin = rfh->GetLastCommittedOrigin();
    if (origin.scheme() == extensions::kExtensionScheme &&
        origin.host() == extension_misc::kPdfExtensionId) {
      return true;
    }

    content::WebContents* const web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    if (!web_contents) {
      return true;
    }

    // Is this a WebViewGuest?
    content::RenderWidgetHostViewBase* view =
        static_cast<content::RenderWidgetHostViewBase*>(
            rfh->GetRenderViewHost()->GetWidget()->GetView());
    // See if its a child/webview.
    if (view && view->IsRenderWidgetHostViewChildFrame()) {
      return false;
    }
    // If no view then this is a subframe inside a page. (Iframe, frame inside
    // pdf viewer etc.)
    if (!view) {
      return false;
    }

    return true;
  }
#endif  // !IS_ANDROID && !IS_IOS
  return false;
}

bool IsFramePartOfTheVivaldiUI(content::RenderFrameHost* rfh) {
  return (rfh->GetLastCommittedURL() == vivaldi::kVivaldiUIWindowURL);
}

}  // namespace vivaldi
