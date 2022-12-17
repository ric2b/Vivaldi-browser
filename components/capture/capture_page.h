// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CAPTURE_CAPTURE_PAGE_H_
#define COMPONENTS_CAPTURE_CAPTURE_PAGE_H_

#include <string>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vivaldi {

class CapturePage : private content::WebContentsObserver {
 public:
  struct CaptureParams {
    gfx::Rect rect;
    gfx::Size target_size;
    bool full_page = false;
  };

  using BitmapCallback = base::OnceCallback<void(SkBitmap bitmap)>;

  static void Capture(content::WebContents* contents,
                      const CaptureParams& params,
                      BitmapCallback callback);

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
  CapturePage(const CapturePage&) = delete;
  CapturePage& operator=(const CapturePage&) = delete;

  // Start the actual capture of the content.
  void CaptureImpl(content::WebContents* contents,
                   const CaptureParams& params,
                   BitmapCallback callback);

  void RespondAndDelete(SkBitmap bitmap = SkBitmap());
  void OnCaptureDone(SkBitmap bitmap);

  void OnCaptureTimeout();

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  void OnRequestThumbnailForFrameResponse(
      const gfx::Size& image_size,
      base::ReadOnlySharedMemoryRegion region);

  BitmapCallback capture_callback_;
  gfx::Size target_size_;

  base::WeakPtrFactory<CapturePage> weak_ptr_factory_{this};
};

}  // namespace vivaldi

#endif  // COMPONENTS_CAPTURE_CAPTURE_PAGE_H_
