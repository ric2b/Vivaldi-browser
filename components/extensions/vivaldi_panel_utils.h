#ifndef COMPONENTS_EXTENSIONS_VIVALDI_PANEL_UTILS_H_
#define COMPONENTS_EXTENSIONS_VIVALDI_PANEL_UTILS_H_

#include <optional>
#include <string>

#include "extensions/browser/event_router.h"

namespace extensions {
class Extension;
}

namespace content {
class WebContents;
}

namespace vivaldi {

std::optional<std::string> GetVivPanelId(const content::WebContents* contents);

bool CanTouchTheTab(const extensions::Extension* extension,
                    const content::WebContents* contents);

extensions::Event::VivFilter SuggestFiltering(content::WebContents* contents);
}  // namespace vivaldi
#endif // COMPONENTS_EXTENSIONS_VIVALDI_PANEL_UTILS_H_
