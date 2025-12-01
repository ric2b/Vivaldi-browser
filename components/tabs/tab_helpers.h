// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_TABS_TAB_HELPERS_H_
#define COMPONENTS_TABS_TAB_HELPERS_H_

#include "base/values.h"

namespace content {
class WebContents;
}

namespace vivaldi {
inline constexpr char kVivaldiTabZoom[] = "vivaldi_tab_zoom";
inline constexpr char kVivaldiTabMuted[] = "vivaldi_tab_muted";
// Note. This flag is used in vivaldi_session_util.cc
// TODO: Get rid of this duplication.
inline constexpr char kVivaldiWorkspace[] = "workspaceId";

bool IsTabMuted(const content::WebContents* web_contents);
bool IsTabInAWorkspace(const content::WebContents* web_contents);
bool IsTabInAWorkspace(const std::string& viv_extdata);
std::optional<double> GetTabWorkspaceId(const std::string& viv_extdata);
bool SetTabWorkspaceId(content::WebContents* contents, double workspace_id);

}

#endif  // COMPONENTS_TABS_TAB_HELPERS_H_
