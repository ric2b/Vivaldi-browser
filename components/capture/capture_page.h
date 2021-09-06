// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CAPTURE_CAPTURE_PAGE_H_
#define COMPONENTS_CAPTURE_CAPTURE_PAGE_H_

#include <string>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect_f.h"

class SkBitmap;

namespace vivaldi {

class CapturePage : private content::WebContentsObserver {
 public:
  struct CaptureParams {
    gfx::Rect rect;
    gfx::Size target_size;
    int client_id = 0;
    bool full_page = false;

    // Only single capture request will be made per given WebContents.
    bool once_per_contents = false;
  };

  // Move only wrapper around results.
  class Result {
   public:
    Result();
    Result(Result&& other);
    ~Result();
    Result& operator=(Result&& other);

    // This is a heavy operation and should be called from a worker thread.
    // Return false if unsuccessful. This should only be called once per Result
    // instance.
    bool MovePixelsToBitmap(SkBitmap* bitmap);
    int client_id();

   private:
    friend class CapturePage;

    SkImageInfo image_info_;
    base::ReadOnlySharedMemoryRegion region_;
    int client_id_;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  using DoneCallback = base::OnceCallback<void(Result capture_result)>;

  static void Capture(content::WebContents* contents,
                      const CaptureParams& params,
                      DoneCallback callback);

  // Start capturing the given area of the window corresponding to the given
  // WebContents and send the result to the callback. The rect should be in
  // device-independent pixels. The callback can be called either synchronously
  // or asynchronously on the original thread. The size of the captured bitmap
  // matches the number of physical pixels that covers the area.
  // device_scale_factor gives the scaling from device-independent pixels to
  // physical ones.
  using CaptureVisibleCallback = base::OnceCallback<
      void(bool success, float device_scale_factor, const SkBitmap&)>;
  static void CaptureVisible(content::WebContents* contents,
                             const gfx::RectF& rect,
                             CaptureVisibleCallback callback);

 private:
  explicit CapturePage();
  ~CapturePage() override;

  // Start the actual capture of the content.
  void CaptureImpl(content::WebContents* contents,
                   const CaptureParams& params,
                   DoneCallback callback);

  void RespondAndDelete(Result captured = Result());

  void OnCaptureTimeout();

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  void OnRequestThumbnailForFrameResponse(
      int callback_id,
      gfx::Size image_size,
      base::ReadOnlySharedMemoryRegion region,
      int client_id);

  DoneCallback capture_callback_;
  int callback_id_ = 0;
  bool once_per_contents_ = false;
  gfx::Size target_size_;

  // Incrementing callback id counter
  static int s_callback_id;

  base::WeakPtrFactory<CapturePage> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CapturePage);
};

}  // namespace vivaldi

#endif  // COMPONENTS_CAPTURE_CAPTURE_PAGE_H_
