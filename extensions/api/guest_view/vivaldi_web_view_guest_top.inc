
// included by "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "content/public/browser/notification_source.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/notification_types.h"

#include "chrome/browser/ui/validation_message_bubble.h"

#include "third_party/WebKit/public/web/WebInputEvent.h"

#include "chrome/browser/ui/tabs/tab_utils.h"

extern extensions::WebViewGuest* s_current_webviewguest;


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
