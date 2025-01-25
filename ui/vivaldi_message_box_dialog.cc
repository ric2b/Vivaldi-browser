// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_message_box_dialog.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/browser/ui/views/message_box_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#include "ui/views/view_utils.h"
#include "ui/views/window/dialog_client_view.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/message_box_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/cocoa/simple_message_box_cocoa.h"
#endif

namespace {

// static
chrome::MessageBoxResult ShowSync(
    gfx::NativeWindow parent,
    const vivaldi::VivaldiMessageBoxDialog::Config& config) {
  static bool g_message_box_is_showing_sync = false;
  // To avoid showing another VivaldiMessageBoxDialog when one is already
  // pending. Otherwise, this might lead to a stack overflow due to infinite
  // runloops.
  if (g_message_box_is_showing_sync)
    return chrome::MESSAGE_BOX_RESULT_NO;

  base::AutoReset<bool> is_showing(&g_message_box_is_showing_sync, true);
  chrome::MessageBoxResult result = chrome::MESSAGE_BOX_RESULT_NO;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  vivaldi::VivaldiMessageBoxDialog::Show(
      parent, config,
      base::BindOnce(
          [](base::RunLoop* run_loop, chrome::MessageBoxResult* out_result,
             chrome::MessageBoxResult messagebox_result) {
            *out_result = messagebox_result;
            run_loop->Quit();
          },
          &run_loop, &result));
  run_loop.Run();
  return result;
}

#if BUILDFLAG(IS_WIN)
UINT GetMessageBoxFlagsFromType(chrome::MessageBoxType type,
                                bool cancel_default) {
  UINT flags = MB_SETFOREGROUND;
  switch (type) {
    case chrome::MESSAGE_BOX_TYPE_WARNING:
      return flags | MB_OK | MB_ICONWARNING;
    case chrome::MESSAGE_BOX_TYPE_QUESTION:
      if (cancel_default) {
        // Default=Cancel implicates a warning type dialog.
        return flags | MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING;
      }
      return flags | MB_YESNO | MB_ICONQUESTION;
  }
  NOTREACHED();
}
#endif

}  // namespace

