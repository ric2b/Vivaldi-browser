// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

bool TabsEventRouter::TabEntry::SetDiscarded(bool new_val) {
  if (was_discarded_ == new_val)
    return false;
  was_discarded_ = new_val;
  return true;
}

void TabsEventRouter::TabEntry::VivExtDataSet(content::WebContents* contents) {
  router_->VivExtDataUpdated(contents);
}

void TabsEventRouter::VivExtDataUpdated(WebContents* contents) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(vivaldi::kExtDataKey);
  DispatchTabUpdatedEvent(contents, std::move(changed_property_names));
}

}  // namespace extensions
