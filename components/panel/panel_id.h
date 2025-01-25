#ifndef COMPONENTS_PANEL_PANEL_ID_H_
#define COMPONENTS_PANEL_PANEL_ID_H_

#include <string>
#include <optional>

namespace vivaldi {
  std::optional<std::string>
  ParseVivPanelId(const std::optional<std::string>& viv_ext_data);
  void SanitizeExtDataBeforeRestore(std::string *viv_ext_data);
}

#endif // COMPONENTS_PANEL_PANEL_ID_H_
