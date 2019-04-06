// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "app/vivaldi_apptools.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/common/content_settings_renderer.mojom.h"
#include "chrome/common/render_messages.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/helper/vivaldi_init_helpers.h"
#include "net/cert/x509_certificate.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/vivaldi_browser_window.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "content/public/browser/render_frame_host.h"

// Vivaldi constants
#include "extensions/api/guest_view/vivaldi_web_view_constants.h"

bool extensions::WebViewGuest::gesture_recording_;

using content::WebContents;
using guest_view::GuestViewEvent;
using guest_view::GuestViewManager;
using vivaldi::IsVivaldiApp;
using vivaldi::IsVivaldiRunning;

namespace extensions {

namespace {

void SetAllowRunningInsecureContent(content::RenderFrameHost* frame) {
  chrome::mojom::ContentSettingsRendererAssociatedPtr renderer;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&renderer);
  renderer->SetAllowRunningInsecureContent();
}

static std::string SSLStateToString(security_state::SecurityLevel status) {
  switch (status) {
    case security_state::NONE:
      // HTTP/no URL/user is editing
      return "none";
    case security_state::HTTP_SHOW_WARNING:
      // HTTP, show a visible warning about the page's lack of security
      return "http_show_warning";
    case security_state::EV_SECURE:
      // HTTPS with valid EV cert
      return "secure_with_ev";
    case security_state::SECURE:
      // HTTPS (non-EV)
      return "secure_no_ev";
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      // HTTPS, but the certificate verification chain is anchored on a
      // certificate that was installed by the system administrator
      return "security_policy_warning";
    case security_state::DANGEROUS:
      // Attempted HTTPS and failed, page not authenticated
      return "security_error";
    default:
      // fallthrough
      break;
  }
  NOTREACHED() << "Unknown SecurityLevel";
  return "unknown";
}

static std::string ContentSettingsTypeToString(
    ContentSettingsType content_type) {
  // Note there are more types, but these are the ones in
  // ContentSettingSimpleBubbleModel. Also note that some of these will be moved
  // elsewhere soon, based on comments in Chromium-code.
  switch (content_type) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return "cookies";
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return "images";
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return "javascript";
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return "plugins";
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return "popups";
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      return "location";
    case CONTENT_SETTINGS_TYPE_MIXEDSCRIPT:
      return "mixed-script";
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      return "register-protocol-handler";
    case CONTENT_SETTINGS_TYPE_PPAPI_BROKER:
      return "ppapi-broker";
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      return "multiple-automatic-downloads";
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      return "midi-sysex";
    default:
      // fallthrough
      break;
  }
  return "unknown";
}

}  // namespace

WebContents::CreateParams WebViewGuest::GetWebContentsCreateParams(
    content::BrowserContext* context,
    const GURL site) {
  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  auto* guest_view_manager = GuestViewManager::FromBrowserContext(context);
  scoped_refptr<content::SiteInstance> guest_site_instance =
      guest_view_manager ? guest_view_manager->GetGuestSiteInstance(site)
                         : nullptr;
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that webview tags are also not allowed to send messages across
    // different partitions.
    Profile* profile = Profile::FromBrowserContext(context);

    ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
    if (registry) {
      std::string extension_id = site.host();
      const Extension* extension =
          registry->enabled_extensions().GetByID(extension_id);

      if (extension && !IncognitoInfo::IsSplitMode(extension)) {
        // If it's not split-mode the host is associated with the original
        // profile.
        profile = profile->GetOriginalProfile();
        context = profile;
        guest_site_instance =
            ProcessManager::Get(profile)->GetSiteInstanceForURL(site);
      } else {
        guest_site_instance =
            content::SiteInstance::CreateForURL(context, site);
      }
    }
  }

  WebContents::CreateParams params(context, guest_site_instance);
  // As the tabstrip-content is not a guest we need to delete it and
  // re-create it as a guest then attach all the webcontents observers and
  // in addition replace the tabstrip content with the new guest content.
  params.guest_delegate = this;
  return params;
}

