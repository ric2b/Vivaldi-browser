// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/sessions/session_service.h"

#include "app/vivaldi_apptools.h"
#include "base/json/json_reader.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/sessions/core/base_session_service.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/vivaldi_session_service_commands.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "ui/vivaldi_browser_window.h"

void SessionService::SetWindowExtData(const SessionID& window_id,
                                      const std::string& ext_data) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetWindowExtDataCommand(window_id, ext_data));
}

void SessionService::SetTabExtData(const SessionID& window_id,
                                   const SessionID& tab_id,
                                   const std::string& ext_data) {
  if (!ShouldTrackChangesToWindow(window_id))
    return;

  ScheduleCommand(sessions::CreateSetExtDataCommand(tab_id, ext_data));
}

void SessionService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_EXTDATA_UPDATED: {
      content::WebContents* web_contents =
          content::Source<content::WebContents>(source).ptr();
      SessionTabHelper* session_tab_helper =
          SessionTabHelper::FromWebContents(web_contents);
      if (!session_tab_helper)
        return;
      SetTabExtData(session_tab_helper->window_id(),
                    session_tab_helper->session_id(),
                    web_contents->GetExtData());
      break;
    }

    default:
      NOTREACHED();
  }
}

/* static */
bool SessionService::ShouldTrackVivaldiBrowser(Browser* browser) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::unique_ptr<base::Value> json =
      base::JSONReader::Read(browser->ext_data(), options);
  base::DictionaryValue* dict = NULL;
  std::string window_type;
  if (json && json->GetAsDictionary(&dict)) {
    dict->GetString("windowType", &window_type);
    // Don't track popup windows (like settings) in the session.
    // We have "", "popup" and "settings".
    // TODO(pettern): Popup windows still rely on extData, this
    // should go away and we should use the type sent to the apis
    // instead.
    if (window_type == "popup" || window_type == "settings") {
      return false;
    }
  }
  if (static_cast<VivaldiBrowserWindow*>(browser->window())->type() ==
      VivaldiBrowserWindow::WindowType::NORMAL) {
    return true;
  }
  return false;
}

/* static */
bool SessionService::ShouldTrackBrowserOfType(Browser::Type type) {
  sessions::SessionWindow::WindowType window_type =
      WindowTypeForBrowserType(type);
  return window_type == sessions::SessionWindow::TYPE_TABBED;
}

namespace resource_coordinator {

void TabLifecycleUnitSource::TabLifecycleUnit::SetIsDiscarded() {
  SetState(LifecycleUnitState::DISCARDED,
           LifecycleUnitStateChangeReason::EXTENSION_INITIATED);
}

void TabLifecycleUnitExternal::SetIsDiscarded() {}

}  // namespace resource_coordinator
