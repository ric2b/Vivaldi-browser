// Copyright (c) 2018 Vivaldi Technologies

#include "components/sessions/core/tab_restore_service_client.h"

namespace sessions {

LiveTabContext* TabRestoreServiceClient::CreateLiveTabContext(
    LiveTabContext* existing_context,
    SessionWindow::WindowType type,
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state,
    const std::string& workspace,
    const std::string& user_title,
    const std::map<std::string, std::string>& extra_data) {
  const std::string dummy_ext_data;
  return CreateLiveTabContext(existing_context, type, app_name, bounds, show_state,
                              workspace, user_title, extra_data,
                              dummy_ext_data);
}

LiveTabContext* TabRestoreServiceClient::CreateLiveTabContext(
    LiveTabContext* existing_context,
    SessionWindow::WindowType type,
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state,
    const std::string& workspace,
    const std::string& user_title,
    const std::map<std::string, std::string>& extra_data,
    const std::string& viv_ext_data) {
  return CreateLiveTabContext(existing_context, type, app_name, bounds, show_state,
                              workspace, user_title, extra_data);
}

const std::map<base::FilePath, bool>*
TabRestoreServiceClient::GetPageActionOverridesForTab(LiveTab* tab) {
  return nullptr;
}

}  // namespace sessions
