// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// CPPLint won't let us put this above the C++ stdlib headers, because the path
// doesn't match the cc file. As long as this compiles fine, it's probably
// better than the alternative, which is to use NOLINT on all
// the C++ headers.
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/common/insecure_content_renderer.mojom.h"
#include "chrome/common/render_messages.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/helper/vivaldi_init_helpers.h"
#include "net/cert/x509_certificate.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "prefs/vivaldi_pref_names.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if defined(OS_LINUX) || defined(OS_MACOSX)
// Vivaldi addition: only define mouse gesture for OSX & linux
// because context menu is shown on mouse down for those systems
#define MOUSE_GESTURES
#endif

using content::WebContents;
using guest_view::GuestViewEvent;
using guest_view::GuestViewManager;
using vivaldi::IsVivaldiApp;
using vivaldi::IsVivaldiRunning;

namespace extensions {

ExtensionHostForWebContents::ExtensionHostForWebContents(
    const Extension* extension,
    content::SiteInstance* site_instance,
    const GURL& url,
    ViewType host_type,
    content::WebContents* contents)
    : ExtensionHost(extension, site_instance, url, host_type) {
  SetHostContentsAndRenderView(contents);
  content::WebContentsObserver::Observe(host_contents());
  SetViewType(host_contents(), host_type);
}

ExtensionHostForWebContents::~ExtensionHostForWebContents() {
  // We must release host_contents here since it is not owned by us.
  ExtensionHost::ReleaseHostContents();
}

namespace {

void SetAllowRunningInsecureContent(content::RenderFrameHost* frame) {
  chrome::mojom::InsecureContentRendererPtr renderer;
  frame->GetRemoteInterfaces()->GetInterface(&renderer);
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
    case security_state::SECURITY_WARNING:
      // HTTPS, but unable to check certificate revocation status or with
      // insecure content on the page
      return "security_warning";
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

static std::string TabAlertStateToString(TabAlertState status) {
  switch (status) {
    case TabAlertState::NONE:
      return "none";
    case TabAlertState::MEDIA_RECORDING:
      return "recording";
    case TabAlertState::TAB_CAPTURING:
      return "capturing";
    case TabAlertState::AUDIO_PLAYING:
      return "playing";
    case TabAlertState::AUDIO_MUTING:
      return "muting";
    case TabAlertState::BLUETOOTH_CONNECTED:
      return "bluetooth";
    case TabAlertState::USB_CONNECTED:
      return "usb";
  }
  NOTREACHED() << "Unknown TabAlertState Status.";
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
    guest_site_instance = content::SiteInstance::CreateForURL(context, site);
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

  if (render_view_host && current_host_ != render_view_host) {
    // Add mouse event listener, only one for every new render_view_host
    mouseevent_callback_ =
        base::Bind(&WebViewGuest::OnMouseEvent, base::Unretained(this));
    render_view_host->GetWidget()->AddMouseEventCallback(mouseevent_callback_);
    current_host_ = render_view_host;
  }
}

void WebViewGuest::ToggleFullscreenModeForTab(
    content::WebContents* web_contents,
    bool enter_fullscreen) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean("enterFullscreen", enter_fullscreen);

  bool state_changed = enter_fullscreen != is_fullscreen_;
  is_fullscreen_ = enter_fullscreen;

  extensions::AppWindow* app_win = GetAppWindow();
  if (app_win) {
    extensions::NativeAppWindow* native_app_window = app_win->GetBaseWindow();
    ui::WindowShowState current_window_state =
        native_app_window->GetRestoredState();
#if defined(USE_AURA)
    PrefService* pref_service =
        Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->GetPrefs();
    bool hide_cursor =
        pref_service->GetBoolean(vivaldiprefs::kHideMouseCursorInFullscreen);
    if (hide_cursor && enter_fullscreen) {
      aura::Window* window =
          static_cast<aura::Window*>(web_contents->GetNativeView());
      cursor_hider_.reset(new CursorHider(window->GetRootWindow()));
    } else {
      cursor_hider_.reset(nullptr);
    }
#endif  // USE_AURA

    if (enter_fullscreen) {
      window_state_prior_to_fullscreen_ = current_window_state;
      app_win->Fullscreen();
    } else {
      switch (window_state_prior_to_fullscreen_) {
        case ui::SHOW_STATE_MAXIMIZED:
        case ui::SHOW_STATE_NORMAL:
        case ui::SHOW_STATE_DEFAULT:
          // If state did not change we had a plugin that came out of
          // fullscreen. Only HTML-element fullscreen changes the appwindow
          // state.
          if (state_changed) {
            app_win->Restore();
          }
          break;
        case ui::SHOW_STATE_FULLSCREEN:
          app_win->Fullscreen();
          break;
        default:
          NOTREACHED() << "uncovered state";
          break;
      }
    }
  }
  if (state_changed) {
    DispatchEventToView(base::WrapUnique(
        new GuestViewEvent(webview::kEventOnFullscreen, std::move(args))));
  }
}

void WebViewGuest::ShowValidationMessage(content::WebContents* web_contents,
                                         const gfx::Rect& anchor_in_root_view,
                                         const base::string16& main_text,
                                         const base::string16& sub_text) {
  validation_message_bubble_ =
      TabDialogs::FromWebContents(web_contents)
          ->ShowValidationMessage(anchor_in_root_view, main_text, sub_text);
}

void WebViewGuest::HideValidationMessage(content::WebContents* web_contents) {
  if (validation_message_bubble_)
    validation_message_bubble_->CloseValidationMessage();
}

void WebViewGuest::MoveValidationMessage(content::WebContents* web_contents,
                                         const gfx::Rect& anchor_in_root_view) {
  if (!validation_message_bubble_)
    return;
  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    validation_message_bubble_->SetPositionRelativeToAnchor(
        rwhv->GetRenderWidgetHost(), anchor_in_root_view);
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
          web_contents()->GetRenderProcessHost()->GetID());
    }
    if (!is_visible && widgethostview->IsShowing()) {
      widgethostview->Hide();
    }
  }
}

