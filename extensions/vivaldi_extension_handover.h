// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved.

#include <string>

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace vivaldi {

// Fwd for type safety.
class VivaldiExtensionHandoverImpl;

// Indirects extension events from chromium to our extension code, so that API
// is not needed to be linked. Instanced via a runtime Impl.
class VivaldiExtensionHandover {
 public:
  virtual ~VivaldiExtensionHandover() = default;

  static VivaldiExtensionHandover* GetInstance();

  virtual void ExtensionActionUtil_SendIconLoaded(
      content::BrowserContext* browser_context,
      const std::string& extension_id,
      const gfx::Image& image) = 0;

 protected:
  friend class VivaldiExtensionHandoverImpl;

  static void SetInstance(VivaldiExtensionHandover* wrapper);
};

inline void NotifyExtensionIconLoaded(content::BrowserContext* browser_context,
                                      const std::string& extension_id,
                                      const gfx::Image& image)

{
  VivaldiExtensionHandover::GetInstance()->ExtensionActionUtil_SendIconLoaded(
      browser_context, extension_id, image);
}

}  // namespace vivaldi