void WebViewGuest::ExtendedLoadProgressChanged(WebContents* source,
                                               double progress,
                                               double loaded_bytes,
                                               int loaded_elements,
                                               int total_elements) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guest_view::kUrl, web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  args->SetDouble(webview::kLoadedBytes, loaded_bytes);
  args->SetInteger(webview::kLoadedElements, loaded_elements);
  args->SetInteger(webview::kTotalElements, total_elements);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventLoadProgress, std::move(args))));
}

// Initialize listeners (cannot do it in constructor as RenderViewHost not
// ready.)
void WebViewGuest::InitListeners() {
  content::RenderViewHost* render_view_host =
      web_contents()->GetRenderViewHost();

  // If any inner web contents are in place we must be in a PDF
  // or similar and the inner contents must get the mouse events.
  std::vector<content::WebContentsImpl*> inner_contents =
      static_cast<content::WebContentsImpl*>(web_contents())
        ->GetInnerWebContents();
  if (inner_contents.size() > 0) {
    render_view_host = inner_contents[0]->GetRenderViewHost();
  }

  if (render_view_host && current_host_ != render_view_host) {
    // Add mouse event listener, only one for every new render_view_host
    mouseevent_callback_ =
        base::Bind(&WebViewGuest::OnMouseEvent, base::Unretained(this));
    render_view_host->GetWidget()->AddMouseEventCallback(mouseevent_callback_);

#if defined(OS_MACOSX) || defined(OS_LINUX)
    content::WebPreferences prefs = render_view_host->GetWebkitPreferences();
    prefs.context_menu_on_mouse_up = true;
    render_view_host->UpdateWebkitPreferences(prefs);
#endif
    current_host_ = render_view_host;
  }
}

void WebViewGuest::ToggleFullscreenModeForTab(
    content::WebContents* web_contents,
    bool enter_fullscreen,
    bool skip_window_state) {
  if (enter_fullscreen == is_fullscreen_)
    return;
  is_fullscreen_ = enter_fullscreen;

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean("enterFullscreen", enter_fullscreen);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventOnFullscreen, std::move(args))));

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

#if defined(USE_AURA)
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs();
  bool hide_cursor =
      pref_service->GetBoolean(vivaldiprefs::kWebpagesFullScreenHideMouse);
  if (hide_cursor && enter_fullscreen) {
    aura::Window* window =
        static_cast<aura::Window*>(web_contents->GetNativeView());
    cursor_hider_.reset(new CursorHider(window->GetRootWindow()));
  } else {
    cursor_hider_.reset(nullptr);
  }
#endif  // USE_AURA

  if (skip_window_state)
    return;

  VivaldiBrowserWindow* app_win =
      static_cast<VivaldiBrowserWindow*>(browser->window());
  if (!app_win)
    return;

  if (enter_fullscreen) {
    ui::WindowShowState current_window_state = app_win->GetRestoredState();
    window_state_prior_to_fullscreen_ = current_window_state;
    app_win->SetFullscreen(true);
  } else {
    switch (window_state_prior_to_fullscreen_) {
      case ui::SHOW_STATE_MAXIMIZED:
      case ui::SHOW_STATE_NORMAL:
      case ui::SHOW_STATE_DEFAULT:
        app_win->Restore();
        break;
      case ui::SHOW_STATE_FULLSCREEN:
        app_win->SetFullscreen(true);
        break;
      default:
        NOTREACHED() << "uncovered state";
        break;
    }
  }
}

void WebViewGuest::BeforeUnloadFired(content::WebContents* web_contents,
                                     bool proceed,
                                     bool* proceed_to_fire_unload) {
  // Call the Browser class as it already has an instance of the
  // active unload controller.
  Browser* browser = ::vivaldi::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  if (browser) {
    browser->DoBeforeUnloadFired(web_contents, proceed, proceed_to_fire_unload);
  }
}

bool WebViewGuest::IsVivaldiMail() {
  return name_.compare("vivaldi-mail") == 0;
}

bool WebViewGuest::IsVivaldiWebPanel() {
  return name_.compare("vivaldi-webpanel") == 0;
}

