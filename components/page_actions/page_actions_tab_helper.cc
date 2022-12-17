// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/page_actions/page_actions_tab_helper.h"

namespace page_actions {
WEB_CONTENTS_USER_DATA_KEY_IMPL(TabHelper);

TabHelper::TabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<TabHelper>(*web_contents) {}
TabHelper::~TabHelper() = default;

void TabHelper::SetScriptOverride(base::FilePath script, bool enabled) {
  script_overrides_[script] = enabled;
}

bool TabHelper::RemoveScriptOverride(base::FilePath script) {
  return (script_overrides_.erase(script) != 0);
}

const std::map<base::FilePath, bool>& TabHelper::GetScriptOverrides() {
  return script_overrides_;
}
}  // namespace page_actions