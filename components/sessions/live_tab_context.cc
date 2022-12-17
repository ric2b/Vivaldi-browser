// Copyright (c) 2016 Vivaldi Technologies

#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/live_tab_context.h"

#if !BUILDFLAG(IS_IOS)
#include "components/sessions/content/content_live_tab.h"
#endif

namespace sessions {

const std::string &LiveTab::GetVivExtData() const {
  static std::string dummy;
  return dummy;
}

std::string LiveTabContext::GetVivExtData() const {
  return std::string();
}

LiveTab* LiveTabContext::AddRestoredTab(
    const std::vector<SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    absl::optional<tab_groups::TabGroupId> group,
    const tab_groups::TabGroupVisualData& group_visual_data,
    bool select,
    bool pin,
    const PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    const SessionID* tab_id) {
  const std::map<std::string, bool> dummy_page_action_overrides;
  const std::string dummy_ext_data;
  return AddRestoredTab(navigations, tab_index, selected_navigation,
                        extension_app_id, group, group_visual_data, select, pin,
                        tab_platform_data, user_agent_override, extra_data, tab_id,
                        dummy_page_action_overrides, dummy_ext_data);
}

LiveTab* LiveTabContext::AddRestoredTab(
    const std::vector<SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    absl::optional<tab_groups::TabGroupId> group,
    const tab_groups::TabGroupVisualData& group_visual_data,
    bool select,
    bool pin,
    const PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    const SessionID* tab_id,
    const std::map<std::string, bool> viv_page_action_overrides,
    const std::string& viv_ext_data) {
  return AddRestoredTab(navigations, tab_index, selected_navigation,
                        extension_app_id, group, group_visual_data, select, pin,
                        tab_platform_data, user_agent_override, extra_data, tab_id);
}

LiveTab* LiveTabContext::ReplaceRestoredTab(
    const std::vector<SerializedNavigationEntry>& navigations,
    absl::optional<tab_groups::TabGroupId> group,
    int selected_navigation,
    const std::string& extension_app_id,
    const PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data) {
  const std::map<std::string, bool> dummy_page_action_overrides;
  const std::string dummy_ext_data;
  return ReplaceRestoredTab(navigations, group, selected_navigation,
                            extension_app_id, tab_platform_data,
                            user_agent_override, extra_data,
                            dummy_page_action_overrides, dummy_ext_data);
}

LiveTab* LiveTabContext::ReplaceRestoredTab(
    const std::vector<SerializedNavigationEntry>& navigations,
    absl::optional<tab_groups::TabGroupId> group,
    int selected_navigation,
    const std::string& extension_app_id,
    const PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    const std::map<std::string, bool> viv_page_action_overrides,
    const std::string& viv_ext_data) {
  return ReplaceRestoredTab(navigations, group, selected_navigation,
                            extension_app_id, tab_platform_data,
                            user_agent_override, extra_data);
}

#if !BUILDFLAG(IS_IOS)
const std::string& ContentLiveTab::GetVivExtData() const {
  return web_contents()->GetVivExtData();
}
#endif

}  // namespace sessions