void WebViewGuest::SetVisible(bool is_visible) {
  is_visible_ = is_visible;
  content::RenderWidgetHostView* widgethostview =
      web_contents()->GetRenderWidgetHostView();
  if (widgethostview) {
    if (is_visible && !widgethostview->IsShowing()) {
      widgethostview->Show();
      // This is called from CoreTabHelper::WasShown, and must be called because
      // the activity must be updated so that the render-state is updated. This
      // will make sure that the memory usage is on-par with what Chrome use.
      // See VB-671 for more information and comments.
      web_cache::WebCacheManager::GetInstance()->ObserveActivity(
          web_contents()->GetMainFrame()->GetProcess()->GetID());
    }
    if (!is_visible && widgethostview->IsShowing()) {
      widgethostview->Hide();
    }
  }
}

bool WebViewGuest::IsVisible() {
  return is_visible_;
}

void WebViewGuest::ShowPageInfo(gfx::Point pos) {
  content::NavigationController& controller = web_contents()->GetController();
  const content::NavigationEntry* activeEntry = controller.GetActiveEntry();
  if (!activeEntry) {
    return;
  }

  const GURL url = controller.GetActiveEntry()->GetURL();
  auto security_info = security_state::GetVisibleSecurityState(web_contents());
  DCHECK(security_info);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Happens for WebContents not in a tabstrip.
  if (!browser) {
    browser = chrome::FindLastActiveWithProfile(profile);
  }

  if (browser->window()) {
    security_state::SecurityInfo security_state;
    security_state::GetSecurityInfo(std::move(security_info), false,
                                    base::Bind(&content::IsOriginSecure),
                                    &security_state);
    browser->window()->VivaldiShowWebsiteSettingsAt(profile, web_contents(),
                                                    url, security_state, pos);
  }
}

void WebViewGuest::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  // This class is the WebContentsDelegate, so forward this event
  // to the normal delegate here.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    static_cast<content::WebContentsDelegate*>(browser)->NavigationStateChanged(
        web_contents(), changed_flags);
  }
}

bool WebViewGuest::EmbedsFullscreenWidget() const {
  // If WebContents::GetFullscreenRenderWidgetHostView is present there is a
  // window other than this handling the fullscreen operation.
  return web_contents()->GetFullscreenRenderWidgetHostView() == NULL;
}

void WebViewGuest::SetIsFullscreen(bool is_fullscreen, bool skip_window_state) {
  SetFullscreenState(is_fullscreen);
  ToggleFullscreenModeForTab(web_contents(), is_fullscreen, skip_window_state);
}

void WebViewGuest::VisibleSecurityStateChanged(WebContents* source) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  auto security_info = security_state::GetVisibleSecurityState(web_contents());

  security_state::SecurityInfo security_state;
  security_state::GetSecurityInfo(std::move(security_info), false,
                                  base::Bind(&content::IsOriginSecure),
                                  &security_state);
  security_state::SecurityLevel current_level = security_state.security_level;
  args->SetString("SSLState", SSLStateToString(current_level));

  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (entry) {
    scoped_refptr<net::X509Certificate> cert(security_state.certificate);

    // EV are required to have an organization name and country.
    if (cert.get() && (!cert.get()->subject().organization_names.empty() &&
                       !cert.get()->subject().country_name.empty())) {
      args->SetString(
          "issuerstring",
          base::StringPrintf(
              "%s [%s]", cert.get()->subject().organization_names[0].c_str(),
              cert.get()->subject().country_name.c_str()));
    }
  }
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventSSLStateChanged, std::move(args))));
}

bool WebViewGuest::IsMouseGesturesEnabled() const {
  if (web_contents()) {
    PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
        ->GetPrefs();
    return pref_service->GetBoolean(vivaldiprefs::kMouseGesturesEnabled);
  }
  return true;
}

bool WebViewGuest::IsRockerGesturesEnabled() const {
  if (web_contents()) {
    PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
        ->GetPrefs();
    return pref_service->GetBoolean(vivaldiprefs::kMouseGesturesRockerGesturesEnabled);
  }
  return false;
}