namespace vivaldi {

////////////////////////////////////////////////////////////////////////////////
// VivaldiMessageBoxDialog, public:

// static
chrome::MessageBoxResult VivaldiMessageBoxDialog::Show(
    gfx::NativeWindow parent,
    const VivaldiMessageBoxDialog::Config& config,
    VivaldiMessageBoxDialog::MessageBoxResultCallback callback) {
  if (!callback)
    return ShowSync(parent, config);

  // Views dialogs cannot be shown outside the UI thread message loop or if the
  // ResourceBundle is not initialized yet.
  // Fallback to logging with a default response or a Windows MessageBox.
  if (!base::CurrentUIThread::IsSet() ||
      !base::RunLoop::IsRunningOnCurrentThread() ||
      !ui::ResourceBundle::HasSharedInstance()) {
#if BUILDFLAG(IS_WIN)
    LOG_IF(ERROR, !config.checkbox_text.empty())
        << "Using native message box - dialog checkbox won't be shown.";
    int result = ui::MessageBox(
        views::HWNDForNativeWindow(parent), base::AsWString(config.message),
        base::AsWString(config.title),
        GetMessageBoxFlagsFromType(config.type, config.cancel_default));
    std::move(callback).Run((result == IDYES || result == IDOK)
                                ? chrome::MESSAGE_BOX_RESULT_YES
                                : chrome::MESSAGE_BOX_RESULT_NO);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
#elif BUILDFLAG(IS_MAC)
    // Even though this function could return a value synchronously here in
    // principle, in practice call sites do not expect any behavior other than a
    // return of DEFERRED and an invocation of the callback.
    std::move(callback).Run(chrome::ShowMessageBoxCocoa(
        config.message, config.type, config.checkbox_text));
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
#else
    LOG(ERROR) << "Unable to show a dialog outside the UI thread message loop: "
               << config.title << " - " << config.message;
    std::move(callback).Run(chrome::MESSAGE_BOX_RESULT_NO);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
#endif
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System modals are only supported on IS_CHROMEOS_ASH.
  const bool is_system_modal = !parent;
#else
  const bool is_system_modal = false;
#endif

  VivaldiMessageBoxDialog* dialog =
      new VivaldiMessageBoxDialog(config, is_system_modal);
  views::Widget* widget =
      constrained_window::CreateBrowserModalDialogViews(dialog, parent);

#if BUILDFLAG(IS_MAC)
  // Mac does not support system modal dialogs. If there is no parent window to
  // attach to, move the dialog's widget on top so other windows do not obscure
  // it.
  if (!parent)
    widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#endif

  // if there was a size specified, use it
  if (!config.size.IsEmpty()) {
    widget->SetSize(config.size);
  }

  widget->Show();

  dialog->Run(std::move(callback));
  return chrome::MESSAGE_BOX_RESULT_DEFERRED;
}

void VivaldiMessageBoxDialog::OnDialogAccepted() {
  if (!message_box_view_->HasVisibleCheckBox() ||
      message_box_view_->IsCheckBoxSelected()) {
    Done(chrome::MESSAGE_BOX_RESULT_YES);
  } else {
    Done(chrome::MESSAGE_BOX_RESULT_NO);
  }
}

std::u16string VivaldiMessageBoxDialog::GetWindowTitle() const {
  return window_title_;
}

views::View* VivaldiMessageBoxDialog::GetContentsView() {
  return message_box_view_;
}

bool VivaldiMessageBoxDialog::ShouldShowCloseButton() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// VivaldiMessageBoxDialog::Config, public:

VivaldiMessageBoxDialog::Config::Config(std::u16string title,
                                        std::u16string message,
                                        chrome::MessageBoxType type,
                                        std::u16string yes_text,
                                        std::u16string no_text,
                                        std::u16string checkbox_text)
    : title(title),
      message(message),
      type(type),
      yes_text(yes_text),
      no_text(no_text),
      checkbox_text(checkbox_text) {}

VivaldiMessageBoxDialog::Config::~Config() {}

////////////////////////////////////////////////////////////////////////////////
// VivaldiMessageBoxDialog, private:
VivaldiMessageBoxDialog::VivaldiMessageBoxDialog(const Config& config,
                                                 bool is_system_modal)
    : window_title_(config.title),
      type_(config.type),
      message_box_view_(new views::MessageBoxView(config.message)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetModalType(is_system_modal ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_WINDOW);
#else
  DCHECK(!is_system_modal);
  SetModalType(ui::mojom::ModalType::kWindow);
#endif
  SetButtons(type_ == chrome::MESSAGE_BOX_TYPE_QUESTION
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kOk));

  SetAcceptCallback(base::BindOnce(&VivaldiMessageBoxDialog::OnDialogAccepted,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&VivaldiMessageBoxDialog::Done,
                                   base::Unretained(this),
                                   chrome::MESSAGE_BOX_RESULT_NO));
  SetCloseCallback(base::BindOnce(&VivaldiMessageBoxDialog::Done,
                                  base::Unretained(this),
                                  chrome::MESSAGE_BOX_RESULT_NO));

  SetOwnedByWidget(true);

  std::u16string ok_text = config.yes_text;
  if (ok_text.empty()) {
    ok_text =
        type_ == chrome::MESSAGE_BOX_TYPE_QUESTION
            ? l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)
            : l10n_util::GetStringUTF16(IDS_OK);
  }
  SetButtonLabel(ui::mojom::DialogButton::kOk, ok_text);

  // Only MESSAGE_BOX_TYPE_QUESTION has a Cancel button.
  if (type_ == chrome::MESSAGE_BOX_TYPE_QUESTION) {
    std::u16string cancel_text = config.no_text;
    if (cancel_text.empty())
      cancel_text = l10n_util::GetStringUTF16(IDS_CANCEL);
    SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_text);
  }

  if (!config.checkbox_text.empty()) {
    message_box_view_->SetCheckBoxLabel(config.checkbox_text);
    SetButtonStyle(ui::mojom::DialogButton::kOk, ui::ButtonStyle::kTonal);
  }

  if (config.cancel_default) {
    SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kCancel));
  }
}

VivaldiMessageBoxDialog::~VivaldiMessageBoxDialog() {
  GetWidget()->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void VivaldiMessageBoxDialog::Run(MessageBoxResultCallback result_callback) {
  GetWidget()->AddObserver(this);
  result_callback_ = std::move(result_callback);
}

void VivaldiMessageBoxDialog::Done(chrome::MessageBoxResult result) {
  CHECK(!result_callback_.is_null());
  std::move(result_callback_).Run(result);
}

views::Widget* VivaldiMessageBoxDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* VivaldiMessageBoxDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

}  // namespace vivaldi
