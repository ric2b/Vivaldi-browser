//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include <string>

#include "base/memory/shared_memory_handle.h"
#include "browser/thumbnails/capture_page.h"
#include "components/datasource/vivaldi_data_source_api.h"
#include "extensions/browser/extension_function.h"
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

class ThumbnailsCaptureUIFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureUI", THUMBNAILS_CAPTUREUI)

  ThumbnailsCaptureUIFunction();

 private:
  ~ThumbnailsCaptureUIFunction() override;
  void CaptureAsync(content::RenderWidgetHostView* view,
                    const gfx::Rect& capture_area);
  void OnCaptureSuccess(const SkBitmap& bitmap);
  void CopyFromBackingStoreComplete(const SkBitmap& bitmap);
  std::string CaptureError(base::StringPiece details = base::StringPiece());

  // ExtensionFunction:
  ResponseAction Run() override;

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

class ThumbnailsCaptureTabFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureTab", THUMBNAILS_CAPTURETAB)

  ThumbnailsCaptureTabFunction();

 private:
  ~ThumbnailsCaptureTabFunction() override;

  void OnThumbnailsCaptureCompleted(::vivaldi::CapturePage::Result captured);

  void ConvertImageOnWorkerThread(::vivaldi::CapturePage::Result captured);

  void OnImageConverted(const SkBitmap& bitmap,
                        const std::string& image_data,
                        bool success);

  // ExtensionFunction:
  ResponseAction Run() override;

  ImageFormat image_format_ = ImageFormat::IMAGE_FORMAT_PNG;
  int encode_quality_ = 90;
  bool show_file_in_path_ = false;
  bool copy_to_clipboard_ = false;
  bool save_to_disk_ = false;
  base::FilePath file_path_;
  std::string save_folder_;
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

  void ConvertImageOnWorkerThread(scoped_refptr<VivaldiDataSourcesAPI> api,
                                  ::vivaldi::CapturePage::Result captured);

  void SendResult(bool success);

  int64_t bookmark_id_ = 0;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureUrlFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
