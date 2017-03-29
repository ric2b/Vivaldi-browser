// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_

#include "extensions/browser/api/capture_web_contents_function.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/web_view/web_ui/web_ui_url_fetcher.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"

// WARNING: WebViewInternal could be loaded in an unblessed context, thus any
// new APIs must extend WebViewInternalExtensionFunction or
// WebViewInternalExecuteCodeFunction which do a process ID check to prevent
// abuse by normal renderer processes.
namespace extensions {

// An abstract base class for async webview APIs. It does a process ID check
// in RunAsync, and then calls RunAsyncSafe which must be overriden by all
// subclasses.
class WebViewInternalExtensionFunction : public AsyncExtensionFunction {
 public:
  WebViewInternalExtensionFunction() {}

 protected:
  ~WebViewInternalExtensionFunction() override {}

  // ExtensionFunction implementation.
  bool RunAsync() final;

 private:
  virtual bool RunAsyncSafe(WebViewGuest* guest) = 0;
};

class WebViewInternalNavigateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.navigate",
                             WEBVIEWINTERNAL_NAVIGATE);
  WebViewInternalNavigateFunction() {}

 protected:
  ~WebViewInternalNavigateFunction() override {}

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalNavigateFunction);
};

class WebViewInternalExecuteCodeFunction
    : public extensions::ExecuteCodeFunction {
 public:
  WebViewInternalExecuteCodeFunction();

 protected:
  ~WebViewInternalExecuteCodeFunction() override;

  // Initialize |details_| if it hasn't already been.
  bool Init() override;
  bool ShouldInsertCSS() const override;
  bool CanExecuteScriptOnPage() override;
  // Guarded by a process ID check.
  extensions::ScriptExecutor* GetScriptExecutor() final;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;
  bool LoadFile(const std::string& file) override;

 private:
  // Loads a file url on WebUI.
  bool LoadFileForWebUI(const std::string& file_src,
                        const WebUIURLFetcher::WebUILoadFileCallback& callback);

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  extensions::ExtensionResource resource_;

  int guest_instance_id_;

  GURL guest_src_;

  scoped_ptr<WebUIURLFetcher> url_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalExecuteCodeFunction);
};

class WebViewInternalExecuteScriptFunction
    : public WebViewInternalExecuteCodeFunction {
 public:
  WebViewInternalExecuteScriptFunction();

 protected:
  ~WebViewInternalExecuteScriptFunction() override {}

  void OnExecuteCodeFinished(const std::string& error,
                             const GURL& on_url,
                             const base::ListValue& result) override;

  DECLARE_EXTENSION_FUNCTION("webViewInternal.executeScript",
                             WEBVIEWINTERNAL_EXECUTESCRIPT)

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalExecuteScriptFunction);
};

class WebViewInternalInsertCSSFunction
    : public WebViewInternalExecuteCodeFunction {
 public:
  WebViewInternalInsertCSSFunction();

 protected:
  ~WebViewInternalInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("webViewInternal.insertCSS",
                             WEBVIEWINTERNAL_INSERTCSS)

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalInsertCSSFunction);
};

class WebViewInternalAddContentScriptsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.addContentScripts",
                             WEBVIEWINTERNAL_ADDCONTENTSCRIPTS);

  WebViewInternalAddContentScriptsFunction();

 protected:
  ~WebViewInternalAddContentScriptsFunction() override;

 private:
  ExecuteCodeFunction::ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalAddContentScriptsFunction);
};

class WebViewInternalRemoveContentScriptsFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.removeContentScripts",
                             WEBVIEWINTERNAL_REMOVECONTENTSCRIPTS);

  WebViewInternalRemoveContentScriptsFunction();

 protected:
  ~WebViewInternalRemoveContentScriptsFunction() override;

 private:
  ExecuteCodeFunction::ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalRemoveContentScriptsFunction);
};

class WebViewInternalSetNameFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setName",
                             WEBVIEWINTERNAL_SETNAME);

  WebViewInternalSetNameFunction();

 protected:
  ~WebViewInternalSetNameFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetNameFunction);
};

class WebViewInternalSetAllowTransparencyFunction :
    public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAllowTransparency",
                             WEBVIEWINTERNAL_SETALLOWTRANSPARENCY);

  WebViewInternalSetAllowTransparencyFunction();

 protected:
  ~WebViewInternalSetAllowTransparencyFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetAllowTransparencyFunction);
};

class WebViewInternalSetAllowScalingFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setAllowScaling",
                             WEBVIEWINTERNAL_SETALLOWSCALING);

  WebViewInternalSetAllowScalingFunction();

 protected:
  ~WebViewInternalSetAllowScalingFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetAllowScalingFunction);
};

class WebViewInternalSetZoomFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setZoom",
                             WEBVIEWINTERNAL_SETZOOM);

  WebViewInternalSetZoomFunction();

 protected:
  ~WebViewInternalSetZoomFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetZoomFunction);
};

class WebViewInternalGetZoomFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getZoom",
                             WEBVIEWINTERNAL_GETZOOM);

  WebViewInternalGetZoomFunction();

 protected:
  ~WebViewInternalGetZoomFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetZoomFunction);
};

class WebViewInternalSetZoomModeFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setZoomMode",
                             WEBVIEWINTERNAL_SETZOOMMODE);

  WebViewInternalSetZoomModeFunction();

 protected:
  ~WebViewInternalSetZoomModeFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetZoomModeFunction);
};

class WebViewInternalGetZoomModeFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getZoomMode",
                             WEBVIEWINTERNAL_GETZOOMMODE);

  WebViewInternalGetZoomModeFunction();

 protected:
  ~WebViewInternalGetZoomModeFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetZoomModeFunction);
};

class WebViewInternalFindFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.find", WEBVIEWINTERNAL_FIND);

  WebViewInternalFindFunction();

  // Exposes SendResponse() for use by WebViewInternalFindHelper.
  using WebViewInternalExtensionFunction::SendResponse;

 protected:
  ~WebViewInternalFindFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalFindFunction);
};

class WebViewInternalStopFindingFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.stopFinding",
                             WEBVIEWINTERNAL_STOPFINDING);

  WebViewInternalStopFindingFunction();

 protected:
  ~WebViewInternalStopFindingFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalStopFindingFunction);
};

class WebViewInternalLoadDataWithBaseUrlFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.loadDataWithBaseUrl",
                             WEBVIEWINTERNAL_LOADDATAWITHBASEURL);

  WebViewInternalLoadDataWithBaseUrlFunction();

 protected:
  ~WebViewInternalLoadDataWithBaseUrlFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalLoadDataWithBaseUrlFunction);
};

class WebViewInternalGoFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.go", WEBVIEWINTERNAL_GO);

  WebViewInternalGoFunction();

 protected:
  ~WebViewInternalGoFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGoFunction);
};

class WebViewInternalReloadFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.reload", WEBVIEWINTERNAL_RELOAD);

  WebViewInternalReloadFunction();

 protected:
  ~WebViewInternalReloadFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalReloadFunction);
};

class WebViewInternalSetPermissionFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setPermission",
                             WEBVIEWINTERNAL_SETPERMISSION);

  WebViewInternalSetPermissionFunction();

 protected:
  ~WebViewInternalSetPermissionFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetPermissionFunction);
};

class WebViewInternalOverrideUserAgentFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.overrideUserAgent",
                             WEBVIEWINTERNAL_OVERRIDEUSERAGENT);

  WebViewInternalOverrideUserAgentFunction();

 protected:
  ~WebViewInternalOverrideUserAgentFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalOverrideUserAgentFunction);
};

class WebViewInternalStopFunction : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.stop", WEBVIEWINTERNAL_STOP);

  WebViewInternalStopFunction();

 protected:
  ~WebViewInternalStopFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalStopFunction);
};

class WebViewInternalTerminateFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.terminate",
                             WEBVIEWINTERNAL_TERMINATE);

  WebViewInternalTerminateFunction();

 protected:
  ~WebViewInternalTerminateFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalTerminateFunction);
};

class WebViewInternalClearDataFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.clearData",
                             WEBVIEWINTERNAL_CLEARDATA);

  WebViewInternalClearDataFunction();

 protected:
  ~WebViewInternalClearDataFunction() override;

 private:
  // WebViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(WebViewGuest* guest) override;

  uint32 GetRemovalMask();
  void ClearDataDone();

  // Removal start time.
  base::Time remove_since_;
  // Removal mask, corresponds to StoragePartition::RemoveDataMask enum.
  uint32 remove_mask_;
  // Tracks any data related or parse errors.
  bool bad_message_;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalClearDataFunction);
};

