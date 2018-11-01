// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/vivaldi_init_helpers.h"

#include "extensions/helper/vivaldi_frame_observer.h"

namespace vivaldi {

void InitHelpers(content::WebContents* web_contents) {
  vivaldi::VivaldiFrameObserver::CreateForWebContents(web_contents);
}

}  // namespace vivaldi
