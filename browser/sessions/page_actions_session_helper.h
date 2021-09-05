// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved.

#ifndef BROWSER_SESSIONS_PAGE_ACTIONS_SESSION_HELPER_H_
#define BROWSER_SESSIONS_PAGE_ACTIONS_SESSION_HELPER_H_

#include "components/page_actions/page_actions_service.h"

class SessionService;
class Profile;

class PageActionsSessionHelper : public page_actions::Service::Observer {
 public:
  PageActionsSessionHelper(SessionService* session_service);
  ~PageActionsSessionHelper() override;
  PageActionsSessionHelper(const PageActionsSessionHelper& other) = delete;
  PageActionsSessionHelper& operator=(const PageActionsSessionHelper& other) =
      delete;

  void OnScriptOverridesChanged(
      content::WebContents* tab_contents,
      const base::FilePath& script_path,
      page_actions::Service::ScriptOverride script_override) override;

 private:
  SessionService* session_service_;
  Profile* profile_;
};

#endif  // BROWSER_SESSIONS_PAGE_ACTIONS_SESSION_HELPER_H_
