//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "browser/menus/vivaldi_developertools_menu_controller.h"

#include "base/command_line.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/devtools/devtools_connector.h"

namespace vivaldi {

DeveloperToolsMenuController::DeveloperToolsMenuController(
    content::WebContents* web_contents,
    const gfx::Point& location)
  :web_contents_(web_contents),
   browser_(FindBrowserForEmbedderWebContents(web_contents)),
   location_(location),
   enabled_(HasFeature()) {
}


const extensions::Extension*
    DeveloperToolsMenuController::GetExtension() const {
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(browser_->profile());
  return process_manager->GetExtensionForWebContents(web_contents_);
}

bool DeveloperToolsMenuController::HasFeature() {
  const extensions::Extension* platform_app = GetExtension();
  return extensions::Manifest::IsUnpackedLocation(platform_app->location()) ||
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDebugPackedApps);
}

void DeveloperToolsMenuController::PopulateModel(
      ui::SimpleMenuModel* menu_model) {
  if (enabled_) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    // NOTE(pettern): Reload will not work with our app, disable it for now.
    //    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
    //                                    IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
    menu_model->AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                    IDS_CONTENT_CONTEXT_RESTART_APP);
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model->AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                    IDS_CONTENT_CONTEXT_INSPECTELEMENT);
    menu_model->AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                    IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
  }
}

bool DeveloperToolsMenuController::IsCommand(int command_id) const {
  return /* command_id == IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP ||*/
      command_id == IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP ||
      command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
      command_id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE;
}


bool DeveloperToolsMenuController::HandleCommand(int command_id) {
  if (enabled_) {
    const extensions::Extension* platform_app = GetExtension();

    switch (command_id) {
      case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
        if (platform_app && platform_app->is_platform_app()) {
          extensions::ExtensionSystem::Get(browser_->profile())
              ->extension_service()
              ->ReloadExtension(platform_app->id());
        }
        return true;

      case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
        if (platform_app && platform_app->is_platform_app()) {
          extensions::DevtoolsConnectorAPI::CloseAllDevtools();
          chrome::AttemptRestart();
        }
        return true;

      case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
        DevToolsWindow::InspectElement(web_contents_->GetMainFrame(),
                                       location_.x(), location_.y());
        return true;

      case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
        if (platform_app && platform_app->is_platform_app()) {
          extensions::devtools_util::InspectBackgroundPage(platform_app,
                                                           browser_->profile());
        }
        return true;
    }
  }
  return false;
}

bool DeveloperToolsMenuController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT) {
    // We used to have ui::VKEY_I ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN here
    // but it makes no sense as that shortcut is reserved for inspecting a
    // regular web page in chromium.
    *accelerator = ui::Accelerator();
    return true;
  }



  return false;
}


}  // namespace vivaldi