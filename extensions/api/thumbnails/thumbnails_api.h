//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
#define EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "content/public/browser/readback_types.h"
#include "extensions/schema/thumbnails.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

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
  bool EncodeBitmap(const SkBitmap& bitmap, std::string* base64_result);

  // ExtensionFunction:
  bool RunAsync() override;

private:
};

}  // namespace extensions

#endif  // EXTENSIONS_API_THUMBNAILS_THUMBNAILS_API_H_
