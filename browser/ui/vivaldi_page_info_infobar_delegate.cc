// Copyright (c) 2025 Vivaldi. All rights reserved.

#include "chrome/browser/ui/page_info/page_info_infobar_delegate.h"

#include "components/infobars/content/content_infobar_manager.h"

#include "ui/infobar_container_web_proxy.h"

// static
void PageInfoInfoBarDelegate::CreateForVivaldi(
    infobars::ContentInfoBarManager* infobar_manager) {
  std::unique_ptr<vivaldi::ConfirmInfoBarWebProxy> infobar =
      std::make_unique<vivaldi::ConfirmInfoBarWebProxy>(
          std::unique_ptr<ConfirmInfoBarDelegate>(
              new PageInfoInfoBarDelegate()),
          std::u16string(), std::u16string());
  infobar_manager->AddInfoBar(std::move(infobar));
}
