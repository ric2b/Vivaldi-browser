// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_
#define EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_

#include <string>

#include "content/browser/host_zoom_map_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace vivaldi {

// NOTE(arnar@vivaldi.com): Helper that refreshes render_preferences if
// hostZoomMap has changed after a new render view is created
class VivaldiFrameObserver
    : public content::WebContentsUserData<VivaldiFrameObserver>,
      public content::WebContentsObserver {
 public:
  ~VivaldiFrameObserver() override;

  void GetFocusedElementInfo(std::string* tagname,
                             std::string* type,
                             bool* editable,
                             std::string* role);

 private:
  explicit VivaldiFrameObserver(content::WebContents* contents);
  friend class content::WebContentsUserData<VivaldiFrameObserver>;

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void OnDidUpdateFocusedElementInfo(std::string tagname,
                                     std::string type,
                                     bool editable,
                                     std::string role);
  void OnFocusedNodeChanged(bool editable, gfx::Rect node_bounds);

  // This gets returned by GetFocusedElementInfo()
  std::string focused_element_tagname_;
  std::string focused_element_type_;
  bool focused_element_editable_ = false;
  std::string focused_element_role_;

  // Keep track of the HostZoomMap we're currently subscribed to.
  content::HostZoomMap* host_zoom_map_;

  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiFrameObserver);
};
}  // namespace vivaldi

#endif  //  EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_
