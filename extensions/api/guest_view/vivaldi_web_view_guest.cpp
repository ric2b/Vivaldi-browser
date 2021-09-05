// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "browser/startup_vivaldi_browser.h"
#include "browser/vivaldi_browser_finder.h"
#include "browser/vivaldi_webcontents_util.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h" // nogncheck
#include "content/browser/web_contents/web_contents_impl.h" // nogncheck
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/guest_view/vivaldi_web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/helper/vivaldi_init_helpers.h"
#include "net/cert/x509_certificate.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"

#if defined(USE_AURA)
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window.h"
#endif

using content::WebContents;
using guest_view::GuestViewEvent;
using guest_view::GuestViewManager;
using vivaldi::IsVivaldiApp;
using vivaldi::IsVivaldiRunning;

namespace extensions {

namespace {

std::string WindowOpenDispositionToString(
    WindowOpenDisposition window_open_disposition) {
  switch (window_open_disposition) {
    case WindowOpenDisposition::IGNORE_ACTION:
      return "ignore";
    case WindowOpenDisposition::SAVE_TO_DISK:
      return "save_to_disk";
    case WindowOpenDisposition::CURRENT_TAB:
      return "current_tab";
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
      return "new_background_tab";
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      return "new_foreground_tab";
    case WindowOpenDisposition::NEW_WINDOW:
      return "new_window";
    case WindowOpenDisposition::NEW_POPUP:
      return "new_popup";
    case WindowOpenDisposition::OFF_THE_RECORD:
      return "off_the_record";
    default:
      NOTREACHED() << "Unknown Window Open Disposition";
      return "ignore";
  }
}

void SetAllowRunningInsecureContent(content::RenderFrameHost* frame) {
  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent> renderer;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&renderer);
  renderer->SetAllowRunningInsecureContent();
}

static std::string SSLStateToString(SecurityStateTabHelper* helper) {
  security_state::SecurityLevel status = helper->GetSecurityLevel();
  switch (status) {
    case security_state::NONE:
      // HTTP/no URL/user is editing
      return "none";
    case security_state::WARNING:
      // show a visible warning about the page's lack of security
      return "warning";
    case security_state::SECURE: {
      auto visible_security_state = helper->GetVisibleSecurityState();
      if (visible_security_state->cert_status & net::CERT_STATUS_IS_EV)
        return "secure_with_ev";
      // HTTPS
      return "secure_no_ev";
    }
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
    case ContentSettingsType::COOKIES:
      return "cookies";
    case ContentSettingsType::IMAGES:
      return "images";
    case ContentSettingsType::JAVASCRIPT:
      return "javascript";
    case ContentSettingsType::PLUGINS:
      return "plugins";
    case ContentSettingsType::POPUPS:
      return "popups";
    case ContentSettingsType::GEOLOCATION:
      return "location";
    case ContentSettingsType::MIXEDSCRIPT:
      return "mixed-script";
    case ContentSettingsType::PROTOCOL_HANDLERS:
      return "register-protocol-handler";
    case ContentSettingsType::PPAPI_BROKER:
      return "ppapi-broker";
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      return "multiple-automatic-downloads";
    case ContentSettingsType::MIDI_SYSEX:
      return "midi-sysex";
    case ContentSettingsType::ADS:
      return "ads";
    default:
      // fallthrough
      break;
  }
  return "unknown";
}

}  // namespace

#if defined(USE_AURA)
std::unique_ptr<WebViewGuest::CursorHider> WebViewGuest::CursorHider::Create(
    aura::Window* window) {
  return std::unique_ptr<CursorHider>(
      base::WrapUnique(new CursorHider(window)));
}

void WebViewGuest::CursorHider::Hide() {
  cursor_client_->HideCursor();
}

WebViewGuest::CursorHider::CursorHider(aura::Window* window) {
  cursor_client_ = aura::client::GetCursorClient(window);
  hide_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(TIME_BEFORE_HIDING_MS),
                    this, &CursorHider::Hide);
}

