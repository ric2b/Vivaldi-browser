// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "components/guest_view/browser/guest_view_base.h"

namespace guest_view {

void GuestViewBase::WebContentsDidDetach() {
  // We can now safely do any pending attaching.
  if (perform_attach_callback_)
    std::move(perform_attach_callback_).Run();
  if (attach_completion_callback_)
    SignalWhenReady(std::move(attach_completion_callback_));
}

content::WebContentsDelegate* GuestViewBase::GetDevToolsConnector() {
  return this;
}

}  // namespace guest_view
