// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/infobars/infobars_api.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/extensions/schema/infobars.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

InfobarsSendButtonActionFunction::InfobarsSendButtonActionFunction() {

}

InfobarsSendButtonActionFunction::~InfobarsSendButtonActionFunction() {

}

bool InfobarsSendButtonActionFunction::RunAsync() {
  std::unique_ptr<vivaldi::infobars::SendButtonAction::Params> params(
      vivaldi::infobars::SendButtonAction::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int identifier = params->identifier;
  int tab_id = params->tab_id;
  std::string action = params->action;

  content::WebContents* contents =
    ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());
  if (contents) {
    InfoBarService *service = InfoBarService::FromWebContents(contents);
    for (size_t i = 0; i < service->infobar_count(); i++) {
      infobars::InfoBar* infobar = service->infobar_at(i);
      if (infobar->delegate()->GetIdentifier() ==
          static_cast<infobars::InfoBarDelegate::InfoBarIdentifier>(
              identifier)) {
        ConfirmInfoBarDelegate* delegate =
            infobar->delegate()->AsConfirmInfoBarDelegate();
        DCHECK(delegate);
        if (delegate) {
          if (action == extensions::vivaldi::infobars::ToString(
                            extensions::vivaldi::infobars::ButtonAction::
                                BUTTON_ACTION_ACCEPT)) {
            delegate->Accept();
          } else if (action == extensions::vivaldi::infobars::ToString(
                                   extensions::vivaldi::infobars::ButtonAction::
                                       BUTTON_ACTION_CANCEL)) {
            delegate->Cancel();
          } else if (action == extensions::vivaldi::infobars::ToString(
                                   extensions::vivaldi::infobars::ButtonAction::
                                       BUTTON_ACTION_DISMISS)) {
            delegate->InfoBarDismissed();
          }
          infobar->RemoveSelf();

          std::unique_ptr<base::ListValue> args(
            extensions::vivaldi::infobars::OnInfobarRemoved::Create(
              tab_id, delegate->GetIdentifier()));
          ::vivaldi::BroadcastEvent(
              extensions::vivaldi::infobars::OnInfobarRemoved::kEventName,
              std::move(args), GetProfile());
        }
      }
    }
  }

  SendResponse(true);
  return true;
}

}  // namespace extensions
