// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_FRAME_TOOLS_H_
#define COMPONENTS_CONTENT_FRAME_TOOLS_H_

namespace content {
class RenderFrameHost;
}

namespace vivaldi {
// Returns false if we should run find-in-page operations on the RFH. Avoid
// jumping into our UI.
bool IsFindInPageDisabled(content::RenderFrameHost* rfh);

// Returns true if the rfh is part of the top level UI-frame in Vivaldi.
bool IsFramePartOfTheVivaldiUI(content::RenderFrameHost* rfh);
}  // namespace vivaldi

#endif  // COMPONENTS_CONTENT_FRAME_TOOLS_H_
