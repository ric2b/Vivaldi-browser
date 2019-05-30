// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef UI_CONTENT_VIVALDI_TAB_CHECK_H_
#define UI_CONTENT_VIVALDI_TAB_CHECK_H_

#include "base/supports_user_data.h"
#include "chromium/content/common/content_export.h"

namespace content {
class RenderWidgetHostViewChildFrame;
class WebContents;
}  // namespace content

// Collection of static methods to check for a Vivaldi tab
class CONTENT_EXPORT VivaldiTabCheck {
 public:
  // The address of this is used as a WebContents user data key for
  // VivaldiPrivateTabObserver. It is defined here so we can use it from
  // Chromium content code.
  static const int kVivaldiTabObserverContextKey;

  static bool IsVivaldiTab(content::WebContents* web_contents);

  static bool IsVivaldiTabFrame(
      content::RenderWidgetHostViewChildFrame* child_frame);
};

#endif  // UI_CONTENT_VIVALDI_TAB_CHECK_H_
