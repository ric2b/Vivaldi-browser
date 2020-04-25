// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_THUMBNAILS_CAPTURE_PAGE_H_
#define BROWSER_THUMBNAILS_CAPTURE_PAGE_H_

#include <string>

#include "base/memory/shared_memory_handle.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;

namespace vivaldi {

class CapturePage : private content::WebContentsObserver {
 public:

  struct CaptureParams {
    gfx::Rect rect;
    gfx::Size target_size;

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

   private:
    friend class CapturePage;

    SkImageInfo image_info_;
    base::ReadOnlySharedMemoryRegion region_;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  using DoneCallback = base::OnceCallback<void(Result capture_result)>;

  static void Capture(content::WebContents* contents,
                      const CaptureParams& params,
                      DoneCallback callback);

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

  bool OnMessageReceived(const IPC::Message& message) override;

  void OnRequestThumbnailForFrameResponse(
      int callback_id,
      gfx::Size image_size,
      base::ReadOnlySharedMemoryRegion region);

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

#endif  // BROWSER_THUMBNAILS_CAPTURE_PAGE_H_