bool WebViewGuest::IsVisible() {
  return is_visible_;
}

extensions::AppWindow* WebViewGuest::GetAppWindow() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    AppWindowRegistry* app_registry =
        AppWindowRegistry::Get(browser->profile());
    const AppWindowRegistry::AppWindowList& app_windows =
        app_registry->app_windows();

    AppWindowRegistry::const_iterator iter = app_windows.begin();
    while (iter != app_windows.end()) {
      if ((*iter)->web_contents() == owner_web_contents()) {
        return (*iter);
      }
      iter++;
    }
  }
  return nullptr;
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

void WebViewGuest::UpdateMediaState(TabAlertState state) {
  if (state != media_state_) {
    std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());

    args->SetString("activeMediaType", TabAlertStateToString(state));

    DispatchEventToView(base::WrapUnique(
        new GuestViewEvent(webview::kEventMediaStateChanged, std::move(args))));
  }
  media_state_ = state;
}

void WebViewGuest::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  UpdateMediaState(chrome::GetTabAlertStateForContents(web_contents()));

  // TODO(gisli):  This would normally be done in the browser, but until we get
  // Vivaldi browser object we do it here (as we did remove the webcontents
  // listener for the current browser).
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

void WebViewGuest::SetIsFullscreen(bool is_fullscreen) {
  is_fullscreen_ = is_fullscreen;
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
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  return pref_service->GetBoolean(vivaldiprefs::kMousegesturesEnabled);
}

bool WebViewGuest::IsRockerGesturesEnabled() const {
  PrefService* pref_service =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext())
          ->GetPrefs();
  return pref_service->GetBoolean(vivaldiprefs::kRockerGesturesEnabled);
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
#ifdef  MOUSE_GESTURES
      // Allows the sequence LMB-DOWN - RMB-DOWN - RMB-UP - LMB-UP (single
      // back nav) to be repeatable without a menu popping up breaking it
      // when LMB-DOWN is activated to start another sequence.
      gesture_recording_ = false;
#endif
      return true;
    }
  }

