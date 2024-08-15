// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIEWS_VIVALDI_MESSAGE_BOX_DIALOG_H
#define UI_VIEWS_VIVALDI_MESSAGE_BOX_DIALOG_H

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/simple_message_box.h"

#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

namespace vivaldi {

/** Customized Message box dialog with more options.
 * Clone of the original code found in chrome/browser/ui/views/message_box_dialog.h
 */
class VivaldiMessageBoxDialog : public views::DialogDelegate,
                                public views::WidgetObserver {
 public:
  using MessageBoxResultCallback =
      base::OnceCallback<void(chrome::MessageBoxResult result)>;

  VivaldiMessageBoxDialog(const VivaldiMessageBoxDialog&) = delete;
  VivaldiMessageBoxDialog& operator=(const VivaldiMessageBoxDialog&) = delete;

  struct Config {
    Config(
      std::u16string title,
      std::u16string message,
      chrome::MessageBoxType type,
      std::u16string yes_text,
      std::u16string no_text,
      std::u16string checkbox_text);

    ~Config();

    std::u16string title;
    std::u16string message;
    chrome::MessageBoxType type;
    std::u16string yes_text;
    std::u16string no_text;
    std::u16string checkbox_text;
    gfx::Size size; // When set, it get to be the overall window size.
    bool cancel_default = false; // Cancel button should be default.
  };

  static chrome::MessageBoxResult Show(
      gfx::NativeWindow parent,
      const Config& config,
      MessageBoxResultCallback callback = MessageBoxResultCallback());

  // views::DialogDelegate:
  std::u16string GetWindowTitle() const override;
  views::View* GetContentsView() override;
  bool ShouldShowCloseButton() const override;

 private:
 VivaldiMessageBoxDialog(const Config &config,
                         bool is_system_modal);
  ~VivaldiMessageBoxDialog() override;

  void Run(MessageBoxResultCallback result_callback);
  void Done(chrome::MessageBoxResult result);

  void OnDialogAccepted();

  // Widget:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  const std::u16string window_title_;
  const chrome::MessageBoxType type_;
  raw_ptr<views::MessageBoxView> message_box_view_;
  MessageBoxResultCallback result_callback_;
};

}  // namespace vivaldi

#endif /* UI_VIEWS_VIVALDI_MESSAGE_BOX_DIALOG_H */
