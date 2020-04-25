//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include <string>

#include "base/memory/shared_memory_handle.h"
#include "browser/thumbnails/capture_page.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
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
  bool CaptureAsync(content::WebContents *web_contents,
                    const gfx::Rect &capture_area,
                    base::OnceCallback<void(const SkBitmap &)> callback);
  void OnCaptureSuccess(const SkBitmap& bitmap);
  void OnCaptureFailure(FailureReason reason);
  void CopyFromBackingStoreComplete(const SkBitmap& bitmap);
  // ExtensionFunction:
  bool RunAsync() override;

 private:
  ImageFormat image_format_ = ImageFormat::IMAGE_FORMAT_PNG;
  int encode_quality_ = 90;
  bool show_file_in_path_ = false;
  bool encode_to_data_url_ = false;
  bool copy_to_clipboard_ = false;
  base::FilePath file_path_;
  bool save_to_disk_ = false;
  std::string save_folder_;
  std::string save_file_pattern_;
  GURL url_;
  std::string title_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureUIFunction);
};

class ThumbnailsCaptureTabFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureTab", THUMBNAILS_CAPTURETAB)

  ThumbnailsCaptureTabFunction();

 protected:
  ~ThumbnailsCaptureTabFunction() override;

  void OnThumbnailsCaptureCompleted(::vivaldi::CapturePage::Result captured);

  void ScaleAndConvertImage(::vivaldi::CapturePage::Result captured);

  void ScaleAndConvertImageDoneOnUIThread(const SkBitmap& bitmap,
                                          const std::string& image_data,
                                          bool success);

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  ImageFormat image_format_ = ImageFormat::IMAGE_FORMAT_PNG;
  int encode_quality_ = 90;
  bool capture_full_page_ = false;
  bool show_file_in_path_ = false;
  bool copy_to_clipboard_ = false;
  bool save_to_disk_ = false;
  base::FilePath file_path_;
  std::string save_folder_;
  gfx::Rect rect_;
  gfx::Size out_dimension_;
  std::string save_file_pattern_;
  GURL url_;
  std::string title_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureTabFunction);
};

class ThumbnailsCaptureUrlFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureUrl", THUMBNAILS_CAPTUREURL)
  ThumbnailsCaptureUrlFunction();

 private:
  ~ThumbnailsCaptureUrlFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnCaptured(::vivaldi::CapturePage::Result captured);

  void ScaleAndConvertImageOnWorkerThread(
      ::vivaldi::CapturePage::Result captured);

  void OnCapturedAndScaled(scoped_refptr<base::RefCountedMemory> thumbnail,
                           bool success);

  void SendResult(bool success);

  int64_t bookmark_id_ = 0;
  gfx::Size scaled_size_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureUrlFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