WebViewGuest::CursorHider::~CursorHider() {
  cursor_client_->ShowCursor();
}
#endif  // USE_AURA

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
      }
      guest_site_instance =
          content::SiteInstance::CreateForGuest(context, site);
    }
  }

  WebContents::CreateParams params(context, guest_site_instance);
  // As the tabstrip-content is not a guest we need to delete it and
  // re-create it as a guest then attach all the webcontents observers and
  // in addition replace the tabstrip content with the new guest content.
  params.guest_delegate = this;
  return params;
}

void WebViewGuest::ExtendedLoadProgressChanged(double progress,
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

void WebViewGuest::ToggleFullscreenModeForTab(
    content::WebContents* web_contents,
    bool enter_fullscreen,
    bool skip_window_state) {
  if (enter_fullscreen == is_fullscreen_)
    return;
  is_fullscreen_ = enter_fullscreen;

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

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger("windowId", browser->session_id().id());
  args->SetBoolean("enterFullscreen", enter_fullscreen);
  DispatchEventToView(base::WrapUnique(
    new GuestViewEvent(webview::kEventOnFullscreen, std::move(args))));

  bool is_tab_casting = web_contents->IsBeingCaptured();
  // Avoid bringing the window to fullscreen if we are tab-casting.
  if (skip_window_state || is_tab_casting) {
    return;
  }

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

void WebViewGuest::ShowPageInfo(gfx::Point pos) {
  content::NavigationController& controller = web_contents()->GetController();
  const content::NavigationEntry* activeEntry = controller.GetActiveEntry();
  if (!activeEntry) {
    return;
  }

  const GURL url = controller.GetActiveEntry()->GetURL();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  // Happens for WebContents not in a tabstrip.
  if (!browser) {
    browser = chrome::FindLastActiveWithProfile(profile);
  }

  if (browser->window()) {
    browser->window()->VivaldiShowWebsiteSettingsAt(
        profile, web_contents(), url, pos);
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
    // Notify the Vivaldi browser window about load state.
    VivaldiBrowserWindow* app_win =
        static_cast<VivaldiBrowserWindow*>(browser->window());
    if (app_win) {
      app_win->NavigationStateChanged(web_contents(), changed_flags);
    }
  }
}

bool WebViewGuest::EmbedsFullscreenWidget() {
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
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  if (!helper) {
    return;
  }

  args->SetString("SSLState", SSLStateToString(helper));

  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (entry) {
    scoped_refptr<net::X509Certificate> cert(
        helper->GetVisibleSecurityState()->certificate);

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
                                           bool activePage,
                                           bool inherit_opener) {
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
  content::WebContents* existing_tab =
      tab_strip->count() == 1 ? tab_strip->GetWebContentsAt(0) : nullptr;

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
      TabStripModel::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;
  if (inherit_opener) {
    add_types |= TabStripModel::ADD_INHERIT_OPENER;
  }

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
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWindowFeatures(blink::mojom::WindowFeatures().Clone());
  }
  if (existing_tab) {
    // We had a single tab open, check if it's speed dial.
    GURL url = existing_tab->GetURL();
    if (url == GURL(::vivaldi::kVivaldiNewTabURL)) {
      // If it's Speed Dial, close it immediately. New windows always
      // get a Speed Dial tab initially as some extensions expect it.
      tab_strip->CloseWebContentsAt(
          tab_strip->GetIndexOfWebContents(existing_tab), 0);
    }
  }

}

blink::SecurityStyle WebViewGuest::GetSecurityStyle(
    WebContents* contents,
    content::SecurityStyleExplanations* security_style_explanations) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (browser) {
    return browser->GetSecurityStyle(contents, security_style_explanations);
  } else {
    return blink::SecurityStyle::kUnknown;
  }
}

void WebViewGuest::OnContentAllowed(ContentSettingsType settings_type) {
  auto args = std::make_unique<base::DictionaryValue>();
  args->SetString("allowedType", ContentSettingsTypeToString(settings_type));
  DispatchEventToView(std::make_unique<GuestViewEvent>(
      webview::kEventContentAllowed, std::move(args)));
}

void WebViewGuest::OnContentBlocked(ContentSettingsType settings_type) {
  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString("blockedType", ContentSettingsTypeToString(settings_type));
  DispatchEventToView(base::WrapUnique(
    new GuestViewEvent(webview::kEventContentBlocked, std::move(args))));
}

void WebViewGuest::OnWindowBlocked(
    const GURL& window_target_url,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features) {

  std::unique_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kTargetURL, window_target_url.spec());
  if (features.has_height) {
    args->SetInteger(webview::kInitialHeight, features.height);
  }
  if (features.has_width) {
    args->SetInteger(webview::kInitialWidth, features.width);
  }
  if (features.has_x) {
    args->SetInteger(webview::kInitialLeft, features.x);
  }
  if (features.has_y) {
    args->SetInteger(webview::kInitialTop, features.y);
  }
  args->SetString(webview::kName, frame_name);
  args->SetString(webview::kWindowOpenDisposition,
    WindowOpenDispositionToString(disposition));

  DispatchEventToView(base::WrapUnique(
    new GuestViewEvent(webview::kEventWindowBlocked, std::move(args))));
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
  TabModalConfirmDialog::Create(std::make_unique<RepostFormWarningController>(source),
                                source);
}

