// Copyright (c) 2016 Vivaldi Technologies

#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/live_tab_context.h"

namespace sessions {

std::string LiveTabContext::GetExtData() const {
  return std::string();
}

LiveTab *LiveTabContext::AddRestoredTab(
    const std::vector<SerializedNavigationEntry> &navigations, int tab_index,
    int selected_navigation, const std::string &extension_app_id, bool select,
    bool pin, bool from_last_session,
    const PlatformSpecificTabData *tab_platform_data,
    const std::string &user_agent_override) {
  const std::string dummy_ext_data;
  return AddRestoredTab(navigations, tab_index, selected_navigation,
                        extension_app_id, select, pin, from_last_session,
                        tab_platform_data, user_agent_override, dummy_ext_data);
}

LiveTab *LiveTabContext::AddRestoredTab(
    const std::vector<SerializedNavigationEntry> &navigations, int tab_index,
    int selected_navigation, const std::string &extension_app_id, bool select,
    bool pin, bool from_last_session,
    const PlatformSpecificTabData *tab_platform_data,
    const std::string &user_agent_override, const std::string &ext_data) {
  return AddRestoredTab(navigations, tab_index, selected_navigation,
    extension_app_id, select, pin, from_last_session,
    tab_platform_data, user_agent_override);
}

LiveTab *LiveTabContext::ReplaceRestoredTab(
    const std::vector<SerializedNavigationEntry> &navigations,
    int selected_navigation, bool from_last_session,
    const std::string &extension_app_id,
    const PlatformSpecificTabData *tab_platform_data,
    const std::string &user_agent_override) {
  const std::string dummy_ext_data;
  return ReplaceRestoredTab(navigations, selected_navigation, from_last_session,
                            extension_app_id, tab_platform_data,
                            user_agent_override, dummy_ext_data);
}

LiveTab *LiveTabContext::ReplaceRestoredTab(
    const std::vector<SerializedNavigationEntry> &navigations,
    int selected_navigation, bool from_last_session,
    const std::string &extension_app_id,
    const PlatformSpecificTabData *tab_platform_data,
    const std::string &user_agent_override, const std::string &ext_data) {
  return ReplaceRestoredTab(navigations, selected_navigation, from_last_session,
                            extension_app_id, tab_platform_data,
                            user_agent_override);
}

const std::string& ContentLiveTab::GetExtData() const {
  return web_contents()->GetExtData();
}

}  // namespace sessions
