// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
#ifndef EXTENSIONS_API_GUEST_VIEW_PARENT_TAB_USER_DATA_H_
#define EXTENSIONS_API_GUEST_VIEW_PARENT_TAB_USER_DATA_H_

#include <optional>

#include "content/public/browser/web_contents_user_data.h"

namespace vivaldi {

class ParentTabUserData
    : public content::WebContentsUserData<ParentTabUserData> {
 public:
  ParentTabUserData(content::WebContents* contents);
  static ParentTabUserData* GetParentTabUserData(
      content::WebContents* contents);
  static std::optional<int> GetParentTabId(content::WebContents* contents);
  static bool ShouldSync(content::WebContents* contents);

  std::optional<int> GetParentTabId() const { return parent_tab_id_; }
  void SetParentTabId(int tab_id) { parent_tab_id_ = tab_id; }
  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  std::optional<int> parent_tab_id_;
};

}  // namespace vivaldi

#endif // EXTENSIONS_API_GUEST_VIEW_PARENT_TAB_USER_DATA_H_
