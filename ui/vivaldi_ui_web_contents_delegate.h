// Copyright (c) 2017-2020 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_CONTENTS_H_
#define UI_VIVALDI_APP_WINDOW_CONTENTS_H_

#include "build/build_config.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/draggable_region.h"

class VivaldiBrowserWindow;
namespace content {
struct OpenURLParams;
}

namespace viz {
class SurfaceId;
}

// WebContents delegate for Vivaldi UI in the browser window.
class VivaldiUIWebContentsDelegate : public content::WebContentsDelegate,
                                     public content::WebContentsObserver {
 public:
  VivaldiUIWebContentsDelegate(VivaldiBrowserWindow* window);
  ~VivaldiUIWebContentsDelegate() override;
  VivaldiUIWebContentsDelegate(const VivaldiUIWebContentsDelegate&) = delete;
  VivaldiUIWebContentsDelegate& operator=(const VivaldiUIWebContentsDelegate&) =
      delete;

  void Initialize();

  // Overridden from WebContentsDelegate
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void ContentsMouseEvent(content::WebContents* source,
                          bool motion,
                          bool exited) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<content::ColorChooser> OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
      override;
#endif
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
  void ActivateContents(content::WebContents* contents) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  void CapturePaintPreviewOfSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      const base::UnguessableToken& guid,
      content::RenderFrameHost* render_frame_host) override;
#endif

 private:
  // content::WebContentsObserver
  void RenderFrameCreated(content::RenderFrameHost* render_view_host) override;
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* sender) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  void UpdateDraggableRegions(
      content::RenderFrameHost* sender,
      const std::vector<extensions::DraggableRegion>& regions);

  VivaldiBrowserWindow* window_;  // owner
};

#endif  // UI_VIVALDI_APP_WINDOW_CONTENTS_H_
