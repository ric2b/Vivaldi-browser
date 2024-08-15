#include "components/extensions/vivaldi_panel_utils.h"
#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

#include "app/vivaldi_apptools.h"
#include "components/panel/panel_id.h"

namespace vivaldi {

std::optional<std::string> GetVivPanelId(const content::WebContents* contents) {
  if (!contents) {
    return std::optional<std::string>();
  }
  return ::vivaldi::ParseVivPanelId(contents->GetVivExtData());
}

bool CanTouchTheTab(const extensions::Extension* extension,
                    const content::WebContents* contents) {
  if (!extension || !contents)
    return true;

  if (::vivaldi::IsVivaldiApp(extension->id()))
    return true;

  auto panel_id = ::vivaldi::GetVivPanelId(contents);
  if (panel_id)
    return false;

  return true;
}

extensions::Event::VivFilter SuggestFiltering(content::WebContents* contents) {
  DCHECK(contents);
  if (!contents)
    return extensions::Event::NO_FILTERING;
  if (::vivaldi::GetVivPanelId(contents)) {
    return extensions::Event::VIVALDI_ONLY;
  }
  return extensions::Event::NO_FILTERING;
}
}  // namespace vivaldi
