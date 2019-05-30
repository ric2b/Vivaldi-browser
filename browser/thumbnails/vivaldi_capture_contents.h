// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BROWSER_THUMBNAIL_VIVALDI_CAPTURE_CONTENTS_H_
#define BROWSER_THUMBNAIL_VIVALDI_CAPTURE_CONTENTS_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "browser/thumbnails/vivaldi_thumbnail_utils.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class BrowserContext;
}  // namespace content

class SkBitmap;
class GURL;

namespace vivaldi {
class ThumbnailCaptureContents;
}

typedef base::Callback<void(const SkBitmap& bitmap, bool success)>
    CapturePageAndScaleDoneCallback;

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

class ThumbnailCaptureContents
    : public base::RefCountedThreadSafe<ThumbnailCaptureContents>,
      protected content::WebContentsDelegate,
      protected content::WebContentsObserver {
 public:
  explicit ThumbnailCaptureContents(content::BrowserContext* context);

  // Starts the navigation with the given size.
  void Start(const GURL& start_url,
             const gfx::Size& initial_size,
             const gfx::Size& scaled_size,
             CapturePageAndScaleDoneCallback callback);

  // Closes the underlying WebContents.
  void Close();

  void set_browser_context(content::BrowserContext* context) {
    browser_context_ = context;
  }

 private:
  friend class base::RefCountedThreadSafe<ThumbnailCaptureContents>;

  ~ThumbnailCaptureContents() override;

  // content::WebContentsDelegate overrides to provide the desired behaviors.
  void CloseContents(content::WebContents* source) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool ShouldFocusLocationBarByDefault(content::WebContents* source) override;
  bool ShouldFocusPageAfterCrash() override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::RepeatingCallback<void(bool)>& callback) override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  bool CanDragEnter(content::WebContents* source,
                    const content::DropData& data,
                    blink::WebDragOperationsMask operations_allowed) override;
  bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      content::RenderFrameHost* opener,
      content::SiteInstance* source_site_instance,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      content::mojom::WindowContainerType window_container_type,
      const GURL& opener_url,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;
  bool EmbedsFullscreenWidget() const override;
  void RequestMediaAccessPermission(
      content::WebContents* contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::MediaStreamType type) override;

  // content::WebContentsObserver overrides
  void DidStartLoading() override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void RenderProcessGone(base::TerminationStatus status) override;

  // callbacks
  void CapturedAndScaledCallback(const SkBitmap& bitmap,
                                 int callback_id,
                                 bool success);

  // Called after load timeout
  void OnCaptureTimeout();

  // Do the capture itself, called from a timer to delay it for a second.
  void OnCapture();

  void ResetWebContents();

  // The initial navigation URL, which may or may not match the current URL if
  // page-initiated navigations have occurred.
  GURL start_url_;

  // Size requested after scaling.  It will be resizes so it keeps the
  // propotions, even if it means the scaled size is not followed exactly.
  gfx::Size scaled_size_;

  // Size of the offscreen web contents.
  gfx::Size capture_size_;

  // The WebContents containing the off-screen tab's page.
  std::unique_ptr<content::WebContents> offscreen_tab_web_contents_;

  // Timer to cancel a request if it fires before the page has loaded.
  base::OneShotTimer capture_timeout_timer_;

  content::BrowserContext* browser_context_ = nullptr;

  // Class responsible for doing the capture itself.
  scoped_refptr<vivaldi::CaptureLoadedPage> capture_page_;

  // The ID associated with the the capture request
  int current_request_id_ = 0;

  // Incrementing capture id counter
  static int s_capture_request_id;

  CapturePageAndScaleDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailCaptureContents);
};

}  // namespace vivaldi

#endif  // BROWSER_THUMBNAIL_VIVALDI_CAPTURE_CONTENTS_H_
