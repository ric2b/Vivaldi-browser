//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include <string>

#include "content/public/common/drop_data.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/extension_types.h"
#include "third_party/skia/include/core/SkBitmap.h"

#include "components/capture/capture_page.h"
#include "extensions/schema/thumbnails.h"
#include "ui/vivaldi_skia_utils.h"

namespace extensions {

struct CaptureFormat {
  CaptureFormat();
  ~CaptureFormat();
  ::vivaldi::skia_utils::ImageFormat image_format =
      ::vivaldi::skia_utils::ImageFormat::kPNG;
  int encode_quality = 90;
  bool show_file_in_path = false;
  bool copy_to_clipboard = false;
  bool save_to_disk = false;
  std::string save_folder;
  std::string save_file_pattern;
  GURL url;
  std::string title;
};

class ThumbnailsCaptureUIFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureUI", THUMBNAILS_CAPTUREUI)

  ThumbnailsCaptureUIFunction();

 private:
  ~ThumbnailsCaptureUIFunction() override;
  void OnCaptureDone(bool success,
                     float device_scale_factor,
                     const SkBitmap& bitmap);
  bool ProcessImageOnWorkerThread(SkBitmap bitmap);
  void SendResult(bool success);

  // ExtensionFunction:
  ResponseAction Run() override;

  CaptureFormat format_;
  std::string image_data_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureUIFunction);
};

class ThumbnailsCaptureTabFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureTab", THUMBNAILS_CAPTURETAB)

  ThumbnailsCaptureTabFunction();

 private:
  ~ThumbnailsCaptureTabFunction() override;

  void OnTabCaptureCompleted(::vivaldi::CapturePage::Result captured);

  bool ConvertImageOnWorkerThread(::vivaldi::CapturePage::Result captured);

  void SendResult(bool success);

  // ExtensionFunction:
  ResponseAction Run() override;

  CaptureFormat format_;
  SkBitmap bitmap_;
  std::string image_data_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureTabFunction);
};

class ThumbnailsCaptureBookmarkFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureBookmark",
                             THUMBNAILS_CAPTUREBOOKMARK)
  ThumbnailsCaptureBookmarkFunction();

 private:
  ~ThumbnailsCaptureBookmarkFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void SendResult(bool success);

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailsCaptureBookmarkFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
