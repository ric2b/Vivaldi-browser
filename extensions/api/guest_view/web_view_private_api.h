// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_
#define EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"

#include <memory>
#include <string>
#include <vector>

#include "extensions/browser/extension_function.h"
#include "extensions/schema/web_view_private.h"

namespace extensions {
namespace vivaldi {

class VivaldiWebViewWithGuestFunction
    : public UIThreadExtensionFunction {
 public:
  VivaldiWebViewWithGuestFunction() = default;

 protected:
  ~VivaldiWebViewWithGuestFunction() override = default;

  WebViewGuest* guest_ = nullptr;

private:
  bool PreRunValidation(std::string* error) override;
  DISALLOW_COPY_AND_ASSIGN(VivaldiWebViewWithGuestFunction);
};

class WebViewPrivateSetVisibleFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.setVisible",
                             WEBVIEWINTERNAL_SETVISIBLE)
  WebViewPrivateSetVisibleFunction() = default;

 private:
  ~WebViewPrivateSetVisibleFunction() override = default;
  ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateSetVisibleFunction);
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
  void SendResult(bool success, const std::string& base64_result);

  // Quality setting to use when encoding jpegs.  Set in RunImpl().
  int image_quality_;
  double scale_;
  int height_;
  int width_;

  // The format (JPEG vs PNG) of the resulting image.  Set in RunImpl().
  api::extension_types::ImageFormat image_format_;

  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetThumbnailFunction);
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
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateShowPageInfoFunction);
};

class WebViewPrivateSetIsFullscreenFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.setIsFullscreen",
                             WEBVIEWINTERNAL_SETISFULLSCREEN)
  WebViewPrivateSetIsFullscreenFunction()= default;

 private:
  ~WebViewPrivateSetIsFullscreenFunction() override = default;
  ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateSetIsFullscreenFunction);
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
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetPageHistoryFunction);
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
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateAllowBlockedInsecureContentFunction);
};

class WebViewPrivateGetFocusedElementInfoFunction
    : public VivaldiWebViewWithGuestFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getFocusedElementInfo",
                             WEBVIEWINTERNAL_GETFOCUSEDELEMENTINFO)
  WebViewPrivateGetFocusedElementInfoFunction() = default;

 private:
  ~WebViewPrivateGetFocusedElementInfoFunction() override = default;
  ResponseAction Run() override;
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetFocusedElementInfoFunction);
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
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateSendRequestFunction);
};

}  // namespace vivaldi
}  // namespace extensions

#endif  // EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_
