// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/frame_host/frame_tree_node.h"

#include "app/vivaldi_apptools.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/web_contents/web_contents_view_child_frame.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/content/vivaldi_tab_check.h"

namespace content {

void WebContentsImpl::SetExtData(const std::string& ext_data) {
  ext_data_ = ext_data;
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->ExtDataSet(this); });

  NotificationService::current()->Notify(
    NOTIFICATION_EXTDATA_UPDATED,
    Source<WebContents>(this),
    NotificationService::NoDetails());
}

const std::string& WebContentsImpl::GetExtData() const {
  return ext_data_;
}

void FrameTreeNode::DidChangeLoadProgressExtended(double load_progress,
  double loaded_bytes,
  int loaded_elements,
  int total_elements) {
  loaded_bytes_ = loaded_bytes;
  loaded_elements_ = loaded_elements;
  total_elements_ = total_elements;

  frame_tree_->UpdateLoadProgress(load_progress);
}

void WebContentsImpl::FrameTreeNodeDestroyed() {
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->WebContentsDidDetach(); });
}

void WebContentsImpl::AttachedToOuter() {
  observers_.ForEachObserver(
      [&](WebContentsObserver* observer) { observer->WebContentsDidAttach(); });
}

void WebContentsImpl::WebContentsTreeNode::VivaldiDestructor() {
  // NOTE(andre@vivaldi.com) : If we have a MimeHandlerViewGuest in an iframe
  // it would not get a OnFrameTreeNodeDestroyed call in time because it would
  // be destroyed when the embedder is destroyed.
  if (OuterContentsFrameTreeNode()) {
    OuterContentsFrameTreeNode()->RemoveObserver(this);
  }
  if (outer_web_contents_) {
    int current_process_id =
      outer_web_contents_->GetRenderViewHost()->GetProcess()->GetID();
    auto* child_rfh = outer_web_contents_->FindFrameByFrameTreeNodeId(
      outer_contents_frame_tree_node_id_, current_process_id);
    if (child_rfh) {
      child_rfh->frame_tree_node()->RemoveObserver(this);

      // TODO(igor@vivaldi.com): This method is called after the code in
      // ~WebContentsImpl() run and, since the declaration of the frame_tree_
      // field comes after the node_ field in WebContentsImpl, after the
      // destructor for FrameTree. So current_web_contents_->GetFrameTree()
      // returns a pointer to FrameTree after its destructor is run. Figure out
      // if this is safe to call here.
      current_web_contents_->GetFrameTree()->RemoveFrame(
          child_rfh->frame_tree_node());
    }

    // This is an unsupported case, but if the inner webcontents of the outer
    // contents has been destroyed, discarded, we won't get notified. Check if
    // it is attached and remove it if it is.

    for (auto* outers_inner_contents :
          outer_web_contents_->GetInnerWebContents()) {
      if (outers_inner_contents == current_web_contents_) {
        // Detach inner so the WebContents is not destroyed, it is destroyed
        // by the |TabStripModel|. This also makes sure there is no dangling
        // pointers to current_web_contents_ when a WebContents is deleted
        // without the FrameTreeNode being removed.
        outer_web_contents_->node_
            .DetachInnerWebContents(current_web_contents_)
            .release();
        break;
      }
    }
  }

  // NOTE(andre@vivaldi.com) : Any inner_web_contents_ items has already been
  // freed at this point.
  for (std::unique_ptr<WebContents>& web_contents : inner_web_contents_) {
    web_contents.release();
  }
}

void WebContentsImpl::WebContentsTreeNode::VivaldiDetachExternallyOwned(
    FrameTreeNode* node) {
  DCHECK(VivaldiTabCheck::IsOwnedByTabStripOrDevTools(current_web_contents_));

  // We're detaching from the outer contents, so move focus away from
  // us early to avoid crashers later. This section is taken
  // from the destructor of WebContentsImpl. Note that this must be done
  // before clearing the proxy hosts.
  WebContentsImpl* outermost =
      current_web_contents_->GetOutermostWebContents();
  if (current_web_contents_->ContainsOrIsFocusedWebContents()) {
    // If the current WebContents is in focus, unset it.
    outermost->SetAsFocusedWebContentsIfNecessary();
  }

  // Disconnect the view hierarhy from the text_input.
  // NOTE(igor@vivaldi.com) This also clears the text input state for each
  // view that is stored is a hash map in TextInputManager. It seems
  // harmless as the state is only relevant when IME is active and when the
  // tabs are moved it is not. But if preserving the state will be necessary,
  // then fixing this may require significant changes to TextInputManager data
  // structures.
  if (outermost->text_input_manager_) {
    for (WebContentsImpl* contents :
          current_web_contents_->GetWebContentsAndAllInner()) {
      auto* view = static_cast<RenderWidgetHostViewBase*>(
          contents->GetRenderManager()->GetRenderWidgetHostView());
      if (view && outermost->text_input_manager_->IsRegistered(view)) {
        outermost->text_input_manager_->Unregister(view);
      }
    }
  }

  // Make sure this isn't freed here. It is owned by the TabStrip.
  current_web_contents_->DetachFromOuterWebContents().release();

  if (outer_web_contents_) {
    // Detach inner so the WebContents is not destroyed, it is destroyed by
    // the |TabStripModel| or DevTools.
    outer_web_contents_->node_.DetachInnerWebContents(current_web_contents_)
      .release();
  }
  outer_web_contents_ = nullptr;
  outer_contents_frame_tree_node_id_ = FrameTreeNode::kFrameTreeNodeInvalidId;

  node->RemoveObserver(this);

  // This is Vivaldi specific to be able to sync the mounting in the client
  // after it has been detached.
  current_web_contents_->FrameTreeNodeDestroyed();
}

}  // namespace content
