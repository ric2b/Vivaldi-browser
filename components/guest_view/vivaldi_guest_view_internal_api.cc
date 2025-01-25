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

constexpr char kTabIdKey[] = "tab_id";
constexpr char kInspectTabIdKey[] = "inspect_tab_id";

bool GuestViewInternalCreateGuestFunction::GetExternalWebContents(
    const base::Value::Dict& create_params) {
  auto callback = base::BindOnce(
      &GuestViewInternalCreateGuestFunction::CreateGuestCallback, this);
  content::WebContents* contents = nullptr;

  auto tab_id_value = create_params.FindInt(kTabIdKey);
  auto inspect_tab_id_value = create_params.FindInt(kInspectTabIdKey);
  int tab_id = (tab_id_value.has_value() ? tab_id_value.value()
                                         : (inspect_tab_id_value.has_value()
                                                ? inspect_tab_id_value.value()
                                                : 0));
  if (tab_id) {
    int tab_index = 0;
    bool include_incognito = true;
    Profile* profile = Profile::FromBrowserContext(browser_context());
    WindowController* browser;
    extensions::ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
                                             &browser, &contents,
                                             &tab_index);
  }

  // We also need to clean up guests used for webviews in our docked devtools.
  // Check if there is a "inspect_tab_id" parameter and check towards if there
  // is a devtools item with a webcontents. Find the guest and delete it to
  // prevent dangling guest objects.
  content::WebContents* devtools_contents =
      DevToolsWindow::GetDevtoolsWebContentsForInspectedWebContents(contents);

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
      std::move(callback).Run(guest);
      return true;
    }
  }
  return false;
}

}  // namespace extensions
