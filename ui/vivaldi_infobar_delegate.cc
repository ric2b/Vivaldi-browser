// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "ui/vivaldi_infobar_delegate.h"

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#include "ui/infobar_container_web_proxy.h"

// static
void VivaldiInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    VivaldiInfoBarDelegate::SpawnParams spawn_params) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new VivaldiInfoBarDelegate(std::move(spawn_params)))));
}

void VivaldiInfoBarDelegate::CreateForVivaldi(
    infobars::ContentInfoBarManager* infobar_manager,
    VivaldiInfoBarDelegate::SpawnParams spawn_params) {
  std::unique_ptr<vivaldi::ConfirmInfoBarWebProxy> infobar =
      std::make_unique<vivaldi::ConfirmInfoBarWebProxy>(
          std::unique_ptr<ConfirmInfoBarDelegate>(
              new VivaldiInfoBarDelegate(std::move(spawn_params))),
          std::u16string(), std::u16string());
  infobar_manager->AddInfoBar(std::move(infobar));
}

VivaldiInfoBarDelegate::VivaldiInfoBarDelegate(
    VivaldiInfoBarDelegate::SpawnParams params)
    : message_text_{params.message_text},
      buttons_{params.buttons},
      button_labels_{std::move(params.button_labels)},
      accept_callback_{std::move(params.accept_callback)} {}

VivaldiInfoBarDelegate::~VivaldiInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
VivaldiInfoBarDelegate::GetIdentifier() const {
  return VIVALDI_INFOBAR_DELEGATE_DESKTOP;
}

const gfx::VectorIcon& VivaldiInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kSettingsChromeRefreshIcon;
}

std::u16string VivaldiInfoBarDelegate::GetMessageText() const {
  return message_text_;
}

int VivaldiInfoBarDelegate::GetButtons() const {
  return buttons_;
}

std::u16string VivaldiInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  auto it = button_labels_.find(button);
  if (it != button_labels_.end())
    return it->second;
  return ConfirmInfoBarDelegate::GetButtonLabel(button);
}

bool VivaldiInfoBarDelegate::Accept() {
  if (accept_callback_)
    std::move(accept_callback_).Run();
  return true;
}
