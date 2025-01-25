// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
namespace sync_sessions {

void SyncSessionsRouterTabHelper::VivExtDataSet(content::WebContents* contents) {
  NotifyRouter();
}

} // namespace sync_sessions
