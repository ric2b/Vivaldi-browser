// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/infobars/infobars_api.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/extensions/schema/infobars.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

ExtensionFunction::ResponseAction InfobarsSendButtonActionFunction::Run() {
  using vivaldi::infobars::SendButtonAction::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int identifier = params->identifier;
  int tab_id = params->tab_id;
  std::string action = params->action;

  std::string error;
  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, browser_context(),
                                                      &error);
  if (!contents)
    return RespondNow(Error(error));

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


        std::unique_ptr<base::ListValue> args(
          extensions::vivaldi::infobars::OnInfobarRemoved::Create(
            tab_id, delegate->GetIdentifier()));
        ::vivaldi::BroadcastEvent(
          extensions::vivaldi::infobars::OnInfobarRemoved::kEventName,
          std::move(args), browser_context());

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
      }
    }
  }

  return RespondNow(NoArguments());
}

}  // namespace extensions
