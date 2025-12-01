// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PANEL_PANEL_ID_H_
#define COMPONENTS_PANEL_PANEL_ID_H_

#include <optional>
#include <string>

namespace vivaldi {
std::optional<std::string> ParseVivPanelId(const std::string& viv_ext_data);
void SanitizeExtDataBeforeRestore(std::string* viv_ext_data);
}  // namespace vivaldi

#endif  // COMPONENTS_PANEL_PANEL_ID_H_
