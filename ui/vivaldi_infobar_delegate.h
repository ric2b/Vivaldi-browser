// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIVALDI_INFOBAR_DELEGATE_H
#define UI_VIVALDI_INFOBAR_DELEGATE_H

#include <map>

#include "base/functional/callback.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

// Represents a customizable infobar.
class VivaldiInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  VivaldiInfoBarDelegate(const VivaldiInfoBarDelegate&) = delete;
  VivaldiInfoBarDelegate& operator=(const VivaldiInfoBarDelegate&) = delete;

  // Customizes the appearance of the InfoBar - add more members as needed.
  struct SpawnParams {
    SpawnParams(std::u16string message_text,
                base::OnceCallback<void(void)> accept_callback)
        : message_text{message_text},
          accept_callback{std::move(accept_callback)} {}

    std::u16string message_text;
    int buttons = BUTTON_OK;
    std::map<InfoBarButton, std::u16string>
        button_labels;  // Labels for buttons. If not specified, default.
    base::OnceCallback<void(void)> accept_callback;
  };

  // Creates a vivaldi infobar and delegate and adds the infobar to
  // |infobar_manager|.
  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     SpawnParams params);

  // Creates a ConfirmInfoBarWebProxy for use in the Vivaldi client.
  static void CreateForVivaldi(infobars::ContentInfoBarManager* infobar_manager,
                               SpawnParams params);

 private:
  VivaldiInfoBarDelegate(SpawnParams params);
  ~VivaldiInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  std::u16string message_text_;
  int buttons_;
  std::map<InfoBarButton, std::u16string> button_labels_;
  base::OnceCallback<void(void)> accept_callback_;
};

#endif /* UI_VIVALDI_INFOBAR_DELEGATE_H */
