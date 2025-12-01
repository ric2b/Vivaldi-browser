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


void WebContentsImpl::SetResumePending(bool resume) {
  is_resume_pending_ = resume;
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
