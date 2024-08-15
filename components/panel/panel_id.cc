#include "components/panel/panel_id.h"
#include "base/json/json_reader.h"

namespace vivaldi {

std::optional<std::string> ParseVivPanelId(
    const std::optional<std::string>& viv_ext_data) {
  if (!viv_ext_data) {
    return std::optional<std::string>();
  }

  std::optional<base::Value> json =
      base::JSONReader::Read(*viv_ext_data, base::JSON_PARSE_RFC);

  if (!json) {
    return std::optional<std::string>();
  }

  if (!json->is_dict()) {
    return std::optional<std::string>();
  }

  const std::string* panel_id = json->GetDict().FindString("panelId");

  if (!panel_id) {
    return std::optional<std::string>();
  }

  return *panel_id;
}

}