#ifdef  MOUSE_GESTURES
  // Both mouse gestures and rocker gestures need a delayed menu (on mouse up)
  // to work propely.
  if (!IsMouseGesturesEnabled() && !IsRockerGesturesEnabled()) {
    return false;
  }

  // Record the gesture
  if (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
      mouse_event.button == blink::WebMouseEvent::Button::kRight &&
      !(mouse_event.GetModifiers() & blink::WebInputEvent::kLeftButtonDown) &&
      !gesture_recording_) {
    gesture_recording_ = true;
    mousedown_x_ = mouse_event.PositionInWidget().x;
    mousedown_y_ = mouse_event.PositionInWidget().y;
    fire_context_menu_ = true;
    return true;
  } else if (gesture_recording_ &&
             (mouse_event.GetType() == blink::WebInputEvent::kMouseMove ||
              mouse_event.GetType() == blink::WebInputEvent::kMouseUp)) {
    int dx = mouse_event.PositionInWidget().x - mousedown_x_;
    int dy = mouse_event.PositionInWidget().y - mousedown_y_;

    // If we went over the 5px threshold to cancel the context menu,
    // the flag is set. It persists if we go under the threshold by
    // moving you mouse into the original coords, which is expected.
    if (abs(dx) >= 5 || abs(dy) >= 5) {
      fire_context_menu_ = false;
    }

    // Copy event and fire
    if (mouse_event.GetType() == blink::WebInputEvent::kMouseUp &&
        mouse_event.button == blink::WebMouseEvent::Button::kRight) {
      blink::WebMouseEvent event_copy(mouse_event);
      content::RenderViewHost* render_view_host =
          web_contents()->GetRenderViewHost();

      if (fire_context_menu_) {
        // Send the originally-culled right mouse down at original coords
        event_copy.SetType(blink::WebInputEvent::kMouseDown);

        int screenx = event_copy.PositionInScreen().x;
        screenx -= (mouse_event.PositionInWidget().x - mousedown_x_);

        int screeny = event_copy.PositionInScreen().y;
        screeny -= (mouse_event.PositionInWidget().y - mousedown_y_);

        event_copy.SetPositionInScreen(screenx, screeny);

        event_copy.SetPositionInWidget(mousedown_x_, mousedown_y_);

        render_view_host->GetWidget()->ForwardMouseEvent(event_copy);

        // Mac will not reset rocker gestures when a context menu closes
        // (like what happens on lin and win that receive a mouse event with
        // blink::WebMouseEvent::Button::kNoButton set). So, reset flags here.
        has_right_mousebutton_down_ = false;
        has_left_mousebutton_down_ = false;
      }

      gesture_recording_ = false;
      return fire_context_menu_;
    } else if (mouse_event.GetType() == blink::WebInputEvent::kMouseDown &&
               mouse_event.button == blink::WebMouseEvent::Button::kLeft) {
      fire_context_menu_ = true;
      gesture_recording_ = false;
    }

    if ((mouse_event.GetModifiers() & blink::WebInputEvent::kRightButtonDown) ==
        0) {
      gesture_recording_ = false;
    }
  }
#endif  // MOUSE_GESTURES
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
  Browser* browser = chrome::FindBrowserWithID(windowId);

  if (!browser || !browser->window()) {
    // TODO(gisli@vivaldi.com): Error message?
    return;
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
  chrome::NavigateParams navigate_params(browser, guest->web_contents());
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  navigate_params.source_contents = web_contents();
  chrome::Navigate(&navigate_params);

  if (active)
    navigate_params.target_contents->SetInitialFocus();

  if (navigate_params.target_contents) {
    navigate_params.target_contents->Send(new ChromeViewMsg_SetWindowFeatures(
        navigate_params.target_contents->GetRenderViewHost()->GetRoutingID(),
        blink::mojom::WindowFeatures()));
  }
}

bool WebViewGuest::RequestPageInfo(const GURL& url) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->Set(webview::kTargetURL, new base::Value(url.spec()));
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventRequestPageInfo, std::move(args))));
  return true;
}

void WebViewGuest::CreateExtensionHost(const std::string& extension_id) {
  if (!IsVivaldiApp(owner_host())) {
    // Only allow creation of ExtensionHost if the hosting app is Vivaldi.
    return;
  }

  if (extension_id.empty()) {
    notification_registrar_.Remove(
        this, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
        content::Source<content::BrowserContext>(browser_context()));
    extension_host_.reset(nullptr);
  } else {
    if (extension_host_.get()) {
      // Do not create a new one if there is one. Use CreateExtensionHost("") to
      // change ExtensionHost
      return;
    }

    Profile* profile = Profile::FromBrowserContext(browser_context());

    const Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::ENABLED);
    if (extension) {
      content::SiteInstance* site_instance = web_contents()->GetSiteInstance();
      const GURL& url = web_contents()->GetURL();

      ViewType host_type = VIEW_TYPE_EXTENSION_POPUP;

      extension_host_.reset(new ExtensionHostForWebContents(
          extension, site_instance, url, host_type, web_contents()));

      notification_registrar_.Add(
          this, extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
          content::Source<content::BrowserContext>(browser_context()));
    }
  }
}

void WebViewGuest::SetExtensionHost(const std::string& extensionhost) {}

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

void WebViewGuest::ShowCertificateViewerInDevTools(
    content::WebContents* web_contents,
    scoped_refptr<net::X509Certificate> certificate) {
  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents));

  DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent.get());
  if (window)
    window->ShowCertificateViewer(certificate);
}

void WebViewGuest::OnContentBlocked(ContentSettingsType settings_type,
                                    const base::string16& details) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());

  args->SetString("blockedType", ContentSettingsTypeToString(settings_type));

  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventContentBlocked, std::move(args))));
}

void WebViewGuest::AllowRunningInsecureContent() {
  web_contents()->ForEachFrame(base::Bind(&SetAllowRunningInsecureContent));
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
