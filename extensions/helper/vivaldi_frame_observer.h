// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_
#define EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_

#include <string>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class HostZoomMap;
}

namespace vivaldi {

// NOTE(arnar@vivaldi.com): Helper that refreshes render_preferences if
// hostZoomMap has changed after a new render view is created
class VivaldiFrameObserver
    : public content::WebContentsUserData<VivaldiFrameObserver>,
      public content::WebContentsObserver {
 public:
  ~VivaldiFrameObserver() override;
  VivaldiFrameObserver(const VivaldiFrameObserver&) = delete;
  VivaldiFrameObserver& operator=(const VivaldiFrameObserver&) = delete;

 private:
  explicit VivaldiFrameObserver(content::WebContents* contents);
  friend class content::WebContentsUserData<VivaldiFrameObserver>;

  // Keep track of the HostZoomMap we're currently subscribed to.
  raw_ptr<content::HostZoomMap> host_zoom_map_;

  void RenderFrameCreated(content::RenderFrameHost* render_view_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace vivaldi

#endif  //  EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_
