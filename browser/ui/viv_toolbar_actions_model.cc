// Copyright (c) 2018 Vivaldi Technologies

#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"

#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "extensions/browser/extension_registry.h"

void ToolbarActionsModel::OnPageActionsUpdated(
  content::WebContents* web_contents) {

  // Vivaldi: We use a unified model for both action- and browser-actions.
  Browser* browser =
    web_contents ? chrome::FindBrowserWithWebContents(web_contents) : nullptr;
  bool is_vivaldi = (browser && browser->is_vivaldi());

  if (!is_vivaldi)
    return;

  const extensions::ExtensionSet& extensions =
    extensions::ExtensionRegistry::Get(profile_)
    ->enabled_extensions();

  extensions::ExtensionActionManager* action_manager =
    extensions::ExtensionActionManager::Get(profile_);

  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
    it != extensions.end();
    ++it) {
    const extensions::Extension* extension = it->get();

    ExtensionAction* action = action_manager->GetExtensionAction(*extension);

    if (action && action->action_type() == extensions::ActionInfo::TYPE_PAGE) {
      // all extensions with pageactions must be updated
      for (auto& observer : observers_)
        observer.OnToolbarActionUpdated(extension->name());
    }
  }
}
