// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "app/vivaldi_apptools.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/view_type_utils.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "content/public/browser/cert_store.h"

#ifdef VIVALDI_BUILD
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "prefs/vivaldi_pref_names.h"
#endif // VIVALDI_BUILD

#define ROCKER_GESTURES

#if defined(OS_LINUX) || defined(OS_MACOSX)
// Vivaldi addition: only define mouse gesture for OSX & linux
// because context menu is shown on mouse down for those systems
#define MOUSE_GESTURES
#endif

using content::WebContents;
using guest_view::GuestViewEvent;
using guest_view::GuestViewManager;
using security_state::SecurityStateModel;
using vivaldi::IsVivaldiApp;
using vivaldi::IsVivaldiRunning;

namespace extensions {

ExtensionHostForWebContents::ExtensionHostForWebContents(
    const Extension *extension, content::SiteInstance *site_instance,
    const GURL &url, ViewType host_type, content::WebContents *contents)
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

#ifdef VIVALDI_BUILD
static std::string SSLStateToString(SecurityStateModel::SecurityLevel status) {
  switch (status) {
  case SecurityStateModel::NONE:
    // HTTP/no URL/user is editing
    return "none";
  case SecurityStateModel::EV_SECURE:
    // HTTPS with valid EV cert
    return "secure_with_ev";
  case SecurityStateModel::SECURE:
    // HTTPS (non-EV)
    return "secure_no_ev";
  case SecurityStateModel::SECURITY_WARNING:
    // HTTPS, but unable to check certificate revocation status or with insecure
    // content on the page
    return "security_warning";
  case SecurityStateModel::SECURITY_POLICY_WARNING:
    // HTTPS, but the certificate verification chain is anchored on a certificate
    // that was installed by the system administrator
    return "security_policy_warning";
  case SecurityStateModel::SECURITY_ERROR:
    // Attempted HTTPS and failed, page not authenticated
    return "security_error";
  default:
    //fallthrough
    break;
  }
  NOTREACHED() << "Unknown SecurityStateModel::SecurityLevel";
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
#endif //VIVALDI_BUILD

} // namespace

WebContents::CreateParams WebViewGuest::GetWebContentsCreateParams(
    content::BrowserContext* context,
    const GURL site) {
  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  auto guest_view_manager = GuestViewManager::FromBrowserContext(context);
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

// Initialize listeners (cannot do it in constructor as RenderViewHost not ready.)
void WebViewGuest::InitListeners() {
  content::RenderViewHost* render_view_host =
          web_contents()->GetRenderViewHost();


  if (render_view_host && current_host_ != render_view_host) {
    // Add mouse event listener, only one for every new render_view_host
    mouseevent_callback_ = base::Bind(&WebViewGuest::OnMouseEvent, base::Unretained(this));
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

    if (enter_fullscreen) {
      window_state_prior_to_fullscreen_ = current_window_state;
      app_win->Fullscreen();
    }
    else
    {
      switch (window_state_prior_to_fullscreen_) {
      case ui::SHOW_STATE_MAXIMIZED:
      case ui::SHOW_STATE_NORMAL:
      case ui::SHOW_STATE_DEFAULT:
        // If state did not change we had a plugin that came out of fullscreen.
        // Only HTML-element fullscreen changes the appwindow state.
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

void WebViewGuest::ShowValidationMessage(content::WebContents *web_contents,
                                         const gfx::Rect &anchor_in_root_view,
                                         const base::string16 &main_text,
                                         const base::string16 &sub_text) {
  validation_message_bubble_ =
      TabDialogs::FromWebContents(web_contents)
          ->ShowValidationMessage(anchor_in_root_view, main_text, sub_text);
}

void WebViewGuest::HideValidationMessage(content::WebContents* web_contents) {
  if (validation_message_bubble_)
    validation_message_bubble_->CloseValidationMessage();
}

void WebViewGuest::MoveValidationMessage(content::WebContents *web_contents,
                                         const gfx::Rect &anchor_in_root_view) {
  if (!validation_message_bubble_)
    return;
  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    validation_message_bubble_->SetPositionRelativeToAnchor(
        rwhv->GetRenderWidgetHost(), anchor_in_root_view);
  }
}

bool WebViewGuest::IsVivaldiWebPanel() {
  return name_.compare("vivaldi-webpanel") == 0;
}

void WebViewGuest::SetVisible(bool is_visible) {

  is_visible_ = is_visible;
  content::RenderWidgetHostView *widgethostview =
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
    AppWindowRegistry* app_registry = AppWindowRegistry::Get(browser->profile());
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
#ifdef VIVALDI_BUILD
  content::NavigationController& controller =
    web_contents()->GetController();
  const content::NavigationEntry* activeEntry = controller.GetActiveEntry();
  if (!activeEntry) {
    return;
  }

  const GURL url = controller.GetActiveEntry()->GetURL();

  const ChromeSecurityStateModelClient *security_info =
      ChromeSecurityStateModelClient::FromWebContents(web_contents());
  DCHECK(security_info);
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  if (browser->window())
    browser->window()->VivaldiShowWebsiteSettingsAt(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      web_contents(), url, security_info->GetSecurityInfo(), pos);

#endif // VIVALDI_BUILD
}

#ifdef VIVALDI_BUILD
void WebViewGuest::UpdateMediaState(TabAlertState state) {
  if (state != media_state_) {
    std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());

    args->SetString("activeMediaType", TabAlertStateToString(state));

    DispatchEventToView(base::WrapUnique(
        new GuestViewEvent(webview::kEventMediaStateChanged, std::move(args))));
  }
  media_state_ = state;
}
#endif // VIVALDI_BUILD

void WebViewGuest::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
#ifdef VIVALDI_BUILD
  UpdateMediaState(chrome::GetTabAlertStateForContents(web_contents()));

  // TODO(gisli):  This would normally be done in the browser, but until we get
  // Vivaldi browser object we do it here (as we did remove the webcontents
  // listener for the current browser).
  Browser *browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    static_cast<content::WebContentsDelegate*>(browser)
        ->NavigationStateChanged(web_contents(), changed_flags);
  }
#endif // VIVALDI_BUILD
}

bool WebViewGuest::EmbedsFullscreenWidget() const {
  // If WebContents::GetFullscreenRenderWidgetHostView is present there is a
  // window other than this handling the fullscreen operation.
  return web_contents()->GetFullscreenRenderWidgetHostView() == NULL;
}

void WebViewGuest::SetIsFullscreen(bool is_fullscreen) {
  is_fullscreen_ = is_fullscreen;
}

void WebViewGuest::VisibleSSLStateChanged(const WebContents* source) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
#ifdef VIVALDI_BUILD
  const ChromeSecurityStateModelClient *security_model =
      ChromeSecurityStateModelClient::FromWebContents(source);
  if (!security_model)
    return;

  SecurityStateModel::SecurityLevel current_level =
    security_model->GetSecurityInfo().security_level;
  args->SetString("SSLState", SSLStateToString(current_level));

  content::NavigationController& controller =
    web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (entry) {

    scoped_refptr<net::X509Certificate> cert;
    content::CertStore::GetInstance()->RetrieveCert(entry->GetSSL().cert_id,
                                                    &cert);

    // EV are required to have an organization name and country.
    if (cert.get() && (!cert.get()->subject().organization_names.empty() &&
      !cert.get()->subject().country_name.empty())) {

      args->SetString("issuerstring", base::StringPrintf("%s [%s]",
        cert.get()->subject().organization_names[0].c_str(),
        cert.get()->subject().country_name.c_str()));

    }

  }
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventSSLStateChanged, std::move(args))));
#endif // VIVALDI_BUILD
}

