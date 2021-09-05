// Copyright (c) 2020 Vivaldi Technologies

#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"

#include "base/files/file_path.h"
#include "components/page_actions/page_actions_tab_helper.h"
#include "components/sessions/content/content_live_tab.h"

const std::map<base::FilePath, bool>*
ChromeTabRestoreServiceClient::GetPageActionOverridesForTab(
    sessions::LiveTab* tab) {
  auto* page_actions_helper = page_actions::TabHelper::FromWebContents(
      static_cast<sessions::ContentLiveTab*>(tab)->web_contents());
  if (!page_actions_helper)
    return nullptr;
  return &page_actions_helper->GetScriptOverrides();
}
