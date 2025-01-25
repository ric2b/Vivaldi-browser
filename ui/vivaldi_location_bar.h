// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef UI_VIVALDI_LOCATION_BAR_H_
#define UI_VIVALDI_LOCATION_BAR_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/location_bar/location_bar.h"

namespace content {
class WebContents;
}

class VivaldiBrowserWindow;

/*
This is the Vivaldi implementation of the LocationBar class, but
for the most part it's empty.
*/
class VivaldiLocationBar : public LocationBar {
 public:
  VivaldiLocationBar(VivaldiBrowserWindow& window);
  ~VivaldiLocationBar() override;
  VivaldiLocationBar(const VivaldiLocationBar&) = delete;
  VivaldiLocationBar& operator=(const VivaldiLocationBar&) = delete;

  // The details necessary to open the user's desired omnibox match.
  void FocusLocation(bool select_all) override {}
  void FocusSearch() override {}
  void UpdateContentSettingsIcons() override;

  // TODO?
  void SaveStateToContents(content::WebContents* contents) override {}
  void Revert() override {}

  OmniboxView* GetOmniboxView() override;

  content::WebContents* GetWebContents() override;

  LocationBarModel* GetLocationBarModel() override;

  void OnChanged() override {}

  void OnPopupVisibilityChanged() override {}

  void UpdateWithoutTabRestore() override {}

  // Returns a pointer to the testing interface.
  LocationBarTesting* GetLocationBarForTesting() override;

 private:
  // Owner
  const raw_ref<const VivaldiBrowserWindow> window_;
};

#endif  // UI_VIVALDI_LOCATION_BAR_H_
