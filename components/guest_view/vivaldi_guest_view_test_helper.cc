// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

namespace content {
class WebContents;
}

///////////////////////////////////////////////////////////////////////////////
// Helpers for the guest_view unit tests (since they don't link with the
// browser version.
///////////////////////////////////////////////////////////////////////////////
namespace guest_view {
// Vivaldi helper function that is defined multiple places.
// Returns true if a Browser object owns and manage the lifecycle of the
// |content::WebContents|

// declared in src/components/guest_view/browser/guest_view_base.h
void AttachWebContentsObservers(content::WebContents* contents) {}

}  // namespace guest_view
