// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/api/guest_view/guest_view_internal_api.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"

using guest_view::GuestViewBase;
using guest_view::GuestViewManager;

namespace extensions {

bool GuestViewInternalCreateGuestFunction::GetExternalWebContents(
  base::DictionaryValue* create_params) {
  GuestViewManager::WebContentsCreatedCallback callback = base::BindOnce(
    &GuestViewInternalCreateGuestFunction::CreateGuestCallback, this);
  content::WebContents* contents = nullptr;

  std::string tab_id_as_string;
  std::string guest_id_str;
  if (create_params->GetString("tab_id", &tab_id_as_string) ||
      create_params->GetString("inspect_tab_id", &tab_id_as_string)) {
    int tab_id = atoi(tab_id_as_string.c_str());
    int tab_index = 0;
    bool include_incognito = true;
    Profile* profile = Profile::FromBrowserContext(context_);
    Browser* browser;
    TabStripModel* tab_strip;
    extensions::ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
      &browser, &tab_strip, &contents,
      &tab_index);
  }

  // We also need to clean up guests used for webviews in our docked devtools.
  // Check if there is a "inspect_tab_id" parameter and check towards if there
  // is a devtools item with a webcontents. Find the guest and delete it to
  // prevent dangling guest objects.
  content::WebContents* devtools_contents =
  DevToolsWindow::GetDevtoolsWebContentsForInspectedWebContents(
    contents);

  if (devtools_contents) {
    contents = devtools_contents;
  }

  GuestViewBase* guest = GuestViewBase::FromWebContents(contents);

  if (guest) {
    // If there is a guest with the WebContents already then
    // use this if it is not yet attached. This is done through the
    // WebContentsImpl::CreateNewWindow code-path. Ie. clicking a link in a
    // webpage with target set. The guest has been created with
    // GuestViewManager::CreateGuestWithWebContentsParams.
    if (!guest->attached()) {
      std::move(callback).Run(guest->web_contents());
      return true;
    }
    // Otherwise we need to make sure the guest is recreated since the guest is
    // un-mounted and mounted.
    guest->Destroy(true);
  }
  return false;
}

}  // namespace extensions
