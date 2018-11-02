// Copyright (c) 2018 Vivaldi Technologies

#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"

std::string TabContentsSyncedTabDelegate::GetExtData() const {
  return web_contents_->GetExtData();
}
