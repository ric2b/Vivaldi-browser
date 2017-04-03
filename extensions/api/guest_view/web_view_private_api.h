// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_
#define EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"

#include <string>
#include <vector>
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/web_view_private.h"

namespace extensions {
namespace vivaldi {

class WebViewPrivateSetVisibleFunction
    : public LegacyWebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.setVisible",
                             WEBVIEWINTERNAL_SETVISIBLE);

  WebViewPrivateSetVisibleFunction();

 protected:
  ~WebViewPrivateSetVisibleFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateSetVisibleFunction);
};

class WebViewInternalThumbnailFunction
    : public LegacyWebViewInternalExtensionFunction {
 public:
  WebViewInternalThumbnailFunction();

 protected:
  ~WebViewInternalThumbnailFunction() override;
  virtual void SendResultFromBitmap(const SkBitmap& screen_capture);
  bool InternalRunAsyncSafe(
      WebViewGuest* guest,
      std::unique_ptr<web_view_private::ThumbnailParams>& params);

  // Quality setting to use when encoding jpegs.  Set in RunImpl().
  int image_quality_;
  double scale_;
  int height_;
  int width_;
  // Are we running in incognito mode?
  bool is_incognito_;

  // The format (JPEG vs PNG) of the resulting image.  Set in RunImpl().
  api::extension_types::ImageFormat image_format_;

 private:
  // Callback for the RWH::CopyFromBackingStore call.
  void CopyFromBackingStoreComplete(const SkBitmap& bitmap,
                                    content::ReadbackResponse response);
};

class WebViewPrivateGetThumbnailFunction
    : public WebViewInternalThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getThumbnail",
                             WEBVIEWINTERNAL_GETTHUMBNAIL);

  WebViewPrivateGetThumbnailFunction();

 protected:
  ~WebViewPrivateGetThumbnailFunction() override;
  bool RunAsyncSafe(WebViewGuest* guest) override;

 private:
  void SendInternalError();

  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetThumbnailFunction);
};

class WebViewPrivateGetThumbnailFromServiceFunction
    : public WebViewInternalThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getThumbnailFromService",
                             WEBVIEWINTERNAL_GETTHUMBNAILFROMSERVICE);

  WebViewPrivateGetThumbnailFromServiceFunction();

 protected:
  ~WebViewPrivateGetThumbnailFromServiceFunction() override;
  bool RunAsyncSafe(WebViewGuest* guest) override;
  void SendResultFromBitmap(const SkBitmap& screen_capture) override;

  GURL url_;
  bool incognito = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetThumbnailFromServiceFunction);
};

class WebViewPrivateAddToThumbnailServiceFunction
    : public WebViewInternalThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.addToThumbnailService",
                             WEBVIEWINTERNAL_ADDTOTHUMBNAILSERVICE);

  WebViewPrivateAddToThumbnailServiceFunction();

 protected:
  ~WebViewPrivateAddToThumbnailServiceFunction() override;
  bool RunAsyncSafe(WebViewGuest* guest) override;
  void SendResultFromBitmap(const SkBitmap& screen_capture) override;
  void SetPageThumbnailOnUIThread(
      bool send_result,
      scoped_refptr<::thumbnails::ThumbnailService> thumbnail_service,
      scoped_refptr<::thumbnails::ThumbnailingContext> context,
      const gfx::Image& thumbnail);

  std::string key_;
  GURL url_;
  bool incognito = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateAddToThumbnailServiceFunction);
};

class WebViewPrivateShowPageInfoFunction
    : public LegacyWebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.showPageInfo",
                             WEBVIEWINTERNAL_SHOWPAGEINFO);

  WebViewPrivateShowPageInfoFunction();

 protected:
  ~WebViewPrivateShowPageInfoFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateShowPageInfoFunction);
};

class WebViewPrivateSetIsFullscreenFunction
    : public LegacyWebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.setIsFullscreen",
                             WEBVIEWINTERNAL_SETISFULLSCREEN);

  WebViewPrivateSetIsFullscreenFunction();

 protected:
  ~WebViewPrivateSetIsFullscreenFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateSetIsFullscreenFunction);
};

class WebViewPrivateGetPageHistoryFunction
    : public LegacyWebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.getPageHistory",
                             WEBVIEWINTERNAL_GETPAGEHISTORY);

  WebViewPrivateGetPageHistoryFunction();

 protected:
  ~WebViewPrivateGetPageHistoryFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetPageHistoryFunction);
};

class WebViewPrivateSetExtensionHostFunction
    : public LegacyWebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewPrivate.setExtensionHost",
                             WEBVIEWINTERNAL_SETEXTENSIONHOST);

  WebViewPrivateSetExtensionHostFunction();

 protected:
  ~WebViewPrivateSetExtensionHostFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewPrivateSetExtensionHostFunction);
};

class WebViewPrivateAllowBlockedInsecureContentFunction
    : public LegacyWebViewInternalExtensionFunction {
public:
    DECLARE_EXTENSION_FUNCTION("webViewPrivate.allowBlockedInsecureContent",
                               WEBVIEWINTERNAL_ALLOWBLOCKEDINSECURECONTENT)
    WebViewPrivateAllowBlockedInsecureContentFunction();

protected:
    ~WebViewPrivateAllowBlockedInsecureContentFunction() override;

private:
    bool RunAsyncSafe(WebViewGuest* guest) override;

    DISALLOW_COPY_AND_ASSIGN(WebViewPrivateAllowBlockedInsecureContentFunction);
};

class WebViewPrivateGetFocusedElementInfoFunction
    : public LegacyWebViewInternalExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("webViewPrivate.getFocusedElementInfo",
                              WEBVIEWINTERNAL_GETFOCUSEDELEMENTINFO)
   WebViewPrivateGetFocusedElementInfoFunction();

  protected:
   ~WebViewPrivateGetFocusedElementInfoFunction() override;

  private:
   bool RunAsyncSafe(WebViewGuest* guest) override;

   DISALLOW_COPY_AND_ASSIGN(WebViewPrivateGetFocusedElementInfoFunction);
};

}  // namespace vivaldi
}  // namespace extensions

#endif  // EXTENSIONS_API_GUEST_VIEW_WEB_VIEW_PRIVATE_API_H_
