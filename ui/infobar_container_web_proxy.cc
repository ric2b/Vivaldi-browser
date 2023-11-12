// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/infobar_container_web_proxy.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/extensions/schema/infobars.h"

namespace vivaldi {

using extensions::vivaldi::infobars::ButtonAction;
using extensions::vivaldi::infobars::InfobarButton;

ConfirmInfoBarWebProxy::ConfirmInfoBarWebProxy(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)) {}

ConfirmInfoBarWebProxy::~ConfirmInfoBarWebProxy() {}

void ConfirmInfoBarWebProxy::PlatformSpecificShow(bool animate) {
  // In some rare and specific cases, the infobar might show up
  // in a Chromium PWA window even though it's been created like
  // a web proxy infobar. Let the base class have a go at it first
  // to handle that case.  See also VB-85190.
  InfoBarView::PlatformSpecificShow(animate);

  ConfirmInfoBarDelegate* delegate = GetDelegate();
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(this);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  tab_id_ = sessions::SessionTabHelper::IdForTab(web_contents).id();

  extensions::vivaldi::infobars::Infobar infobar;

  infobar.message_text = base::UTF16ToUTF8(delegate->GetMessageText());
  infobar.link_text = base::UTF16ToUTF8(delegate->GetLinkText());

  if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
    InfobarButton button[1] = {};

    button->action = ButtonAction::BUTTON_ACTION_ACCEPT;
    button->prominent = true;
    button->text = base::UTF16ToUTF8(
        delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
    infobar.buttons.push_back(std::move(*button));
  }
  if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    InfobarButton button[1] = {};

    button->action = ButtonAction::BUTTON_ACTION_CANCEL;
    button->text = base::UTF16ToUTF8(
        delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));

    if (delegate->GetButtons() == ConfirmInfoBarDelegate::BUTTON_CANCEL) {
      // Apply prominent styling only if the cancel button is the only button.
      button->prominent = true;
    }
    infobar.buttons.push_back(std::move(*button));
  }
  infobar.tab_id = tab_id_;
  infobar.identifier = delegate->GetIdentifier();

  base::Value::List args(
      extensions::vivaldi::infobars::OnInfobarCreated::Create(infobar));
  vivaldi::BroadcastEvent(
      extensions::vivaldi::infobars::OnInfobarCreated::kEventName,
      std::move(args), profile_);
}

ConfirmInfoBarDelegate* ConfirmInfoBarWebProxy::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

InfoBarContainerWebProxy::InfoBarContainerWebProxy(Delegate* delegate)
    : infobars::InfoBarContainer(delegate) {}

InfoBarContainerWebProxy::~InfoBarContainerWebProxy() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerWebProxy::PlatformSpecificAddInfoBar(
    infobars::InfoBar* infobar,
    size_t position) {}

void InfoBarContainerWebProxy::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  if (infobar->delegate()->GetIdentifier() ==
      infobars::InfoBarDelegate::INVALID) {
    // This infobar was handled elsewere. Ie. a devtools window.
    return;
  }
  ConfirmInfoBarWebProxy* infobar_proxy =
      static_cast<ConfirmInfoBarWebProxy*>(infobar);
  ConfirmInfoBarDelegate* delegate = infobar_proxy->GetDelegate();

  base::Value::List args(
      extensions::vivaldi::infobars::OnInfobarRemoved::Create(
          infobar_proxy->tab_id(), delegate->GetIdentifier()));
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::infobars::OnInfobarRemoved::kEventName,
      std::move(args), infobar_proxy->profile());
}

}  // namespace vivaldi
