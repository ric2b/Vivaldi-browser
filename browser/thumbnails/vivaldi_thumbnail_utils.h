// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_THUMBNAILS_VIVALDI_THUMBNAIL_UTILS_H_
#define BROWSER_THUMBNAILS_VIVALDI_THUMBNAIL_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "extensions/common/api/extension_types.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

using extensions::api::extension_types::ImageFormat;

typedef base::Callback<void(base::SharedMemoryHandle handle,
                            const gfx::Size image_size,
                            int callback_id,
                            bool success)>
    CaptureDoneCallback;

typedef base::Callback<
    void(const SkBitmap& bitmap, int callback_id, bool success)>
    CaptureAndScaleDoneCallback;

namespace vivaldi {

class CaptureLoadedPage;

class CaptureLoadedPage : public base::RefCountedThreadSafe<CaptureLoadedPage>,
                          public content::WebContentsObserver {
 public:
  explicit CaptureLoadedPage(content::BrowserContext* context);

  struct CaptureParams {
    // Size of the capture.
    gfx::Size capture_size_;
    // Scaled size of the capture.
    gfx::Size scaled_size_;

    bool full_page_ = false;
  };

  // Wrapper method to handle capture and scaling
  void CaptureAndScale(content::WebContents* contents,
                       CaptureParams& params,
                       int request_id,
                       const CaptureAndScaleDoneCallback& callback);

  // Start the actual capture of the content.
  void Capture(content::WebContents* contents,
               CaptureParams& params,
               int request_id,
               const CaptureDoneCallback& callback);

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~CaptureLoadedPage() override;

 private:
   friend class base::RefCountedThreadSafe<CaptureLoadedPage>;

   void OnRequestThumbnailForFrameResponse(base::SharedMemoryHandle handle,
                                          const gfx::Size image_size,
                                          int callback_id,
                                          bool success);

  void OnCaptureCompleted(base::SharedMemoryHandle handle,
                          const gfx::Size image_size,
                          int callback_id,
                          bool success);

  void ScaleAndConvertImage(base::SharedMemoryHandle handle,
                            const gfx::Size image_size,
                            int callback_id);

  void ScaleAndConvertImageDoneOnUIThread(const SkBitmap& bitmap,
                                          int callback_id);

   CaptureDoneCallback capture_callback_;
  CaptureAndScaleDoneCallback capture_scale_callback_;

  CaptureParams input_params_;

  std::string error_;
};

}  // namespace vivaldi

#endif  // BROWSER_THUMBNAILS_VIVALDI_THUMBNAIL_UTILS_H_
