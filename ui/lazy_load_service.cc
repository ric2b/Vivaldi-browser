// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "ui/lazy_load_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "prefs/vivaldi_gen_prefs.h"

namespace vivaldi {

const char LazyLoadService::kLazyLoadIsSafe[] = "lazy_load_is_safe";

LazyLoadService::LazyLoadService(Profile* profile) : profile_(profile) {
  SessionRestore::AddObserver(this);
  // Make sure the TabLifecycleUnitSource instance has been set up.
  g_browser_process->GetTabManager();
  resource_coordinator::GetTabLifecycleUnitSource()->AddObserver(this);
}

void LazyLoadService::Shutdown() {
  SessionRestore::RemoveObserver(this);
  resource_coordinator::GetTabLifecycleUnitSource()->RemoveObserver(this);
}

void LazyLoadService::OnLifecycleUnitCreated(
    resource_coordinator::LifecycleUnit* lifecycle_unit) {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetBoolean(vivaldiprefs::kTabsDeferLoadingAfterRestore))
    return;
  if (lifecycle_unit->GetState() ==
      resource_coordinator::LifecycleUnitState::DISCARDED)
    return;
  resource_coordinator::TabLifecycleUnitExternal* tab_lifecycle_unit_external =
      lifecycle_unit->AsTabLifecycleUnitExternal();
  if (!tab_lifecycle_unit_external)
    return;

  content::WebContents* web_contents =
      tab_lifecycle_unit_external->GetWebContents();

  if (!web_contents->GetUserData(&kLazyLoadIsSafe))
    return;

  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return;

  // Update the WebContents state. See https://github.com/WICG/web-lifecycle.
  // This needs to come before SetIsDiscarded as it sends an event and the state
  // must be in sync.
  web_contents->SetWasDiscarded(true);

  // Discard all restored tabs as the activation is now done after the webview
  // has been attached.
  tab_lifecycle_unit_external->SetIsDiscarded();
}

void LazyLoadService::OnWillRestoreTab(
    content::WebContents* web_contents) {
  // Always allow lazyload/discard the webcontents. We must load it
  // after it has been attached in a webview .
  web_contents->SetUserData(&vivaldi::LazyLoadService::kLazyLoadIsSafe,
                            std::make_unique<base::SupportsUserData::Data>());
}

}  // namespace vivaldi
