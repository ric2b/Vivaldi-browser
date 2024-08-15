// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the vivaldi specific public-facing API functions
// for the <webview> tag.

var WebViewPrivate = getInternalApi ? getInternalApi("webViewPrivate") : require("webViewPrivate").WebViewPrivate;

//var WebViewPrivate = require('webViewPrivate').WebViewPrivate;
//var WebViewImpl = require('webView').WebViewImpl;

function RegisterVivaldiWebViewExtensions(WebViewImpl) {

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

WebViewImpl.prototype.setIsFullscreen = function (isFullscreen) {
  if (!this.guest.getId()) {
    return false;
  }
  WebViewPrivate.setIsFullscreen(this.guest.getId(), isFullscreen);
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

WebViewImpl.prototype.getPageSelection = function () {
  if (!this.guest.getId()) {
    return;
  }
  return WebViewPrivate.getPageSelection(this.guest.getId());
};

WebViewImpl.prototype.sendRequest =
  function (url, transition, fromUrlField, usePost, postData, extraHeaders) {
    if (!this.guest.getId()) {
      return;
    }
    WebViewPrivate.sendRequest(
      this.guest.getId(), url, transition, fromUrlField, usePost, postData, extraHeaders);
  };

}
// -----------------------------------------------------------------------------

// Exports.
exports.$set('RegisterVivaldiWebViewExtensions', RegisterVivaldiWebViewExtensions);
