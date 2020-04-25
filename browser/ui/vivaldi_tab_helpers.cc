// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "browser/ui/vivaldi_tab_helpers.h"

#include "components/adverse_adblocking/vivaldi_subresource_filter_client.h"
#include "content/public/browser/web_contents.h"

#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/tabs/tabs_private_api.h"
#endif

using content::WebContents;

namespace vivaldi {
void VivaldiAttachTabHelpers(WebContents* web_contents) {
  VivaldiSubresourceFilterClient::CreateForWebContents(web_contents);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (vivaldi::IsVivaldiRunning()) {
    extensions::VivaldiPrivateTabObserver::CreateForWebContents(web_contents);
  }
#endif
}
}  // namespace vivaldi
