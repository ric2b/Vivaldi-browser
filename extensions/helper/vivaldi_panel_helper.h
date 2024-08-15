#pragma once

#include "content/public/browser/web_contents_user_data.h"
#include "components/sessions/core/session_id.h"

namespace extensions {

class VivaldiPanelHelper
    : public content::WebContentsUserData<VivaldiPanelHelper> {
 public:
  friend class content::WebContentsUserData<VivaldiPanelHelper>;

  VivaldiPanelHelper(content::WebContents* web_contents,
                     const std::string& view_name);

  ~VivaldiPanelHelper() override;

  int tab_id() const {
    return id_.id();
  }

 private:
  std::string view_name_;
  SessionID id_;

  static const int &kUserDataKey;
};

}  // namespace extensions