// add Vivaldi customizations below

class WebViewInternalSetVisibleFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setVisible",
                             WEBVIEWINTERNAL_SETVISIBLE);

  WebViewInternalSetVisibleFunction();

 protected:
  ~WebViewInternalSetVisibleFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetVisibleFunction);
};

class WebViewInternalGetThumbnailFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getThumbnail",
                             WEBVIEWINTERNAL_GETTHUMBNAIL);

  WebViewInternalGetThumbnailFunction();

 protected:
  // The default quality setting used when encoding jpegs.
  static const int kDefaultQuality;

  ~WebViewInternalGetThumbnailFunction() override;
  bool RunAsyncSafe(WebViewGuest* guest) override;
  virtual void SendResultFromBitmap(const SkBitmap& screen_capture);
  bool EncodeBitmap(const SkBitmap& screen_capture,
                    std::vector<unsigned char>& data,
                    std::string& mime_type);

  // The format (JPEG vs PNG) of the resulting image.  Set in RunImpl().
  core_api::extension_types::ImageFormat image_format_;

  // Quality setting to use when encoding jpegs.  Set in RunImpl().
  int image_quality_;
  double scale_;
  int height_;
  int width_;

  // Additionally store the it under the current url key.
  bool store_as_current_url_ = false;

 private:
  // Callback for the RWH::CopyFromBackingStore call.
  void CopyFromBackingStoreComplete(const SkBitmap& bitmap,
                                    content::ReadbackResponse response);

  void SendInternalError();

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetThumbnailFunction);
};

class WebViewInternalGetThumbnailFromServiceFunction
    : public WebViewInternalGetThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getThumbnailFromService",
                             WEBVIEWINTERNAL_GETTHUMBNAILFROMSERVICE);

  WebViewInternalGetThumbnailFromServiceFunction();

 protected:
  ~WebViewInternalGetThumbnailFromServiceFunction() override;
  bool RunAsyncSafe(WebViewGuest* guest) override;
  void SendResultFromBitmap(const SkBitmap& screen_capture) override;

  GURL url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetThumbnailFromServiceFunction);
};

class WebViewInternalAddToThumbnailServiceFunction
    : public WebViewInternalGetThumbnailFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.addToThumbnailService",
                             WEBVIEWINTERNAL_ADDTOTHUMBNAILSERVICE);

  WebViewInternalAddToThumbnailServiceFunction();

 protected:
  ~WebViewInternalAddToThumbnailServiceFunction() override;
  bool RunAsyncSafe(WebViewGuest* guest) override;
  void SendResultFromBitmap(const SkBitmap& screen_capture) override;
  void SetPageThumbnailOnUIThread(
      bool send_result,
      scoped_refptr<thumbnails::ThumbnailService> thumbnail_service,
      scoped_refptr<thumbnails::ThumbnailingContext> context,
      const gfx::Image& thumbnail);

  std::string key_;
  GURL url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewInternalAddToThumbnailServiceFunction);
};

class WebViewInternalShowPageInfoFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.showPageInfo",
                             WEBVIEWINTERNAL_SHOWPAGEINFO);

  WebViewInternalShowPageInfoFunction();

 protected:
  ~WebViewInternalShowPageInfoFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalShowPageInfoFunction);
};

class WebViewInternalSetIsFullscreenFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setIsFullscreen",
                             WEBVIEWINTERNAL_SETISFULLSCREEN);

  WebViewInternalSetIsFullscreenFunction();

 protected:
  ~WebViewInternalSetIsFullscreenFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetIsFullscreenFunction);
};

class WebViewInternalSetShowImagesFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.setShowImages",
                             WEBVIEWINTERNAL_SETSHOWIMAGES);

  WebViewInternalSetShowImagesFunction();

 protected:
  ~WebViewInternalSetShowImagesFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalSetShowImagesFunction);
};

class WebViewInternalGetPageHistoryFunction
    : public WebViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webViewInternal.getPageHistory",
                             WEBVIEWINTERNAL_GETPAGEHISTORY);

  WebViewInternalGetPageHistoryFunction();

 protected:
  ~WebViewInternalGetPageHistoryFunction() override;

 private:
  bool RunAsyncSafe(WebViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(WebViewInternalGetPageHistoryFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_VIEW_WEB_VIEW_INTERNAL_API_H_