bool WebViewGuest::OnMouseEvent(const blink::WebMouseEvent& mouse_event) {
  // Rocker gestures, a.la Opera
  if (IsRockerGesturesEnabled()) {
    if (has_left_mousebutton_down_ &&
        mouse_event.GetType() == blink::WebInputEvent::kMouseUp &&
        mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
      has_left_mousebutton_down_ = false;
    } else if (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
               mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
      has_left_mousebutton_down_ = true;
    }

    if (has_right_mousebutton_down_ &&
        mouse_event.GetType() == blink::WebInputEvent::kMouseUp &&
        mouse_event.button == blink::WebMouseEvent::Button::kRight) {
      has_right_mousebutton_down_ = false;
    } else if (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
               mouse_event.button == blink::WebMouseEvent::Button::kRight) {
      has_right_mousebutton_down_ = true;
    }

    if (mouse_event.button == blink::WebMouseEvent::Button::kNoButton) {
      has_right_mousebutton_down_ = false;
      has_left_mousebutton_down_ = false;
    }

    if (has_left_mousebutton_down_ &&
        (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
         mouse_event.button == blink::WebMouseEvent::Button::kRight)) {
      eat_next_right_mouseup_ = true;
      Go(1);
      return true;
    }

    if (has_right_mousebutton_down_ &&
        (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
         mouse_event.button == blink::WebMouseEvent::Button::kLeft)) {
      Go(-1);
      eat_next_right_mouseup_ = true;
      return true;
    }

    if (eat_next_right_mouseup_ &&
        mouse_event.GetType() == blink::WebInputEvent::kMouseUp &&
        mouse_event.button == blink::WebMouseEvent::Button::kRight) {
      eat_next_right_mouseup_ = false;
      return true;
    }
  }

  if (!gesture_recording_ &&
      (mouse_event.GetType() == blink::WebInputEvent::kMouseDown ||
       mouse_event.GetType() == blink::WebInputEvent::kMouseMove) &&
      mouse_event.button == blink::WebMouseEvent::Button::kRight &&
      !(mouse_event.GetModifiers() & blink::WebInputEvent::kLeftButtonDown) &&
      IsMouseGesturesEnabled()) {
    gesture_recording_ = true;
    mousedown_x_ = mouse_event.PositionInWidget().x;
    mousedown_y_ = mouse_event.PositionInWidget().y;
    ::vivaldi::SetBlockNextContextMenu(false);
  } else if (gesture_recording_ &&
             mouse_event.GetType() == blink::WebInputEvent::kMouseMove) {
    if (mouse_event.GetModifiers() & blink::WebInputEvent::kRightButtonDown) {
      if (!::vivaldi::GetBlockNextContextMenu()) {
        int dx = mouse_event.PositionInWidget().x - mousedown_x_;
        int dy = mouse_event.PositionInWidget().y - mousedown_y_;

        // If we went over the 5px threshold to cancel the context menu,
        // the flag is set. It persists if we go under the threshold by
        // moving the mouse into the original coords, which is expected.
        if (abs(dx) >= 5 || abs(dy) >= 5) {
          ::vivaldi::SetBlockNextContextMenu(true);
        }
      }
    } else {
      // Happens when right mouse button is released outside of webview.
      gesture_recording_ = false;
    }
  } else if (gesture_recording_ &&
             mouse_event.GetType() == blink::WebInputEvent::kMouseUp) {
    gesture_recording_ = false;
  }
  return false;
}

void WebViewGuest::UpdateTargetURL(content::WebContents* source,
                                   const GURL& url) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kNewURL, url.spec());
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventTargetURLChanged, std::move(args))));
}

void WebViewGuest::CreateSearch(const base::ListValue& search) {
  std::string keyword, url;

  if (!search.GetString(0, &keyword)) {
    NOTREACHED();
    return;
  }
  if (!search.GetString(1, &url)) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kNewSearchName, keyword);
  args->SetString(webview::kNewSearchUrl, url);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventCreateSearch, std::move(args))));
}

