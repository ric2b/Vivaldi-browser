// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_VIEW_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_VIEW_IMPL_H_

#include "ash/public/cpp/annotator/annotations_overlay_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

class Profile;

namespace views {
class WebView;
}  // namespace views

// The actual implementation of the view that will be used as the contents view
// of the annotations overlay widget. This view hosts a |views::WebView| which
// will show the contents of the annotator embedder URL.
class AnnotationsOverlayViewImpl : public ash::AnnotationsOverlayView {
  METADATA_HEADER(AnnotationsOverlayViewImpl, ash::AnnotationsOverlayView)

 public:
  explicit AnnotationsOverlayViewImpl(Profile* profile);
  AnnotationsOverlayViewImpl(const AnnotationsOverlayViewImpl&) = delete;
  AnnotationsOverlayViewImpl& operator=(const AnnotationsOverlayViewImpl&) =
      delete;
  ~AnnotationsOverlayViewImpl() override;

  views::WebView* GetWebViewForTest() { return web_view_; }

 private:
  // Initializes `web_view_` to load the annotator app.
  void InitializeAnnotator();

  raw_ptr<views::WebView> web_view_;

  base::WeakPtrFactory<AnnotationsOverlayViewImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_ANNOTATOR_ANNOTATIONS_OVERLAY_VIEW_IMPL_H_