#ifdef VIVALDI_BUILD
//vivaldi addition:
bool WebViewGuest::GetMousegesturesEnabled() {
  PrefService *pref_service = Profile::FromBrowserContext(
    web_contents()->GetBrowserContext())->GetPrefs();
  return pref_service->GetBoolean(vivaldiprefs::kMousegesturesEnabled);
}
#endif //VIVALDI_BUILD

bool WebViewGuest::IsRockerGesturesEnabled() const {
  PrefService* pref_service = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext())->GetPrefs();
  return pref_service->GetBoolean(vivaldiprefs::kRockerGesturesEnabled);
}

bool WebViewGuest::OnMouseEvent(const blink::WebMouseEvent& mouse_event) {
  // Rocker gestures, a.la Opera
#ifdef ROCKER_GESTURES
  if (IsRockerGesturesEnabled()) {
    if (has_left_mousebutton_down_ &&
        mouse_event.type == blink::WebInputEvent::MouseUp &&
        mouse_event.button == blink::WebMouseEvent::ButtonLeft) {
      has_left_mousebutton_down_ = false;
    }
    else if (mouse_event.type == blink::WebInputEvent::MouseDown &&
        mouse_event.button == blink::WebMouseEvent::ButtonLeft) {
      has_left_mousebutton_down_ = true;
    }

    if (has_right_mousebutton_down_ &&
        mouse_event.type == blink::WebInputEvent::MouseUp &&
        mouse_event.button == blink::WebMouseEvent::ButtonRight) {
      has_right_mousebutton_down_ = false;
    }
    else if (mouse_event.type == blink::WebInputEvent::MouseDown &&
        mouse_event.button == blink::WebMouseEvent::ButtonRight) {
      has_right_mousebutton_down_ = true;
    }

    if (mouse_event.button == blink::WebMouseEvent::ButtonNone) {
      has_right_mousebutton_down_ = false;
      has_left_mousebutton_down_ = false;
    }

    if (has_left_mousebutton_down_ &&
        (mouse_event.type == blink::WebInputEvent::MouseDown &&
        mouse_event.button == blink::WebMouseEvent::ButtonRight)) {
      eat_next_right_mouseup_ = true;
      Go(1);
      return true;
    }

    if (has_right_mousebutton_down_ &&
        (mouse_event.type == blink::WebInputEvent::MouseDown &&
        mouse_event.button == blink::WebMouseEvent::ButtonLeft)) {
      Go(-1);
      eat_next_right_mouseup_ = true;
      return true;
    }

    if (eat_next_right_mouseup_ &&
        mouse_event.type == blink::WebInputEvent::MouseUp &&
        mouse_event.button == blink::WebMouseEvent::ButtonRight) {
      eat_next_right_mouseup_ = false;
      return true;
    }
  }
#endif // ROCKER_GESTURES

#ifdef MOUSE_GESTURES
  if (!GetMousegesturesEnabled()) {
    return false;
  }

  if (IsVivaldiRunning() &&
      mouse_event.type == blink::WebInputEvent::MouseMove &&
      mouse_event.type != blink::WebInputEvent::MouseDown &&
      gesture_state_ == GestureStateRecording) {
    // recording a gesture when the mouse is not down does not make sense
    gesture_state_ = GestureStateNone;
  }

  // Record the gesture
  if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonRight &&
    !(mouse_event.modifiers & blink::WebInputEvent::LeftButtonDown) &&
    gesture_state_ == GestureStateNone) {
    gesture_state_ = GestureStateRecording;
    gesture_direction_candidate_x_ = x_ = mouse_event.x;
    gesture_direction_candidate_y_ = y_ = mouse_event.y;
    gesture_direction_ = GestureDirectionNONE;
    gesture_data_ = 0;
    return true;
  } else if (gesture_state_ == GestureStateRecording) {
    if (mouse_event.type == blink::WebInputEvent::MouseMove ||
      mouse_event.type == blink::WebInputEvent::MouseUp) {
      int dx = mouse_event.x - gesture_direction_candidate_x_;
      int dy = mouse_event.y - gesture_direction_candidate_y_;
      GestureDirection candidate_direction;
      // Find current direction from last candidate location.
      if (abs(dx) > abs(dy))
        candidate_direction =
            dx > 0 ? GestureDirectionRight : GestureDirectionLeft;
      else // abs(dx) <= abs(dy)
        candidate_direction =
            dy > 0 ? GestureDirectionDown : GestureDirectionUp;

      if (candidate_direction == gesture_direction_) {
        // The mouse is still moving in an overall direction of the last gesture
        // direction, update the candidate location.
        gesture_direction_candidate_x_ = mouse_event.x;
        gesture_direction_candidate_y_ = mouse_event.y;
      } else if (abs(dx) >= 5 || abs(dy) >= 5) {
        gesture_data_ = 0x1; // no more info needed since mouse gestures are
                             // handled by javascript
      }
    }

    // Map a gesture to an action
    if (mouse_event.type == blink::WebInputEvent::MouseUp &&
      mouse_event.button == blink::WebMouseEvent::ButtonRight) {
      blink::WebMouseEvent event_copy(mouse_event);
      content::RenderViewHost *render_view_host =
          web_contents()->GetRenderViewHost();
      // At this point we may be sending events that could look like new
      // gestures, don't consume them.
      gesture_state_ = GestureStateBlocked;
      switch (gesture_data_) {
      case 0: // No sufficient movement
        // Send the originally-culled right mouse down at original coords
        event_copy.type = blink::WebInputEvent::MouseDown;
        event_copy.windowX -= (mouse_event.x - x_);
        event_copy.windowY -= (mouse_event.y - y_);
        event_copy.x = x_;
        event_copy.y = y_;
        render_view_host->GetWidget()->ForwardMouseEvent(event_copy);
        break;
      default:
        //Unknown gesture, don't do anything.
        break;
      }
      gesture_state_ = GestureStateNone;
      return gesture_data_ != 0;
    }
    else if (mouse_event.type == blink::WebInputEvent::MouseDown &&
      mouse_event.button == blink::WebMouseEvent::ButtonLeft) {
      gesture_state_ = GestureStateNone;
    }
  }
  /*if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonLeft &&
    (mouse_event.modifiers & blink::WebInputEvent::RightButtonDown)) {
    if (mouse_event.modifiers & blink::WebInputEvent::ShiftKey)
      // Rewind
      ;
    else
      Go(-1);
    return true;
  }
  if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonRight &&
    (mouse_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    if (mouse_event.modifiers & blink::WebInputEvent::ShiftKey)
      // Fast-forward
      ;
    else
      Go(1);
    return true;
  }*/
