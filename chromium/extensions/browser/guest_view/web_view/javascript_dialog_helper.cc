// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/javascript_dialog_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"

#include "app/vivaldi_apptools.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"

namespace extensions {

namespace {

std::string JavaScriptDialogTypeToString(
    content::JavaScriptDialogType dialog_type) {
  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT:
      return "alert";
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM:
      return "confirm";
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT:
      return "prompt";
    default:
      NOTREACHED() << "Unknown JavaScript Message Type.";
  }
}

}  // namespace

JavaScriptDialogHelper::JavaScriptDialogHelper(WebViewGuest* guest)
    : web_view_guest_(guest) {
}

JavaScriptDialogHelper::~JavaScriptDialogHelper() {
}

void JavaScriptDialogHelper::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  // Keep a local copy here. This is safe as the renderer is blocked until it is
  // called.
  dialog_callback_ = std::move(callback);

  base::Value::Dict request_info;
  request_info.Set(webview::kDefaultPromptText, default_prompt_text);
  request_info.Set(webview::kMessageText, message_text);
  request_info.Set(webview::kMessageType,
                   JavaScriptDialogTypeToString(dialog_type));
  request_info.Set(guest_view::kUrl,
                   render_frame_host->GetLastCommittedURL().spec());

  WebViewPermissionHelper* web_view_permission_helper =
      web_view_guest_->web_view_permission_helper();
  web_view_permission_helper->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_JAVASCRIPT_DIALOG, std::move(request_info),
      base::BindOnce(&JavaScriptDialogHelper::OnPermissionResponse,
                     weak_factory_.GetWeakPtr()),
      false /* allowed_by_default */);
}

void JavaScriptDialogHelper::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  if (vivaldi::IsVivaldiRunning()) {
    // NOTE(pettern@vivaldi.com): We want beforeunload dialogs in Vivaldi,
    // so call the full implementation here.
    content::JavaScriptDialogManager* tab_dialog_manager =
        javascript_dialogs::TabModalDialogManager::FromWebContents(
            web_contents);
    if (tab_dialog_manager) {
      tab_dialog_manager->RunBeforeUnloadDialog(web_contents, render_frame_host,
                                                is_reload, std::move(callback));
      return;
    }
  }
  // This is called if the guest has a beforeunload event handler.
  // This callback allows navigation to proceed.
  std::move(callback).Run(true, std::u16string());
}

bool JavaScriptDialogHelper::HandleJavaScriptDialog(
    content::WebContents* web_contents,
    bool accept,
    const std::u16string* prompt_override) {
  return false;
}

void JavaScriptDialogHelper::CancelDialogs(content::WebContents* web_contents,
                                           bool reset_state) {
  if (vivaldi::IsVivaldiRunning()) {
    // VB-104649 Call full implementation to cancel dialog
    content::JavaScriptDialogManager* tab_dialog_manager =
        javascript_dialogs::TabModalDialogManager::FromWebContents(
            web_contents);
    if (tab_dialog_manager) {
      tab_dialog_manager->CancelDialogs(web_contents, reset_state);
      return;
    }
  }
// Calling the callback will resume the renderer.
  if (dialog_callback_) {
    std::move(dialog_callback_).Run(false, std::u16string());
    dialog_callback_.Reset();
  }
}

void JavaScriptDialogHelper::OnPermissionResponse(bool allow,
    const std::string& user_input) {
  bool allowed_and_attached = allow && web_view_guest_->attached();
  std::move(dialog_callback_).Run(allowed_and_attached,
                          base::UTF8ToUTF16(user_input));
}

}  // namespace extensions
