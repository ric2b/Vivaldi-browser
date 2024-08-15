#include "extensions/renderer/extension_frame_helper.h"

namespace extensions {

void ExtensionFrameHelper::SetVivaldiPanelId(int32_t vivaldi_panel_id) {
  vivaldi_panel_ = true;
  tab_id_ = vivaldi_panel_id;
}

}  // namespace extensions
