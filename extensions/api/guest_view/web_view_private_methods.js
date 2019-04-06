// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the vivaldi specific public-facing API functions
// for the <webview> tag.

var WebViewPrivate = require('webViewPrivate').WebViewPrivate;
var WebViewImpl = require('webView').WebViewImpl;

// An array of <webview>'s public-facing API methods. Methods without custom
// implementations will be given default implementations that call into the
// internal API method with the same name in |WebViewPrivate|. For example, a
// method called 'someApiMethod' would be given the following default
// implementation:
//
// WebViewImpl.prototype.someApiMethod = function(var_args) {
//   if (!this.guest.getId()) {
//     return false;
//   }
//   var args = $Array.concat([this.guest.getId()], $Array.slice(arguments));
//   $Function.apply(WebViewPrivate.someApiMethod, null, args);
//   return true;
// };
//
// These default implementations come from createDefaultApiMethod() in
// web_view_private_impl.js.
var WEB_VIEW_API_METHODS = [
  // Vivaldi methods
  'addToThumbnailService',
  'allowBlockedInsecureContent',
  'discardPage',
  'getFocusedElementInfo',
  'getPageHistory',
  'getThumbnail',
  'getThumbnailFromService',
  'setIsFullscreen',
  'setVisible',
  'showPageInfo',
  'sendRequest',
];

WebViewImpl.prototype.setVisible = function (isVisible) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.setVisible(this.guest.getId(), isVisible);
};

WebViewImpl.prototype.showPageInfo = function (pos) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.showPageInfo(this.guest.getId(), pos);
};

WebViewImpl.prototype.getThumbnail = function (dimensions, callback) {
  if (!this.guest.getId()) {
    return false;
  }
  var args = $Array.concat([this.guest.getId()], $Array.slice(arguments));
  $Function.apply(WebViewPrivate.getThumbnail, null, args);
};

WebViewImpl.prototype.getThumbnailFromService = function (callback) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.getThumbnailFromService(this.guest.getId(), callback);
};

WebViewImpl.prototype.addToThumbnailService = function (
      key, dimensions, callback) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.addToThumbnailService(
      this.guest.getId(), key, dimensions, callback);
};

WebViewImpl.prototype.setIsFullscreen = function (isFullscreen,
                                                  skipWindowState) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.setIsFullscreen(this.guest.getId(),
      isFullscreen, skipWindowState);
};

WebViewImpl.prototype.contextMenusCreate = function (createProperties) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.contextMenusCreate(this.guest.getId(), createProperties);
};

WebViewImpl.prototype.getPageHistory = function (callback) {
  if (!this.guest.getId()) {
    return;
  }
  WebViewPrivate.getPageHistory(this.guest.getId(), callback);
};

WebViewImpl.prototype.discardPage = function () {
  if (!this.guest.getId()) {
    return;
  }
  WebViewPrivate.discardPage(this.guest.getId());
};

WebViewImpl.prototype.allowBlockedInsecureContent = function () {
  if (!this.guest.getId()) {
    return;
  }
  WebViewPrivate.allowBlockedInsecureContent(this.guest.getId());
};

WebViewImpl.prototype.getFocusedElementInfo = function (callback) {
  if (!this.guest.getId()) {
    return;
  }
  WebViewPrivate.getFocusedElementInfo(this.guest.getId(), callback);
};

WebViewImpl.prototype.sendRequest =
  function (url, wasTyped, usePost, postData, extraHeaders) {
    if (!this.guest.getId()) {
      return;
    }
    WebViewPrivate.sendRequest(
      this.guest.getId(), url, wasTyped, usePost, postData, extraHeaders);
  };

// -----------------------------------------------------------------------------

WebViewImpl.getApiMethodsPrivate = function () {
  return WEB_VIEW_API_METHODS;
};
