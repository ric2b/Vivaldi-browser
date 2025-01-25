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
    const tab_restore::Tab& tab,
    int tab_index,
    bool select,
    tab_restore::Type original_session_type) {
  const std::map<std::string, bool> dummy_page_action_overrides;
  const std::string dummy_ext_data;
  return AddRestoredTab(tab, tab_index, select, original_session_type,
                        dummy_page_action_overrides, dummy_ext_data);
}

LiveTab* LiveTabContext::AddRestoredTab(
    const tab_restore::Tab& tab,
    int tab_index,
    bool select,
    tab_restore::Type original_session_type,
    const std::map<std::string, bool> viv_page_action_overrides,
    const std::string& viv_ext_data) {
  return AddRestoredTab(tab, tab_index, select, original_session_type);
}

LiveTab* LiveTabContext::ReplaceRestoredTab(const tab_restore::Tab& tab) {
  const std::map<std::string, bool> dummy_page_action_overrides;
  const std::string dummy_ext_data;
  return ReplaceRestoredTab(tab, dummy_page_action_overrides, dummy_ext_data);
}

LiveTab* LiveTabContext::ReplaceRestoredTab(
    const tab_restore::Tab& tab,
    const std::map<std::string, bool> viv_page_action_overrides,
    const std::string& viv_ext_data) {
  return ReplaceRestoredTab(tab);
}

#if !BUILDFLAG(IS_IOS)
const std::string& ContentLiveTab::GetVivExtData() const {
  return web_contents()->GetVivExtData();
}
#endif

}  // namespace sessions
