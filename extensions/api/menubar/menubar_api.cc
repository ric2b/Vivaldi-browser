//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//

#include "extensions/api/menubar/menubar_api.h"

#include "extensions/schema/menubar.h"
#include "ui/vivaldi_main_menu.h"

namespace extensions {

namespace menubar = vivaldi::menubar;

// static
void MenubarAPI::SendOnActivated(content::BrowserContext* browser_context,
                                 int window_id,
                                 int command_id,
                                 const std::string& parameter) {
  ::vivaldi::BroadcastEvent(
      menubar::OnActivated::kEventName,
      menubar::OnActivated::Create(window_id, command_id, parameter),
      browser_context);
}

ExtensionFunction::ResponseAction MenubarSetupFunction::Run() {
  auto params = menubar::Setup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());
#if defined(OS_MACOSX)
  // There may be no windows. Allow a nullptr profile.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ::vivaldi::CreateVivaldiMainMenu(profile, &params->items, params->mode);
  return RespondNow(NoArguments());
#else
  return RespondNow(Error("NOT IMPLEMENTED"));
#endif  // defined(OS_MACOSX)
}

}  // namespace extensions