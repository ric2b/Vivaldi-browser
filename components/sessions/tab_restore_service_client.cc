// Copyright (c) 2018 Vivaldi Technologies

#include "components/sessions/core/tab_restore_service_client.h"

namespace sessions {

LiveTabContext* TabRestoreServiceClient::CreateLiveTabContext(
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state,
    const std::string& workspace,
    const std::string& user_title) {
  const std::string dummy_ext_data;
  return CreateLiveTabContext(app_name, bounds, show_state, workspace,
                              user_title, dummy_ext_data);
}

LiveTabContext* TabRestoreServiceClient::CreateLiveTabContext(
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state,
    const std::string& workspace,
    const std::string& user_title,
    const std::string& ext_data) {
  return CreateLiveTabContext(app_name, bounds, show_state, workspace,
                              user_title);
}

}  // namespace sessions
