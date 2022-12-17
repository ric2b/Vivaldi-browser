// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/ui/browser.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"

// static
Browser::CreateParams Browser::CreateParams::CreateForDevToolsForVivaldi(
    Profile* profile) {
  CreateParams params(TYPE_POPUP, profile, true);
  params.app_name = DevToolsWindow::kDevToolsApp;
  params.trusted_source = true;
  params.is_vivaldi = true;
  return params;
}

void Browser::set_viv_ext_data(const std::string& viv_ext_data) {
  viv_ext_data_ = viv_ext_data;

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (session_service)
    session_service->SetWindowVivExtData(session_id(), viv_ext_data_);
}

void Browser::DoBeforeUnloadFired(content::WebContents* web_contents,
                                  bool proceed,
                                  bool* proceed_to_fire_unload) {
  BeforeUnloadFired(web_contents, proceed, proceed_to_fire_unload);
}

void Browser::DoCloseContents(content::WebContents* source) {
  CloseContents(source);
}
