// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIVALDI_LOCATION_BAR_H_
#define UI_VIVALDI_LOCATION_BAR_H_

#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"

class Profile;
class Browser;

namespace content {
class WebContents;
}

/*
This is the Vivaldi implementation of the LocationBar class, but
for the most part it's empty.
*/
class VivaldiLocationBar : public LocationBar {
 public:
  VivaldiLocationBar(Profile* profile, Browser* browser);
  ~VivaldiLocationBar() override;

  // The details necessary to open the user's desired omnibox match.
  GURL GetDestinationURL() const override;
  WindowOpenDisposition GetWindowOpenDisposition() const override;
  ui::PageTransition GetPageTransition() const override;
  void AcceptInput() override {}
  void FocusLocation(bool select_all) override {}
  void FocusSearch() override {}
  void UpdateContentSettingsIcons() override {}
  void UpdateManagePasswordsIconAndBubble() override;
  void UpdateSaveCreditCardIcon() override {}
  void UpdateBookmarkStarVisibility() override {}
  void UpdateLocationBarVisibility(bool visible, bool animate) override {}
  void UpdateLocalCardMigrationIcon() override {}

  // TODO?
  void SaveStateToContents(content::WebContents* contents) override {}
  void Revert() override {}

  const OmniboxView* GetOmniboxView() const override;
  OmniboxView* GetOmniboxView() override;

  // Returns a pointer to the testing interface.
  LocationBarTesting* GetLocationBarForTesting() override;

  class VivaldiManagePasswordsIconView : public ManagePasswordsIconView {
   public:
    VivaldiManagePasswordsIconView() = default;
    virtual ~VivaldiManagePasswordsIconView() = default;

    void SetState(password_manager::ui::State state) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(VivaldiManagePasswordsIconView);
  };

 protected:
  Browser* browser_;

  std::unique_ptr<VivaldiManagePasswordsIconView> icon_view_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiLocationBar);
};

#endif  // UI_VIVALDI_LOCATION_BAR_H_
