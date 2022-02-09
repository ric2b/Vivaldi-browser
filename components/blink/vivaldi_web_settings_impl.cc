// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "third_party/blink/renderer/core/exported/web_settings_impl.h"
#include "third_party/blink/renderer/core/frame/settings.h"

namespace blink {

void WebSettingsImpl::SetServeResourceFromCacheOnly(bool only_load_from_cache) {
  settings_->SetServeResourceFromCacheOnly(only_load_from_cache);
}

void WebSettingsImpl::SetAllowAccessKeys(bool allow_access_keys) {
  settings_->SetAllowAccessKeys(allow_access_keys);
}

bool WebSettingsImpl::AllowAccessKeys() const {
  return settings_->GetAllowAccessKeys();
}

void WebSettingsImpl::SetAllowTabCycleIntoUI(
    bool allow_tab_cycle_from_webpage_into_ui) {
  settings_->SetAllowTabCycleIntoUI(allow_tab_cycle_from_webpage_into_ui);
}

void WebSettingsImpl::SetVivaldiShowContextMenuOnDoubleClick(bool value) {
  settings_->SetVivaldiShowContextMenuOnDoubleClick(value);
}

}  // namespace blink
