// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef UI_QUIT_CONFIRMATION_DIALOG_H_
#define UI_QUIT_CONFIRMATION_DIALOG_H_

#include <string>
#include "base/functional/callback_forward.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class Checkbox;
}  // namespace views

namespace vivaldi {

class VivaldiDialogDelegate {
 public:
  virtual ~VivaldiDialogDelegate() = default;

  virtual std::u16string GetWindowTitle() = 0;
  virtual std::u16string GetBodyText() = 0;
  virtual std::u16string GetCheckboxText() = 0;
};

class VivaldiDialogQuitDelegate : public VivaldiDialogDelegate {
 public:
  std::u16string GetWindowTitle() override;
  std::u16string GetBodyText() override;
  std::u16string GetCheckboxText() override;
};

class VivaldiDialogCloseWindowDelegate : public VivaldiDialogDelegate {
 public:
  std::u16string GetWindowTitle() override;
  std::u16string GetBodyText() override;
  std::u16string GetCheckboxText() override;
};

// Dialog class for prompting users to quit
class VivaldiQuitConfirmationDialog : public views::DialogDelegateView {
 public:
  // If stop_asking is true the user should not be asked for
  // a confirmation again.
  using QuitCallback = base::OnceCallback<void(bool accepted, bool stop_asking)>;

  VivaldiQuitConfirmationDialog(QuitCallback quit_callback,
                                gfx::NativeWindow window,
                                gfx::NativeView view,
                                VivaldiDialogDelegate* delegate);
  ~VivaldiQuitConfirmationDialog() override;
  VivaldiQuitConfirmationDialog(const VivaldiQuitConfirmationDialog&) = delete;
  VivaldiQuitConfirmationDialog& operator=(
      const VivaldiQuitConfirmationDialog&) = delete;

  // views::DialogDelegateView:
  bool Accept() override;
  bool Cancel() override;

  // views::WidgetDelegate:
  ui::mojom::ModalType GetModalType() const override;
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  void RunCallback(bool accepted);

  std::unique_ptr<views::Checkbox> CreateExtraView();

  QuitCallback quit_callback_;

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::Checkbox> checkbox_ = nullptr;

  // The dialog takes ownership of the delegate
  std::unique_ptr<VivaldiDialogDelegate> delegate_;
};

}  // namespace vivaldi

#endif  // UI_QUIT_CONFIRMATION_DIALOG_H_
