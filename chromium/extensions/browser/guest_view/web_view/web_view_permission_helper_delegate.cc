// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace extensions {

namespace {

void ProxyCanDownloadCallback(
    const base::Callback<void(const content::DownloadItemAction&)>& callback,
    bool allow) {
  callback.Run(content::DownloadItemAction(allow, false, false));
}

}  // namespace

WebViewPermissionHelperDelegate::WebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper)
    : content::WebContentsObserver(
          web_view_permission_helper->web_view_guest()->web_contents()),
      web_view_permission_helper_(web_view_permission_helper) {
}

WebViewPermissionHelperDelegate::~WebViewPermissionHelperDelegate() {
}

void WebViewPermissionHelperDelegate::CanDownload(
      const GURL& url,
      const std::string& request_method,
      const base::Callback<void(bool)>& callback) {
}

void WebViewPermissionHelperDelegate::CanDownload(
      const GURL& url,
      const std::string& request_method,
      const content::DownloadInformation& info,
      const base::Callback<
          void(const content::DownloadItemAction&)>& callback) {
  CanDownload(url,request_method,
      base::Bind(ProxyCanDownloadCallback, callback));
}


}  // namespace extensions
