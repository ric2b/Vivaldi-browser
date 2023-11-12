//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include <memory>

#include "chrome/browser/profiles/profile_observer.h"
#include "components/capture/thumbnail_capture_contents.h"
#include "extensions/browser/extension_function.h"
#include "url/gurl.h"

#include "extensions/schema/thumbnails.h"

class SkBitmap;

namespace extensions {

struct CaptureData;

class ThumbnailsCaptureUIFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureUI", THUMBNAILS_CAPTUREUI)

  ThumbnailsCaptureUIFunction() = default;

 private:
  ~ThumbnailsCaptureUIFunction() override = default;
  void OnCaptureDone(std::unique_ptr<CaptureData> data,
                     bool success,
                     float device_scale_factor,
                     const SkBitmap& bitmap);
  void SendResult(std::unique_ptr<CaptureData> data);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ThumbnailsCaptureTabFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureTab", THUMBNAILS_CAPTURETAB)

  ThumbnailsCaptureTabFunction() = default;

 private:
  ~ThumbnailsCaptureTabFunction() override = default;
  void SendResult(std::unique_ptr<CaptureData> data);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class ThumbnailsCaptureBookmarkFunction : public ExtensionFunction, public ProfileObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("thumbnails.captureBookmark",
                             THUMBNAILS_CAPTUREBOOKMARK)
  ThumbnailsCaptureBookmarkFunction();

 private:
  ~ThumbnailsCaptureBookmarkFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void SendResult(std::string data_url);

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  GURL url_;
  ::vivaldi::ThumbnailCaptureContents* tcc_ = nullptr;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
