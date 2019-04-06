
// included by "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "content/public/browser/notification_source.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/notification_types.h"

#include "third_party/blink/public/platform/web_input_event.h"

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "content/browser/browser_plugin/browser_plugin_guest.h"

#include "content/public/browser/render_widget_host.h"

#include "components/content_settings/core/common/content_settings_types.h"

#ifdef USE_AURA
#include "base/timer/timer.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window.h"
#endif  // USE_AURA

#include "ui/devtools/devtools_connector.h"
#include "browser/vivaldi_browser_finder.h"
#include "browser/startup_vivaldi_browser.h"
