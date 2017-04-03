//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include "base/memory/shared_memory_handle.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "content/public/browser/readback_types.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/schema/thumbnails.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
class Size;
}

using extensions::api::extension_types::ImageFormat;

namespace extensions {

class ThumbnailsIsThumbnailAvailableFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.isThumbnailAvailable",
                             THUMBNAILS_ISTHUMBNAILAVAILABLE)
  ThumbnailsIsThumbnailAvailableFunction();

 protected:
  ~ThumbnailsIsThumbnailAvailableFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
   DISALLOW_COPY_AND_ASSIGN(ThumbnailsIsThumbnailAvailableFunction);
};

class ThumbnailsCaptureUIFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureUI", THUMBNAILS_CAPTUREUI)

  ThumbnailsCaptureUIFunction();

  enum FailureReason {
    FAILURE_REASON_UNKNOWN,
    FAILURE_REASON_ENCODING_FAILED,
    FAILURE_REASON_VIEW_INVISIBLE
  };

 protected:
  ~ThumbnailsCaptureUIFunction() override;
  bool CaptureAsync(content::WebContents* web_contents,
                    gfx::Rect& capture_area,
                    const content::ReadbackRequestCallback callback);
  void OnCaptureSuccess(const SkBitmap& bitmap);
  void OnCaptureFailure(FailureReason reason);
  void CopyFromBackingStoreComplete(const SkBitmap& bitmap,
                                    content::ReadbackResponse response);
  // ExtensionFunction:
  bool RunAsync() override;

 private:
  ImageFormat image_format_ = ImageFormat::IMAGE_FORMAT_PNG;
  int encode_quality_ = 90;
  bool show_file_in_path_ = false;
  bool encode_to_data_url_ = false;
  bool copy_to_clipboard_ = false;
  base::FilePath file_path_;
  std::string base_path_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureUIFunction);
};

class ThumbnailsCaptureTabFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureTab", THUMBNAILS_CAPTURETAB)

  ThumbnailsCaptureTabFunction();

 protected:
  ~ThumbnailsCaptureTabFunction() override;

  void OnThumbnailsCaptureCompleted(base::SharedMemoryHandle handle,
                                    const gfx::Size image_size,
                                    int callback_id,
                                    bool success);

  void ScaleAndConvertImage(base::SharedMemoryHandle handle,
                            const gfx::Size image_size,
                            int callback_id);

  void ScaleAndConvertImageDoneOnUIThread(const SkBitmap bitmap,
                                          const std::string image_data,
                                          int callback_id);

  void DispatchErrorOnUIThread(const std::string& error_msg);
  void DispatchError(const std::string& error_msg);

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  ImageFormat image_format_ = ImageFormat::IMAGE_FORMAT_PNG;
  int encode_quality_ = 90;
  bool capture_full_page_ = false;
  bool show_file_in_path_ = false;
  bool copy_to_clipboard_ = false;
  base::FilePath file_path_;
  int width_ = 0;
  int height_ = 0;
  std::string base_path_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureTabFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
