// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/infobar_container_web_proxy.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/extensions/schema/infobars.h"

namespace vivaldi {

using extensions::vivaldi::infobars::ButtonAction;
using extensions::vivaldi::infobars::InfobarButton;

ConfirmInfoBarWebProxy::ConfirmInfoBarWebProxy(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate,
    content::WebContents* contents)
    : infobars::InfoBar(std::move(delegate)),
      profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      tab_id_(SessionTabHelper::IdForTab(contents).id()) {
}

ConfirmInfoBarWebProxy::~ConfirmInfoBarWebProxy() {
}

void ConfirmInfoBarWebProxy::PlatformSpecificShow(bool animate) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();

  extensions::vivaldi::infobars::Infobar infobar;

  infobar.message_text = base::UTF16ToUTF8(delegate->GetMessageText());
  infobar.link_text = base::UTF16ToUTF8(delegate->GetLinkText());

  if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
    InfobarButton button[1] = {};

    button->action = ButtonAction::BUTTON_ACTION_ACCEPT;
    button->prominent = true;
    button->triggers_uac_prompt = delegate->OKButtonTriggersUACPrompt();
    button->text = base::UTF16ToUTF8(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
    infobar.buttons.push_back(std::move(*button));
  }
  if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    InfobarButton button[1] = { };

    button->action = ButtonAction::BUTTON_ACTION_CANCEL;
    button->triggers_uac_prompt = delegate->OKButtonTriggersUACPrompt();
    button->text = base::UTF16ToUTF8(delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));

    if (delegate->GetButtons() == ConfirmInfoBarDelegate::BUTTON_CANCEL) {
      // Apply prominent styling only if the cancel button is the only button.
      button->prominent = true;
    }
    infobar.buttons.push_back(std::move(*button));
  }
  infobar.tab_id = tab_id_;
  infobar.identifier = delegate->GetIdentifier();

  std::unique_ptr<base::ListValue> args(
      extensions::vivaldi::infobars::OnInfobarCreated::Create(infobar));
  vivaldi::BroadcastEvent(
      extensions::vivaldi::infobars::OnInfobarCreated::kEventName,
      std::move(args), profile_);
}

ConfirmInfoBarDelegate* ConfirmInfoBarWebProxy::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

void ConfirmInfoBarWebProxy::PlatformSpecificOnCloseSoon() {
}

void ConfirmInfoBarWebProxy::PlatformSpecificHide(bool animate) {
}

InfoBarContainerWebProxy::InfoBarContainerWebProxy(Delegate* delegate)
  : infobars::InfoBarContainer(delegate) {
}

InfoBarContainerWebProxy::~InfoBarContainerWebProxy() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerWebProxy::PlatformSpecificAddInfoBar(
    infobars::InfoBar* infobar,
    size_t position) {
}

void InfoBarContainerWebProxy::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  ConfirmInfoBarWebProxy* infobar_proxy =
      static_cast<ConfirmInfoBarWebProxy*>(infobar);
  ConfirmInfoBarDelegate* delegate = infobar_proxy->GetDelegate();

  std::unique_ptr<base::ListValue> args(
      extensions::vivaldi::infobars::OnInfobarRemoved::Create(
        infobar_proxy->tab_id(), delegate->GetIdentifier()));
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::infobars::OnInfobarRemoved::kEventName,
      std::move(args), infobar_proxy->profile());
}

}  // namespace vivaldi
