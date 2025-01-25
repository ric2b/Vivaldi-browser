// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "app/vivaldi_command_controller.h"

#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/api/menubar/menubar_api.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#endif

namespace chrome {
void BrowserCommandController::InitVivaldiCommandState() {
  vivaldi::UpdateCommandsForVivaldi(&command_updater_);
}
}  // namespace chrome

namespace vivaldi {

void SetVivaldiScrollType(int scrollType) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Browser* browser = chrome::FindLastActive();
  if (browser) {
    extensions::VivaldiUtilitiesAPI::ScrollType(browser->profile(), scrollType);
  }
#endif
}

bool GetIsEnabledWithNoWindows(int action, bool* enabled) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::MenubarAPI::GetIsEnabledWithNoWindows(action, enabled);
#else
  return false;
#endif
}

bool GetIsEnabled(int action, bool hasWindow, bool* enabled) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::MenubarAPI::GetIsEnabled(action, hasWindow, enabled);
#else
  return false;
#endif
}

bool GetIsSupportedInSettings(int action) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::MenubarAPI::GetIsSupportedInSettings(action);
#else
  return false;
#endif
}

bool HasActiveWindow() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  return extensions::MenubarAPI::HasActiveWindow();
#else
  return false;
#endif
}

void UpdateCommandsForVivaldi(CommandUpdater* command_updater_) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MenubarAPI::UpdateCommandEnabled(command_updater_);
#endif
}

bool ExecuteVivaldiCommands(Browser* browser, int id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = browser->profile()->GetOriginalProfile();
  return extensions::MenubarAPI::HandleActionById(profile,
      browser->session_id().id(), id);
#else
  return false;
#endif
}

}  // namespace vivaldi
