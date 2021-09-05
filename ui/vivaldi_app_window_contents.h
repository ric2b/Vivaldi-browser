// Copyright (c) 2017-2020 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIVALDI_APP_WINDOW_CONTENTS_H_
#define UI_VIVALDI_APP_WINDOW_CONTENTS_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/draggable_region.h"

class VivaldiBrowserWindow;
namespace extensions {
  class AppDelegate;
}

// AppWindowContents class specific to vivaldi app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class VivaldiAppWindowContentsImpl : public content::WebContentsDelegate,
  public content::WebContentsObserver {
 public:
  VivaldiAppWindowContentsImpl(VivaldiBrowserWindow* host);
  ~VivaldiAppWindowContentsImpl() override;

  content::WebContents* web_contents() const { return web_contents_.get(); }

  // AppWindowContents
  void Initialize(std::unique_ptr<content::WebContents> web_contents);
  void NativeWindowClosed();

  // Overridden from WebContentsDelegate
  bool HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) override;
  void ContentsMouseEvent(content::WebContents* source,
    bool motion,
    bool exited) override;
  bool PreHandleGestureEvent(content::WebContents* source,
    const blink::WebGestureEvent& event) override;
  content::ColorChooser* OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
    std::unique_ptr<content::FileSelectListener> listener,
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
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) override;
  void ExitPictureInPicture() override;
  void PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const override;
  void ActivateContents(content::WebContents* contents) override;

 private:
  // content::WebContentsObserver
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
    content::RenderFrameHost* sender) override;
  void DidFinishNavigation(
    content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) override;

  void UpdateDraggableRegions(
    content::RenderFrameHost* sender,
    const std::vector<extensions::DraggableRegion>& regions);

  extensions::AppDelegate* GetAppDelegate();

  VivaldiBrowserWindow* host_;  // This class is owned by |host_|
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowContentsImpl);
};

#endif  // UI_VIVALDI_APP_WINDOW_CONTENTS_H_
