// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_
#define EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_

#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/browser/host_zoom_map_impl.h"

namespace vivaldi {

// NOTE(arnar@vivaldi.com): Helper that refreshes render_preferences if
// hostZoomMap has changed after a new render view is created
class VivaldiFrameObserver :
  public content::WebContentsUserData<VivaldiFrameObserver>,
  public content::WebContentsObserver {
 public:
  ~VivaldiFrameObserver() override;

 private:
  explicit VivaldiFrameObserver(content::WebContents *contents);
  friend class content::WebContentsUserData<VivaldiFrameObserver>;

  // Keep track of the HostZoomMap we're currently subscribed to.
  content::HostZoomMap* host_zoom_map_;

  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) override;

  DISALLOW_COPY_AND_ASSIGN(VivaldiFrameObserver);
};
}  // namespace vivaldi

#endif  //  EXTENSIONS_HELPER_VIVALDI_FRAME_OBSERVER_H_