void WebViewGuest::PasteAndGo(const base::ListValue& search) {
  std::string clipBoardText;
  std::string pasteTarget;
  std::string modifiers;

  if (!search.GetString(0, &clipBoardText)) {
    NOTREACHED();
    return;
  }
  if (!search.GetString(1, &pasteTarget)) {
    NOTREACHED();
    return;
  }
  if (!search.GetString(2, &modifiers)) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kClipBoardText, clipBoardText);
  args->SetString(webview::kPasteTarget, pasteTarget);
  args->SetString(webview::kModifiers, modifiers);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventPasteAndGo, std::move(args))));
}

void WebViewGuest::SimpleAction(const base::ListValue& list) {
  std::string command;
  std::string text;
  std::string url;
  std::string modifiers;

  if (!list.GetString(0, &command)) {
    NOTREACHED();
    return;
  }
  if (!list.GetString(1, &text)) {
    NOTREACHED();
    return;
  }
  if (!list.GetString(2, &url)) {
    NOTREACHED();
    return;
  }
  if (!list.GetString(3, &modifiers)) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kGenCommand, command);
  args->SetString(webview::kGenText, text);
  args->SetString(webview::kGenUrl, url);
  args->SetString(webview::kModifiers, modifiers);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventSimpleAction, std::move(args))));
}

void WebViewGuest::ParseNewWindowUserInput(const std::string& user_input,
                                           int& window_id,
                                           bool& foreground,
                                           bool& incognito) {
  // user_input is a string. on the form nn;n;i
  // nn is windowId,
  // n is 1 or 0
  // 1 if tab should be opened in foreground
  // 0 if tab should be opened
  // i is "I" if this is an incognito (private) window
  // if normal window, don't add the "I"
  std::vector<std::string> lines;
  lines = base::SplitString(user_input, ";", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);
  DCHECK(lines.size());
  foreground = true;
  incognito = false;
  window_id = atoi(lines[0].c_str());
  if (lines.size() >= 2) {
    std::string strForeground(lines[1]);
    foreground = (strForeground == "1");
    if (lines.size() == 3) {
      std::string strIncognito(lines[2]);
      incognito = (strIncognito == "I");
    }
  }
}

void WebViewGuest::AddGuestToTabStripModel(WebViewGuest* guest,
                                           int windowId,
                                           bool activePage) {
  Browser* browser =
      chrome::FindBrowserWithID(SessionID::FromSerializedValue(windowId));

  if (!browser || !browser->window()) {
    if (windowId) {
      NOTREACHED();
      return;
    }
    // Find a suitable window.
    browser = chrome::FindTabbedBrowser(
        Profile::FromBrowserContext(guest->web_contents()->GetBrowserContext()),
        true);
    if (!browser || !browser->window()) {
      NOTREACHED();
      return;
    }
  }

  TabStripModel* tab_strip = browser->tab_strip_model();

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = activePage;
  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = false;
  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = -1;
  index = std::min(std::max(index, -1), tab_strip->count());

  int add_types = active ? TabStripModel::ADD_ACTIVE : TabStripModel::ADD_NONE;
  add_types |=
      TabStripModel::ADD_FORCE_INDEX | TabStripModel::ADD_INHERIT_OPENER;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;

  NavigateParams navigate_params(browser,
      std::unique_ptr<WebContents>(guest->web_contents()));
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  navigate_params.source_contents = web_contents();
  Navigate(&navigate_params);

  if (!browser->is_vivaldi()) {
    if (active)
      navigate_params.navigated_or_inserted_contents->SetInitialFocus();
  }
  if (navigate_params.navigated_or_inserted_contents) {
    content::RenderFrameHost* host =
        navigate_params.navigated_or_inserted_contents->GetMainFrame();
    DCHECK(host);
    chrome::mojom::ChromeRenderFrameAssociatedPtr client;
    host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWindowFeatures(blink::mojom::WindowFeatures().Clone());
  }
}

blink::WebSecurityStyle WebViewGuest::GetSecurityStyle(
    WebContents* contents,
    content::SecurityStyleExplanations* security_style_explanations) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser) {
    return browser->GetSecurityStyle(contents, security_style_explanations);
  } else {
    return blink::kWebSecurityStyleUnknown;
  }
}

