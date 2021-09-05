// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_TAB_HELPER_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_TAB_HELPER_H_

#include <map>

#include "base/files/file_path.h"
#include "components/page_actions/page_actions_service.h"
#include "content/public/browser/web_contents_user_data.h"

namespace page_actions {
class TabHelper : public content::WebContentsUserData<TabHelper> {
 public:
  ~TabHelper() override;

  const std::map<base::FilePath, bool>& GetScriptOverrides();

 private:
  friend class content::WebContentsUserData<TabHelper>;
  friend class ServiceImpl;

  explicit TabHelper(content::WebContents*);
  TabHelper(const TabHelper& other) = delete;
  TabHelper& operator=(const TabHelper& other) = delete;

  void SetScriptOverride(base::FilePath script, bool enabled);
  bool RemoveScriptOverride(base::FilePath script);

  std::map<base::FilePath, bool> script_overrides_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace page_actions

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_TAB_HELPER_H_
