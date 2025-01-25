// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTURE_THUMBNAIL_CAPTURE_CONTENTS_H_
#define COMPONENTS_CAPTURE_THUMBNAIL_CAPTURE_CONTENTS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class BrowserContext;
}  // namespace content

// Owns and controls a sandboxed WebContents instance hosting the rendering
// engine for an offscreen tab generating a thumbnail.  Since the offscreen tab
// does not interact with the user in any direct way, the WebContents is not
// attached to any Browser window/UI, and any input and focusing capabilities
// are blocked.
//
// This class operates exclusively on the UI thread and so is not thread-safe.
//
// The code is based on Chromium's OffscreenTab implementation.

namespace vivaldi {

class ThumbnailCaptureContents : protected content::WebContentsDelegate,
                                 protected content::WebContentsObserver {
 public:
  using CaptureCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedMemory> png_data)>;
  // Starts the navigation with the given size.
  static ThumbnailCaptureContents* Capture(content::BrowserContext* browser_context,
                      const GURL& start_url,
                      gfx::Size initial_size,
                      gfx::Size target_size,
                      CaptureCallback callback);


  content::WebContents* GetWebContents() {return offscreen_tab_web_contents_.get();}

  // "this" is no longer valid after the call
  void RespondAndDelete(SkBitmap bitmap = SkBitmap());

 private:
  ThumbnailCaptureContents();
  ~ThumbnailCaptureContents() override;
  ThumbnailCaptureContents(const ThumbnailCaptureContents&) = delete;
  ThumbnailCaptureContents& operator=(const ThumbnailCaptureContents&) = delete;

  void Start(content::BrowserContext* browser_context,
             const GURL& start_url,
             gfx::Size initial_size,
             gfx::Size target_size,
             CaptureCallback callback);

  // content::WebContentsDelegate overrides to provide the desired behaviors.
  void CloseContents(content::WebContents* source) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  bool ShouldFocusPageAfterCrash(content::WebContents * source) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::DragOperationsMask operations_allowed) override;
  void RequestMediaAccessPermission(
      content::WebContents* contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  // content::WebContentsObserver overrides
  void DidStartLoading() override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // Called after load timeout
  void OnPageLoadTimeout();

  // Do the capture itself, called initially from a timer to delay it for a
  // second.
  void TryCapture(bool last_try);

  void OnCopyImageReady(const SkBitmap& bitmap);

  void CaptureViaIpc();

  void OnIpcCaptureDone(SkBitmap bitmap);

  void OnCaptureTimeout();

  // The initial navigation URL, which may or may not match the current URL if
  // page-initiated navigations have occurred.
  GURL start_url_;

  // Size of the resulting bitmap.
  gfx::Size target_size_;

  // The WebContents containing the off-screen tab's page.
  std::unique_ptr<content::WebContents> offscreen_tab_web_contents_;

  base::TimeDelta next_capture_try_wait_;

  // Waiting for capture result.
  bool capture_started_ = false;

  CaptureCallback callback_;

  base::WeakPtrFactory<ThumbnailCaptureContents> weak_ptr_factory_{this};
};

}  // namespace vivaldi

#endif  // COMPONENTS_CAPTURE_THUMBNAIL_CAPTURE_CONTENTS_H_