void WebViewGuest::OnContentBlocked(ContentSettingsType settings_type,
                                    const base::string16& details) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());

  args->SetString("blockedType", ContentSettingsTypeToString(settings_type));

  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventContentBlocked, std::move(args))));
}


void WebViewGuest::AllowRunningInsecureContent() {

  MixedContentSettingsTabHelper* mixed_content_settings =
    MixedContentSettingsTabHelper::FromWebContents(web_contents());
  if (mixed_content_settings) {
    // Update browser side settings to allow active mixed content.
    mixed_content_settings->AllowRunningOfInsecureContent();
  }

  web_contents()->ForEachFrame(base::Bind(&SetAllowRunningInsecureContent));
}

bool WebViewGuest::ShouldAllowRunningInsecureContent(
  WebContents* web_contents,
  bool allowed_per_prefs,
  const url::Origin& origin,
  const GURL& resource_url) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    return browser->ShouldAllowRunningInsecureContent(
      web_contents, allowed_per_prefs, origin, resource_url);
  } else {
    return false;
  }
}

void WebViewGuest::OnMouseEnter() {
#if defined(USE_AURA)
  // Reset the timer so that the hiding sequence starts over.
  if (cursor_hider_.get()) {
    cursor_hider_.get()->Reset();
  }
#endif  // USE_AURA
}

void WebViewGuest::OnMouseLeave() {
#if defined(USE_AURA)
  // Stop hiding the mouse cursor if the mouse leaves the view.
  if (cursor_hider_.get()) {
    cursor_hider_.get()->Stop();
  }
#endif  // USE_AURA
}

void WebViewGuest::ShowRepostFormWarningDialog(WebContents* source) {
  TabModalConfirmDialog::Create(new RepostFormWarningController(source),
                                source);
}

bool WebViewGuest::HasOwnerShipOfContents() {
  if (web_contents_is_owned_by_this_ == false) {
    return false;
  }
  return !chrome::FindBrowserWithWebContents(web_contents());
}

void WebViewGuest::LoadTabContentsIfNecessary() {
  web_contents()->GetController().LoadIfNecessary();

  TabStripModel* tab_strip;
  int tab_index;
  ::vivaldi::VivaldiStartupTabUserData* viv_startup_data =
      static_cast<::vivaldi::VivaldiStartupTabUserData*>(
          web_contents()->GetUserData(
              ::vivaldi::kVivaldiStartupTabUserDataKey));

  if (viv_startup_data && ExtensionTabUtil::GetTabStripModel(
                              web_contents(), &tab_strip, &tab_index)) {
    // Check if we need to make a tab active, this must be done when
    // starting with tabs through the commandline or through start with pages.
    if (viv_startup_data && viv_startup_data->start_as_active()) {
      tab_strip->ActivateTabAt(tab_index, false);
    }
  }
  web_contents()->SetUserData(::vivaldi::kVivaldiStartupTabUserDataKey,
                              nullptr);
}

}  // namespace extensions

////////////////////////////////////////////////////////////////////////////////
// Bridge helpers to allow usage of component code in the browser.
////////////////////////////////////////////////////////////////////////////////
namespace guest_view {

// Vivaldi helper function that is declared in
// src/components/guest_view/browser/guest_view_base.h.
// Returns true if a Browser object owns and manage the lifecycle of the
// |content::WebContents|
bool HandOverToBrowser(content::WebContents* contents) {
  // gisli@vivaldi.com:  In case of Vivaldi view the view is not
  // an owner of the web contents (the browser object owns it).
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser) {
    // Hand the web contents over to the browser.
    contents->SetDelegate(browser);
  }
  return !!browser;
}

// declared in src/components/guest_view/browser/guest_view_base.h
void AttachWebContentsObservers(content::WebContents* contents) {
  if (vivaldi::IsVivaldiRunning()) {
    extensions::WebNavigationTabObserver::CreateForWebContents(contents);
    vivaldi::InitHelpers(contents);
  }
}
}  // namespace guest_view
