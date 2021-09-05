// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/frame_host/interstitial_page_impl.h"

#include "base/metrics/user_metrics.h"

namespace content {

// Undo/Redo/Delete addeded by Vivaldi
void InterstitialPageImpl::Undo() {
  FrameTreeNode* focused_node = frame_tree_->GetFocusedFrame();
  if (!focused_node)
    return;

  focused_node->current_frame_host()->GetFrameInputHandler()->Undo();
}

void InterstitialPageImpl::Redo() {
  FrameTreeNode* focused_node = frame_tree_->GetFocusedFrame();
  if (!focused_node)
    return;

  focused_node->current_frame_host()->GetFrameInputHandler()->Redo();
}

void InterstitialPageImpl::Delete() {
  FrameTreeNode* focused_node = frame_tree_->GetFocusedFrame();
  if (!focused_node)
    return;

  focused_node->current_frame_host()->GetFrameInputHandler()->Delete();
}

}  // namespace content
