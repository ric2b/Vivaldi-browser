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

  // IsWebPanel decision is based on <webview> parent_tab_id argument!
  static bool IsWebPanel(content::WebContents* contents);

  std::optional<int> GetParentTabId() const { return parent_tab_id_; }
  void SetParentTabId(int tab_id) { parent_tab_id_ = tab_id; }
  WEB_CONTENTS_USER_DATA_KEY_DECL();

 private:
  // This value is taken from the <webview> parent_tab_id argument.
  // Regular tabs don't have parent_tab_id set.
  // In case of the web-widgets, it is equal to the tab_id which contains the
  // widget.
  // In case of the web-panels, it is equal to 0.
  std::optional<int> parent_tab_id_;
};

}  // namespace vivaldi

#endif // EXTENSIONS_API_GUEST_VIEW_PARENT_TAB_USER_DATA_H_