content::PictureInPictureResult WebViewGuest::EnterPictureInPicture(
    WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  return PictureInPictureWindowManager::GetInstance()->EnterPictureInPicture(
      web_contents, surface_id, natural_size);
}

void WebViewGuest::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
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
      tab_strip->ActivateTabAt(tab_index);
    }
  }
  web_contents()->SetUserData(::vivaldi::kVivaldiStartupTabUserDataKey,
                              nullptr);

  // Make sure security state is updated.
  VisibleSecurityStateChanged(web_contents());
}

content::WebContentsDelegate* WebViewGuest::GetDevToolsConnector() {
  if (::vivaldi::IsVivaldiRunning() && connector_item_)
    return connector_item_.get();
  return this;
}

content::KeyboardEventProcessingResult WebViewGuest::PreHandleKeyboardEvent(
  content::WebContents* source,
  const content::NativeWebKeyboardEvent& event) {
  DCHECK(source == web_contents());
  // We need override this at an early stage since |KeyboardEventManager| will
  // block the delegate(WebViewGuest::HandleKeyboardEvent) if the page does
  // event.preventDefault
  bool handled = false;
  if (event.windows_key_code == ui::VKEY_ESCAPE) {
    // Go out of fullscreen or mouse-lock and pass the event as
    // handled if any of these modes are ended.
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (browser && browser->is_vivaldi()) {
      // If we have both an html5 full screen and a mouse lock, follow Chromium
      // and unlock both.
      //
      // TODO(igor@vivaldi.com): Find out if we should check for
      // rwhv->IsKeyboardLocked() here and unlock the keyboard as well.
      content::RenderWidgetHostView* rwhv =
          web_contents()->GetMainFrame()->GetView();
      if (rwhv->IsMouseLocked()) {
        rwhv->UnlockMouse();
        handled = true;
      }
      if (IsFullscreenForTabOrPending(web_contents())) {
        // Go out of fullscreen if this was a webpage caused fullscreen.
        ExitFullscreenModeForTab(web_contents());
        handled = true;
      }
    }
  }
  if (handled)
    return content::KeyboardEventProcessingResult::HANDLED;
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

void WebViewGuest::SetIsNavigatingAwayFromVivaldiUI(bool away) {
  is_navigating_away_from_vivaldi_ui_ = away;
}

}  // namespace extensions

////////////////////////////////////////////////////////////////////////////////
// Bridge helpers to allow usage of component code in the browser.
////////////////////////////////////////////////////////////////////////////////
namespace guest_view {

// declared in src/components/guest_view/browser/guest_view_base.h
void AttachWebContentsObservers(content::WebContents* contents) {
  if (vivaldi::IsVivaldiRunning()) {
    extensions::WebNavigationTabObserver::CreateForWebContents(contents);
    vivaldi::InitHelpers(contents);
  }
}
}  // namespace guest_view