#endif // MOUSE_GESTURES

  return false;
}

void WebViewGuest::UpdateTargetURL(content::WebContents *source,
                                   const GURL &url) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kNewURL, url.spec());
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventTargetURLChanged, std::move(args))));
}

void WebViewGuest::CreateSearch(const base::ListValue & search) {
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

void WebViewGuest::PasteAndGo(const base::ListValue & search) {
  std::string clipBoardText;

  if (!search.GetString(0, &clipBoardText)) {
    NOTREACHED();
    return;
  }

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kClipBoardText, clipBoardText);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventPasteAndGo, std::move(args))));
}

void WebViewGuest::SetWebContentsWasInitiallyGuest(bool born_guest) {
  webcontents_was_created_as_guest_ = born_guest;
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
#ifdef VIVALDI_BUILD
  Browser* browser = chrome::FindBrowserWithID(windowId);

  if (!browser || !browser->window()) {
    // TODO(gisli@vivaldi.com): Error message?
    return;
  }

  guest->SetWebContentsWasInitiallyGuest(true);

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
  chrome::NavigateParams navigate_params(
    browser, guest->web_contents());
  navigate_params.disposition =
    active ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  navigate_params.source_contents = web_contents();
  chrome::Navigate(&navigate_params);

  if (active)
    navigate_params.target_contents->SetInitialFocus();

  if (navigate_params.target_contents) {
    navigate_params.target_contents->Send(new ChromeViewMsg_SetWindowFeatures(
        navigate_params.target_contents->GetRoutingID(),
        blink::WebWindowFeatures()));
  }

#endif //VIVALDI_BUILD
}

