#include "extensions/vivaldi_extension_handover.h"

namespace vivaldi {

/// Implements an opaque extension notification interface - separated impl to
/// avoid linux debug linking issues.
class VivaldiExtensionHandoverImpl : public VivaldiExtensionHandover {
 public:
  static void CreateImpl();

  void ExtensionActionUtil_SendIconLoaded(
      content::BrowserContext* browser_context,
      const std::string& extension_id,
      const gfx::Image& image) override;
};

}  // namespace vivaldi