// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/infobar_container_web_proxy.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/tools/vivaldi_tools.h"
#include "vivaldi/extensions/schema/infobars.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {

using extensions::vivaldi::infobars::ButtonAction;
using extensions::vivaldi::infobars::InfobarButton;

ConfirmInfoBarWebProxy::ConfirmInfoBarWebProxy(
    std::unique_ptr<infobars::InfoBarDelegate> delegate,
    const std::u16string shared_tab_name,
    const std::u16string capturer_name)
    : InfoBar(std::move(delegate)) {
  shared_tab_name_ = shared_tab_name;
  capturer_name_ = capturer_name;
}

ConfirmInfoBarWebProxy::~ConfirmInfoBarWebProxy() {}

void ConfirmInfoBarWebProxy::PlatformSpecificHide(bool animate) {
  base::Value::List args(
      extensions::vivaldi::infobars::OnInfobarRemoved::Create(
          tab_id_, 0));
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::infobars::OnInfobarRemoved::kEventName,
      std::move(args), profile_);
}

void ConfirmInfoBarWebProxy::PlatformSpecificShow(bool animate) {

  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(this);

  if (web_contents) {
    profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
    tab_id_ = sessions::SessionTabHelper::IdForTab(web_contents).id();
  }

  extensions::vivaldi::infobars::Infobar infobar;

  if (delegate()->GetIdentifier() ==
      infobars::InfoBarDelegate::TAB_SHARING_INFOBAR_DELEGATE) {
    TabSharingInfoBarDelegate* delegate =
        static_cast<TabSharingInfoBarDelegate*>(this->delegate());

    infobar.message_text = base::UTF16ToUTF8(GetMessageText());
    infobar.link_text = base::UTF16ToUTF8(delegate->GetLinkText());

    if (delegate->GetButtons() & TabSharingInfoBarDelegate::kShareThisTabInstead) {
      InfobarButton button[1] = {};

      button->action = ButtonAction::kAccept;
      button->prominent = true;
      button->text = base::UTF16ToUTF8(delegate->GetButtonLabel(
          TabSharingInfoBarDelegate::kShareThisTabInstead));
      infobar.buttons.push_back(std::move(*button));
    }
    if (delegate->GetButtons() & TabSharingInfoBarDelegate::kStop) {
      InfobarButton button[1] = {};

      button->action = ButtonAction::kCancel;
      button->text = base::UTF16ToUTF8(
          delegate->GetButtonLabel(TabSharingInfoBarDelegate::kStop));

      if (delegate->GetButtons() == TabSharingInfoBarDelegate::kStop) {
        // Apply prominent styling only if the cancel button is the only button.
        button->prominent = true;
      }
      infobar.buttons.push_back(std::move(*button));
    }
  } else {
    ConfirmInfoBarDelegate* delegate = GetDelegate();

    infobar.message_text = base::UTF16ToUTF8(delegate->GetMessageText());
    infobar.link_text = base::UTF16ToUTF8(delegate->GetLinkText());

    if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
      InfobarButton button[1] = {};

      button->action = ButtonAction::kAccept;
      button->prominent = true;
      button->text = base::UTF16ToUTF8(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK));
      infobar.buttons.push_back(std::move(*button));
    }
    if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
      InfobarButton button[1] = {};

      button->action = ButtonAction::kCancel;
      button->text = base::UTF16ToUTF8(
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL));

      if (delegate->GetButtons() == ConfirmInfoBarDelegate::BUTTON_CANCEL) {
        // Apply prominent styling only if the cancel button is the only button.
        button->prominent = true;
      }
      infobar.buttons.push_back(std::move(*button));
    }
  }
  infobar.tab_id = tab_id_;
  infobar.identifier = delegate()->GetIdentifier();
  infobar.is_closeable = delegate()->IsCloseable();

  base::Value::List args(
      extensions::vivaldi::infobars::OnInfobarCreated::Create(infobar));
  vivaldi::BroadcastEvent(
      extensions::vivaldi::infobars::OnInfobarCreated::kEventName,
      std::move(args), profile_);
}

ConfirmInfoBarDelegate* ConfirmInfoBarWebProxy::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

std::u16string GetMessageTextCapturing(bool shared_tab,
                                       const std::u16string& shared_tab_name,
                                       const std::u16string& app_name) {
  if (shared_tab) {
    return l10n_util::GetStringFUTF16(
        IDS_TAB_SHARING_INFOBAR_SHARING_CURRENT_TAB_LABEL, app_name);
  }
  return !shared_tab_name.empty()
             ? l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_TAB_LABEL,
                   shared_tab_name, app_name)
             : l10n_util::GetStringFUTF16(
                   IDS_TAB_SHARING_INFOBAR_SHARING_ANOTHER_UNTITLED_TAB_LABEL,
                   app_name);
}

std::u16string ConfirmInfoBarWebProxy::GetMessageText() const {
  bool thisiscaptured = false;
  return GetMessageTextCapturing(thisiscaptured, shared_tab_name_, capturer_name_);
}

InfoBarContainerWebProxy::InfoBarContainerWebProxy(Delegate* delegate)
    : infobars::InfoBarContainer(delegate) {}

InfoBarContainerWebProxy::~InfoBarContainerWebProxy() {
  RemoveAllInfoBarsForDestruction();
}

void InfoBarContainerWebProxy::PlatformSpecificAddInfoBar(
    infobars::InfoBar* new_infobar,
    size_t position) {
}

  void InfoBarContainerWebProxy::PlatformSpecificReplaceInfoBar(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
}

void InfoBarContainerWebProxy::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
}

}  // namespace vivaldi
