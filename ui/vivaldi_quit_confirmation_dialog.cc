// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_quit_confirmation_dialog.h"

#include "app/vivaldi_resources.h"
#include "base/functional/callback.h"
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

std::u16string VivaldiDialogQuitDelegate::GetWindowTitle() {
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_WARNING_TITLE);
}

std::u16string VivaldiDialogQuitDelegate::GetBodyText() {
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_WARNING);
}

std::u16string VivaldiDialogQuitDelegate::GetCheckboxText() {
  return l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_DONOTSHOW);
}

std::u16string VivaldiDialogCloseWindowDelegate::GetWindowTitle() {
  return l10n_util::GetStringUTF16(IDS_WINDOW_CLOSE_CONFIRMATION_WARNING_TITLE);
}

std::u16string VivaldiDialogCloseWindowDelegate::GetBodyText() {
  return l10n_util::GetStringUTF16(IDS_WINDOW_CLOSE_CONFIRMATION_WARNING);
}

std::u16string VivaldiDialogCloseWindowDelegate::GetCheckboxText() {
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
          views::DialogContentType::kText, views::DialogContentType::kText)));

  auto label = std::make_unique<views::Label>();
  label_ = label.get();
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  AddChildView(std::move(label));

  label_->SetText(delegate_->GetBodyText());

  checkbox_ = DialogDelegate::SetExtraView(CreateExtraView());

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params =
      GetDialogWidgetInitParams(this, window, view, gfx::Rect());
  widget->Init(std::move(params));
  widget->Show();
}

VivaldiQuitConfirmationDialog::~VivaldiQuitConfirmationDialog() {}

std::unique_ptr<views::Checkbox>
VivaldiQuitConfirmationDialog::CreateExtraView() {
  auto checkbox = std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_EXIT_CONFIRMATION_DONOTSHOW));
  checkbox->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  return checkbox;
}

bool VivaldiQuitConfirmationDialog::Accept() {
  RunCallback(true);
  return true;
}

bool VivaldiQuitConfirmationDialog::Cancel() {
  RunCallback(false);
  return true;
}

void VivaldiQuitConfirmationDialog::RunCallback(bool accepted) {
  bool stop_asking = checkbox_ && checkbox_->GetChecked();
  std::move(quit_callback_).Run(accepted, stop_asking);
}

ui::mojom::ModalType VivaldiQuitConfirmationDialog::GetModalType() const {
  return ui::mojom::ModalType::kSystem;
}

std::u16string VivaldiQuitConfirmationDialog::GetWindowTitle() const {
  return delegate_->GetWindowTitle();
}

bool VivaldiQuitConfirmationDialog::ShouldShowCloseButton() const {
  return false;
}

gfx::Size VivaldiQuitConfirmationDialog::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      kDefaultWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

}  // namespace vivaldi
