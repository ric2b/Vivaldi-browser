// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "components/guest_view/browser/guest_view_base.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "extensions/helper/vivaldi_init_helpers.h"

namespace guest_view {

void GuestViewBase::WebContentsDidDetach() {
}

content::WebContentsDelegate* GuestViewBase::GetDevToolsConnector() {
  return this;
}

gfx::Size GuestViewBase::GetSizeForNewRenderView(
    content::WebContents* web_contents) {
  return normal_size_;
}

}  // namespace guest_view
