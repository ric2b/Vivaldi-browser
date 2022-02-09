// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/ui/browser.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"

Browser::CreateParams::~CreateParams() = default;

// static
Browser::CreateParams Browser::CreateParams::CreateForDevToolsForVivaldi(
    Profile* profile) {
  CreateParams params(TYPE_POPUP, profile, true);
  params.app_name = DevToolsWindow::kDevToolsApp;
  params.trusted_source = true;
  params.is_vivaldi = true;
  return params;
}

void Browser::set_ext_data(const std::string& ext_data) {
  ext_data_ = ext_data;

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (session_service)
    session_service->SetWindowExtData(session_id(), ext_data_);
}

void Browser::DoBeforeUnloadFired(content::WebContents* web_contents,
                                  bool proceed,
                                  bool* proceed_to_fire_unload) {
  BeforeUnloadFired(web_contents, proceed, proceed_to_fire_unload);
}

void Browser::DoCloseContents(content::WebContents* source) {
  CloseContents(source);
}
