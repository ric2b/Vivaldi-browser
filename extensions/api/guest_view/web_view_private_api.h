// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_
#define EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"

#include <memory>
#include <string>
#include <vector>

#include "extensions/browser/extension_function.h"
#include "extensions/schema/web_view_private.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/vivaldi_skia_utils.h"

namespace extensions {
namespace vivaldi {

class VivaldiWebViewWithGuestFunction : public ExtensionFunction {
 public:
  VivaldiWebViewWithGuestFunction() = default;

 protected:
  ~VivaldiWebViewWithGuestFunction() override = default;

  raw_ptr<WebViewGuest> guest_ = nullptr;

 private:
  bool PreRunValidation(std::string* error) override;
};

class WebViewPrivateGetThumbnailFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getThumbnail",
                             WEBVIEWINTERNAL_GETTHUMBNAIL)
  WebViewPrivateGetThumbnailFunction();

 private:
  ~WebViewPrivateGetThumbnailFunction() override;
  ResponseAction Run() override;
  ResponseAction RunImpl(const web_view_private::ThumbnailParams& params);
  void CopyFromBackingStoreComplete(const SkBitmap& bitmap);
  void ScaleAndEncodeOnWorkerThread(SkBitmap bitmap);
  void SendResult();

  // Quality setting to use when encoding jpegs.  Set in RunImpl().
  int image_quality_;
  double scale_;
  int height_ = 0;
  int width_ = 0;
  std::string base64_result_;

  // The format (JPEG vs PNG) of the resulting image.  Set in RunImpl().
  ::vivaldi::skia_utils::ImageFormat image_format_ =
      ::vivaldi::skia_utils::ImageFormat::kJPEG;
};

class WebViewPrivateShowPageInfoFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.showPageInfo",
                             WEBVIEWINTERNAL_SHOWPAGEINFO)
  WebViewPrivateShowPageInfoFunction() = default;

 private:
  ~WebViewPrivateShowPageInfoFunction() override = default;
  ResponseAction Run() override;
};

class WebViewPrivateSetIsFullscreenFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.setIsFullscreen",
                             WEBVIEWINTERNAL_SETISFULLSCREEN)
  WebViewPrivateSetIsFullscreenFunction() = default;

 private:
  ~WebViewPrivateSetIsFullscreenFunction() override = default;
  ResponseAction Run() override;
};

class WebViewPrivateGetPageHistoryFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getPageHistory",
                             WEBVIEWINTERNAL_GETPAGEHISTORY)
  WebViewPrivateGetPageHistoryFunction() = default;

 private:
  ~WebViewPrivateGetPageHistoryFunction() override = default;
  ResponseAction Run() override;
};

class WebViewPrivateAllowBlockedInsecureContentFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.allowBlockedInsecureContent",
                             WEBVIEWINTERNAL_ALLOWBLOCKEDINSECURECONTENT)
  WebViewPrivateAllowBlockedInsecureContentFunction() = default;

 private:
  ~WebViewPrivateAllowBlockedInsecureContentFunction() override = default;
  ResponseAction Run() override;
};

class WebViewPrivateSendRequestFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.sendRequest",
                             WEBVIEWINTERNAL_SENDREQUEST)
  WebViewPrivateSendRequestFunction() = default;

 private:
  ~WebViewPrivateSendRequestFunction() override = default;
  ResponseAction Run() override;
};

class WebViewPrivateGetPageSelectionFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getPageSelection",
                             WEBVIEWINTERNAL_GETPAGESELECTION)
  WebViewPrivateGetPageSelectionFunction() = default;

 private:
  ~WebViewPrivateGetPageSelectionFunction() override = default;
  ResponseAction Run() override;
};

}  // namespace vivaldi
}  // namespace extensions

#endif  // EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_
