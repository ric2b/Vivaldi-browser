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

content::WebContents* Browser::AddNewContentsVivaldi(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  return AddNewContents(source, std::move(new_contents), target_url,
                        disposition, window_features, user_gesture,
                        was_blocked);
}

// Overrides WebContentsDelegate::IsWebApp.
bool Browser::IsWebApp() {
  return is_type_app();
}
