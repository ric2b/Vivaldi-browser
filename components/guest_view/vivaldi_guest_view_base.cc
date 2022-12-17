// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "components/guest_view/browser/guest_view_base.h"

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