bool WebViewGuest::RequestPageInfo(const GURL& url) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->Set(webview::kTargetURL, new base::StringValue(url.spec()));
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventRequestPageInfo, std::move(args))));
  return true;
}

void WebViewGuest::CreateExtensionHost(const std::string& extension_id) {

  if(!IsVivaldiApp(owner_host())) {
    // Only allow creation of ExtensionHost if the hosting app is Vivaldi.
    return;
  }

  if (extension_id.empty()) {
    notification_registrar_.Remove(
        this,
        extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
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
        extensions::ExtensionRegistry::Get(profile)
            ->GetExtensionById(extension_id,
                               extensions::ExtensionRegistry::ENABLED);
    if (extension) {
      content::SiteInstance* site_instance = web_contents()->GetSiteInstance();
      const GURL& url = web_contents()->GetURL();

      ViewType host_type = VIEW_TYPE_EXTENSION_POPUP;

      extension_host_.reset(new ExtensionHostForWebContents(
          extension, site_instance, url, host_type, web_contents()));

      notification_registrar_.Add(
          this,
          extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
          content::Source<content::BrowserContext>(browser_context()));
    }
  }
}

void WebViewGuest::SetExtensionHost(const std::string& extensionhost) {
}

content::SecurityStyle WebViewGuest::GetSecurityStyle(
    WebContents* contents,
    content::SecurityStyleExplanations* security_style_explanations) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser) {
    return browser->GetSecurityStyle(contents, security_style_explanations);
  } else {
    return content::SECURITY_STYLE_UNKNOWN;
  }
}

void WebViewGuest::ShowCertificateViewerInDevTools(
    content::WebContents *web_contents, int cert_id) {
  Browser *browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser) {
    browser->ShowCertificateViewerInDevTools(web_contents, cert_id);
  }
}

} // namespace extensions
