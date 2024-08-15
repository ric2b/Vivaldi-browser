#include "extensions/helper/vivaldi_panel_helper.h"
#include "ui/content/vivaldi_tab_check.h"

namespace extensions {

const int &VivaldiPanelHelper::kUserDataKey = VivaldiTabCheck::kVivaldiPanelHelperContextKey;

VivaldiPanelHelper::VivaldiPanelHelper(content::WebContents* web_contents,
                                       const std::string& view_name)
    : content::WebContentsUserData<VivaldiPanelHelper>(*web_contents),
      view_name_(view_name),
      id_(SessionID::NewUnique())
  {}


VivaldiPanelHelper::~VivaldiPanelHelper() {}

}  // namespace extensions
