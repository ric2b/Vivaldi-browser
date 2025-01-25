// Copyright (c) 2018-2021 Vivaldi Technologies AS. All rights reserved.

#include "app/vivaldi_apptools.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_child_frame.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/content/vivaldi_tab_check.h"

namespace content {

void WebContentsImpl::SetVivExtData(const std::string& viv_ext_data) {
  viv_ext_data_ = viv_ext_data;
  observers_.NotifyObservers(&WebContentsObserver::VivExtDataSet, this);

  vivaldi::GetExtDataUpdatedCallbackList().Notify(this);
}

void WebContentsImpl::SetIgnoreLinkRouting(const bool ignore_link_routing) {
  ignore_link_routing_ = ignore_link_routing;
}

const std::string& WebContentsImpl::GetVivExtData() const {
  return viv_ext_data_;
}

bool WebContentsImpl::GetIgnoreLinkRouting() const {
  return ignore_link_routing_;
}

void WebContentsImpl::FrameTreeNodeDestroyed() {
  observers_.NotifyObservers(&WebContentsObserver::WebContentsDidDetach);
}

void WebContentsImpl::AttachedToOuter() {
  observers_.NotifyObservers(&WebContentsObserver::WebContentsDidAttach);
}

void WebContentsImpl::WebContentsTreeNode::VivaldiDestructor() {
  // NOTE(andre@vivaldi.com) : If we have a MimeHandlerViewGuest in an iframe
  // it would not get a OnFrameTreeNodeDestroyed call in time because it would
  // be destroyed when the embedder is destroyed.
  if (OuterContentsFrameTreeNode()) {
    OuterContentsFrameTreeNode()->RemoveObserver(this);
  }
  if (outer_web_contents_) {
    auto* outernode = OuterContentsFrameTreeNode();

    if (outernode) {
      outernode->RemoveObserver(this);

      // TODO(igor@vivaldi.com): This method is called after the code in
      // ~WebContentsImpl() run and, since the declaration of the frame_tree_
      // field comes after the node_ field in WebContentsImpl, after the
      // destructor for FrameTree. So
      // current_web_contents_->GetPrimaryFrameTree() returns a pointer to
      // FrameTree after its destructor is run. Figure out if this is safe to
      // call here.
      current_web_contents_->GetPrimaryFrameTree().RemoveFrame(outernode);
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
        outer_web_contents_->node_.DetachInnerWebContents(current_web_contents_)
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
  WebContentsImpl* outermost = current_web_contents_->GetOutermostWebContents();
  if (current_web_contents_->ContainsOrIsFocusedWebContents()) {
    // If the current WebContents is in focus, unset it.
    outermost->SetAsFocusedWebContentsIfNecessary();
  }

  // Make sure we can reattach with a new ProxyHost.
  current_web_contents_->GetRenderManager()
      ->current_frame_host()
      ->browsing_context_state()
      ->DeleteRenderFrameProxyHost(
          OuterContentsFrameTreeNode()
              ->current_frame_host()
              ->GetSiteInstance()
              ->group(),
          BrowsingContextState::ProxyAccessMode::kAllowOuterDelegate);

  // Make sure this isn't freed here. It is owned by the TabStrip or DevTools.
  std::unique_ptr<WebContents> inner_contents =
      outer_web_contents_->node_.DetachInnerWebContents(current_web_contents_);
  OuterContentsFrameTreeNode()->RemoveObserver(this);
  outer_contents_frame_tree_node_id_ = FrameTreeNodeId();
  outer_web_contents_ = nullptr;

  inner_contents.release();

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

  node->RemoveObserver(this);

  // This is Vivaldi specific to be able to sync the mounting in the client
  // after it has been detached.
  current_web_contents_->FrameTreeNodeDestroyed();
}

void WebContentsImpl::SetResumePending(bool resume) {
  is_resume_pending_ = resume;
}

void WebContentsImpl::SetJavaScriptDialogManager(
    JavaScriptDialogManager* dialog_manager) {
  dialog_manager_ = dialog_manager;
}

// Loop through all web contents and check if it cointains the point.
// Returns true if the point is only contained by the UI content.
bool WebContentsImpl::IsVivaldiUI(const gfx::Point& point) {
  bool uiContainsPoint = false;

  if (this->GetVisibility() == Visibility::VISIBLE &&
      this->GetViewBounds().Contains(point)) {
    if (vivaldi::IsVivaldiUrl(this->GetVisibleURL().spec())) {
      uiContainsPoint = true;
    } else {
      return false;
    }
  }

  std::vector<WebContentsImpl*> relevant_contents(1, this);
  for (size_t i = 0; i != relevant_contents.size(); ++i) {
    for (auto* inner : relevant_contents[i]->GetInnerWebContents()) {

      if (inner->GetVisibility() == Visibility::VISIBLE &&
          inner->GetViewBounds().Contains(point)) {
        if (vivaldi::IsVivaldiUrl(inner->GetVisibleURL().spec())) {
          uiContainsPoint = true;
        }
        if (!vivaldi::IsVivaldiUrl(inner->GetVisibleURL().spec())) {
          return false;
        }
        relevant_contents.push_back(static_cast<WebContentsImpl*>(inner));
      }
    }
  }

  return uiContainsPoint;
}

}  // namespace content
