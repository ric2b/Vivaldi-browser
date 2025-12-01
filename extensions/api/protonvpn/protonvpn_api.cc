// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/protonvpn/protonvpn_api.h"
#include "extensions/schema/protonvpn.h"

#include "vivaldi_account/vivaldi_account_manager.h"
#include "vivaldi_account/vivaldi_account_manager_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

ExtensionFunction::ResponseAction ProtonvpnGetStatusFunction::Run() {
  namespace Results = vivaldi::protonvpn::GetStatus::Results;

  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  if (!account_manager)
    return RespondNow(Error("Account manager is unavailable"));

  // Respond with the custom data.
  vivaldi::protonvpn::Status result;
  result.is_logged_in = account_manager->has_refresh_token();
  return RespondNow(ArgumentList(Results::Create(result)));
}


} // namespace extensions
