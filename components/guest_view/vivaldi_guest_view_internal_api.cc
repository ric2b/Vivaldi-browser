// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/api/guest_view/guest_view_internal_api.h"

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "extensions/vivaldi_browser_component_wrapper.h"

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
    VivaldiBrowserComponentWrapper::GetInstance()
        ->ExtensionTabUtilGetTabById(tab_id, profile,
                                               include_incognito,
                                             &browser, &contents,
                                             &tab_index);
  }

  // We also need to clean up guests used for webviews in our docked devtools.
  // Check if there is a "inspect_tab_id" parameter and check towards if there
  // is a devtools item with a webcontents. Find the guest and delete it to
  // prevent dangling guest objects.
  content::WebContents* devtools_contents =
      VivaldiBrowserComponentWrapper::GetInstance()->
          DevToolsWindowGetDevtoolsWebContentsForInspectedWebContents(
              contents);

  if (devtools_contents) {
    contents = devtools_contents;
  }

  GuestViewBase* guest = GuestViewBase::FromWebContents(contents);

  if (guest) {
    // note (ondrej@vivaldi) VB-113067: Simplified explanation: if there are >1
    // webviews with the same tab_id in the DOM, none of the guests are
    // initially attached, obviously.
    //
    // Calling the callback in response doesn't immediately attach the guest.
    // Instead, the guest_instance_id is sent back to the renderer, which then
    // calls "attach" again. Meanwhile, if a second guest is needed, this code
    // is called a second time, and we end up calling the same callback twice,
    // which causes the well-known crash.
    //
    // Instead of checking whether the guest is attached (which it is not), we
    // need to check whether the callback has already been used.
    if (!guest->creation_confirmed()) {
      std::move(callback).Run(guest);
      return true;
    }
  }
  return false;
}

}  // namespace extensions
