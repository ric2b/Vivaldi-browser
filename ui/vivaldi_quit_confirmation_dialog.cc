// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_quit_confirmation_dialog.h"

#include "app/vivaldi_resources.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace vivaldi {

constexpr int kDefaultWidth = 450;  // Default width of the dialog.

base::string16 VivaldiDialogQuitDelegate::GetWindowTitle() {
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_WARNING_TITLE);
}

base::string16 VivaldiDialogQuitDelegate::GetBodyText() {
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_WARNING);
}

base::string16 VivaldiDialogQuitDelegate::GetCheckboxText() {
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_DONOTSHOW);
}

base::string16 VivaldiDialogCloseWindowDelegate::GetWindowTitle() {
  return l10n_util::GetStringUTF16(IDS_WINDOW_CLOSE_CONFIRMATION_WARNING_TITLE);
}

base::string16 VivaldiDialogCloseWindowDelegate::GetBodyText() {
  return l10n_util::GetStringUTF16(IDS_WINDOW_CLOSE_CONFIRMATION_WARNING);
}

base::string16 VivaldiDialogCloseWindowDelegate::GetCheckboxText() {
  // Re-use existing string
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_DONOTSHOW);
}

VivaldiQuitConfirmationDialog::VivaldiQuitConfirmationDialog(
    QuitCallback quit_callback,
    gfx::NativeWindow window,
    gfx::NativeView view,
    VivaldiDialogDelegate* delegate)
    : quit_callback_(std::move(quit_callback)), delegate_(delegate) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(
      views::LayoutProvider::Get()->GetDialogInsetsForContentType(
          views::TEXT, views::TEXT)));

  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  AddChildView(label_);

  label_->SetText(delegate_->GetBodyText());

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params =
      GetDialogWidgetInitParams(this, window, view, gfx::Rect());
  widget->Init(std::move(params));
  widget->Show();
}

VivaldiQuitConfirmationDialog::~VivaldiQuitConfirmationDialog() {
}

std::unique_ptr<views::View> VivaldiQuitConfirmationDialog::CreateExtraView() {
  auto checkbox = std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_DONOTSHOW));
  checkbox->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  checkbox_ = checkbox.get();
  return checkbox;
}

bool VivaldiQuitConfirmationDialog::Accept() {
  std::move(quit_callback_).Run(true);
  return true;
}

bool VivaldiQuitConfirmationDialog::Cancel() {
  std::move(quit_callback_).Run(false);
  return true;
}

base::string16 VivaldiQuitConfirmationDialog::GetDialogButtonLabel(
  ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL);
  }
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

ui::ModalType VivaldiQuitConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 VivaldiQuitConfirmationDialog::GetWindowTitle() const {
  return delegate_->GetWindowTitle();
}

bool VivaldiQuitConfirmationDialog::ShouldShowCloseButton() const {
  return false;
}

bool VivaldiQuitConfirmationDialog::IsChecked() {
  return checkbox_ && checkbox_->GetChecked();
}


gfx::Size VivaldiQuitConfirmationDialog::CalculatePreferredSize() const {
  return gfx::Size(
    kDefaultWidth,
    GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

}  // namespace vivaldi
