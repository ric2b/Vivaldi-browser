
// included by "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "content/public/browser/notification_source.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/notification_types.h"

#include "chrome/browser/ui/validation_message_bubble.h"

#include "third_party/WebKit/public/platform/WebInputEvent.h"

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "content/public/browser/render_widget_host.h"

#include "components/content_settings/core/common/content_settings_types.h"

#ifdef USE_AURA
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window.h"
#include "base/timer/timer.h"
#endif // USE_AURA


namespace extensions {

class ExtensionHostForWebContents : public ExtensionHost {
  public:
  ExtensionHostForWebContents(const Extension *extension,
                              content::SiteInstance *site_instance,
                              const GURL &url, ViewType host_type,
                              content::WebContents *host_contents);

  ~ExtensionHostForWebContents() override;

};

} //extensions
