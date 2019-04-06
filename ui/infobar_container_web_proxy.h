// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef UI_INFOBAR_CONTAINER_WEB_PROXY_H_
#define UI_INFOBAR_CONTAINER_WEB_PROXY_H_

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_container.h"

class Profile;

namespace content {
class WebContents;
}

namespace vivaldi {
/*
This would normally represent the UI of the InfoBar, but in our case, it
will proxy itself to the web ui side.
*/
class ConfirmInfoBarWebProxy : public infobars::InfoBar {
 public:
  ConfirmInfoBarWebProxy(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate,
    content::WebContents* contents);
  ~ConfirmInfoBarWebProxy() override;

  ConfirmInfoBarDelegate* GetDelegate();

  Profile* profile() { return profile_; }
  int tab_id() { return tab_id_; }

 protected:
  void PlatformSpecificShow(bool animate) override;
  void PlatformSpecificHide(bool animate) override;
  void PlatformSpecificOnCloseSoon() override;

private:
  Profile* profile_ = nullptr;
  int tab_id_ = 0;
};

/*
This class is responsible for proxying the infobars to the web ui side.
*/
class InfoBarContainerWebProxy : public infobars::InfoBarContainer {
 public:
  explicit InfoBarContainerWebProxy(Delegate* delegate);
  ~InfoBarContainerWebProxy() override;

 protected:
  // InfobarContainer:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificReplaceInfoBar(infobars::InfoBar* old_infobar,
                                      infobars::InfoBar* new_infobar) override {
  }
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;
  void PlatformSpecificInfoBarStateChanged(bool is_animating) override {}

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerWebProxy);
};

}  // namespace vivaldi

#endif  // UI_INFOBAR_CONTAINER_WEB_PROXY_H_
