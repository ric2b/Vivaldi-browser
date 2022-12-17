// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

#include "content/public/browser/web_contents.h"

std::string TabContentsSyncedTabDelegate::GetVivExtData() const {
  return web_contents_->GetVivExtData();
}
