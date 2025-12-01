// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"  // nogncheck
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"  // nogncheck
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/eye_dropper_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "extensions/browser/bad_message.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/helper/vivaldi_panel_helper.h"
#include "net/cert/x509_certificate.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/files/file_util.h"
#include "browser/startup_vivaldi_browser.h"
#include "browser/vivaldi_browser_finder.h"
#include "browser/vivaldi_webcontents_util.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "extensions/api/events/vivaldi_ui_events.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/guest_view/parent_tab_user_data.h"
#include "extensions/api/guest_view/vivaldi_web_view_constants.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/helper/vivaldi_init_helpers.h"
#include "extensions/vivaldi_browser_component_wrapper.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/content/vivaldi_tab_check.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"


#if defined(USE_AURA)
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
#include "components/paint_preview/browser/paint_preview_client.h"
#endif


using content::RenderProcessHost;
using content::StoragePartition;
using content::WebContents;
using guest_view::GuestViewEvent;
using guest_view::GuestViewManager;
using vivaldi::IsVivaldiApp;
using vivaldi::IsVivaldiRunning;

namespace extensions {

namespace {

bool IsPanelSensitiveUrl(const GURL& url) {
  // Http and https pages only can be displayed in the incognito window
  // web-panels.
  if (url.SchemeIs("http") || url.SchemeIs("https"))
    return false;
  return true;
}

// local function copied from web_view_guest.cc
/*
std::string GetStoragePartitionIdFromPartitionConfig(
    const content::StoragePartitionConfig& storage_partition_config) {
  const auto& partition_id = storage_partition_config.partition_name();
  bool persist_storage = !storage_partition_config.in_memory();
  return (persist_storage ? webview::kPersistPrefix : "") + partition_id;
}*/

void ParsePartitionParam(const base::Value::Dict& create_params,
                         std::string* storage_partition_id,
                         bool* persist_storage) {
  const std::string* partition_str =
      create_params.FindString(webview::kStoragePartitionId);
  if (!partition_str)
    return;

  // Since the "persist:" prefix is in ASCII, base::StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (base::StartsWith(*partition_str,
                       "persist:", base::CompareCase::SENSITIVE)) {
    size_t index = partition_str->find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    *storage_partition_id = partition_str->substr(index + 1);

    if (storage_partition_id->empty()) {
      // TODO(lazyboy): Better way to deal with this error.
      return;
    }
    *persist_storage = true;
  } else {
    *storage_partition_id = *partition_str;
    *persist_storage = false;
  }
}

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
      //return "ignore";
  }
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
      // HTTPS
      return "secure";
    }
    case security_state::DANGEROUS:
      // Attempted HTTPS and failed, page not authenticated
      return "security_error";
    default:
      // fallthrough
      break;
  }
  NOTREACHED() << "Unknown SecurityLevel";
  //return "unknown";
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
    case ContentSettingsType::POPUPS:
      return "popups";
    case ContentSettingsType::GEOLOCATION:
      return "geolocation";
    case ContentSettingsType::MIXEDSCRIPT:
      return "mixed-script";
    case ContentSettingsType::PROTOCOL_HANDLERS:
      return "register-protocol-handler";
    case ContentSettingsType::AUTOMATIC_DOWNLOADS:
      return "multiple-automatic-downloads";
    case ContentSettingsType::MIDI_SYSEX:
      return "midi-sysex";
    case ContentSettingsType::ADS:
      return "ads";
    case ContentSettingsType::SOUND:
      return "sound";
    case ContentSettingsType::AUTOPLAY:
      return "autoplay";
    case ContentSettingsType::NOTIFICATIONS:
      return "notifications";
    case ContentSettingsType::IDLE_DETECTION:
      return "idle-detection";
    case ContentSettingsType::SENSORS:
      return "sensors";
    case ContentSettingsType::CLIPBOARD_READ_WRITE:
      return "clipboard";
    default:
      // fallthrough
      break;
  }
  return "unknown";
}

void SendEventToView(WebViewGuest& guest,
                     const std::string& event_name,
                     base::Value::Dict args) {
  base::Value::Dict args_value(std::move(args));
  guest.DispatchEventToView(
      std::make_unique<GuestViewEvent>(event_name, std::move(args_value)));
}

bool IsPanelId(const std::string &name) {
  if (name.rfind("WEBPANEL_", 0) == 0) {
    return true;
  }

  if (name.rfind("EXT_PANEL_", 0) == 0) {
    return true;
  }

  return false;
}

void AttachWebContentsObservers(content::WebContents* contents) {
  VivaldiBrowserComponentWrapper::GetInstance()
      ->CreateWebNavigationTabObserver(contents);
  ::vivaldi::InitHelpers(contents);
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

WebViewGuest::CursorHider::CursorHider(aura::Window* window)
    : cursor_client_(aura::client::GetCursorClient(window)) {
  hide_timer_.Start(FROM_HERE, base::Milliseconds(TIME_BEFORE_HIDING_MS), this,
                    &CursorHider::Hide);
}

WebViewGuest::CursorHider::~CursorHider() {
  cursor_client_->ShowCursor();
}
#endif  // USE_AURA

Browser* WebViewGuest::GetBrowser(content::WebContents* web_contents) {
  Browser* browser =
      VivaldiBrowserComponentWrapper::GetInstance()->FindBrowserWithTab(
          web_contents);
  return browser;
}

void WebViewGuest::VivaldiSetLoadProgressEventExtraArgs(
    base::Value::Dict& dictionary) {
  if (!IsVivaldiRunning())
    return;
  const content::PageImpl& page =
      static_cast<const content::PageImpl&>(web_contents()->GetPrimaryPage());
  dictionary.Set(webview::kLoadedBytes,
                 static_cast<double>(page.vivaldi_loaded_bytes()));
  dictionary.Set(webview::kLoadedElements, page.vivaldi_loaded_resources());
  dictionary.Set(webview::kTotalElements, page.vivaldi_total_resources());
}

void WebViewGuest::ToggleFullscreenModeForTab(
    content::WebContents* web_contents,
    bool enter_fullscreen) {
  if (enter_fullscreen == is_fullscreen_)
    return;
  is_fullscreen_ = enter_fullscreen;

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

  Browser* browser = GetBrowser(web_contents);
  base::Value::Dict args;
  args.Set("windowId", browser ? browser->session_id().id() : -1);
  args.Set("enterFullscreen", enter_fullscreen);
  SendEventToView(*this, webview::kEventOnFullscreen, std::move(args));
}

void WebViewGuest::BeforeUnloadFired(content::WebContents* web_contents,
                                     bool proceed,
                                     bool* proceed_to_fire_unload) {
  // Call the Browser class as it already has an instance of the
  // active unload controller.
  VivaldiBrowserComponentWrapper::GetInstance()->DoBeforeUnloadFired(
      web_contents, proceed, proceed_to_fire_unload);
}

void WebViewGuest::SetContentsBounds(content::WebContents* source,
                                     const gfx::Rect& bounds) {
  DCHECK_EQ(web_contents(), source);
  Browser* browser = GetBrowser(source);
  if (browser && browser->window() && !browser->is_type_normal() &&
      !browser->is_type_picture_in_picture()) {
    browser->window()->SetBounds(bounds);
  } else {
    // Store the bounds and use the last received on attach.
    last_set_bounds_.reset(new gfx::Rect(bounds));
  }
}

bool WebViewGuest::IsVivaldiMail() {
  return name_.compare("vivaldi-mail") == 0;
}

bool WebViewGuest::IsVivaldiWebPanel() {
  return name_.compare("vivaldi-webpanel") == 0;
}

bool WebViewGuest::IsVivaldiWebPageWidget() {
  return name_.compare("vivaldi-webpage-widget") == 0;
}

void WebViewGuest::ShowPageInfo(gfx::Point pos) {
  content::NavigationController& controller = web_contents()->GetController();
  const content::NavigationEntry* activeEntry = controller.GetActiveEntry();
  if (!activeEntry) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  Browser* browser = GetBrowser(web_contents());
  if (!browser) {
    // Happens for WebContents not in a tabstrip.
    browser = VivaldiBrowserComponentWrapper::GetInstance()
        ->FindLastActiveBrowserWithProfile(profile);
  }

  if (browser->window()) {
    const GURL url = controller.GetActiveEntry()->GetURL();
    browser->window()->VivaldiShowWebsiteSettingsAt(profile, web_contents(),
                                                    url, pos);
  }
}

void WebViewGuest::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  // This class is the WebContentsDelegate, so forward this event
  // to the normal delegate here.
  Browser* browser = GetBrowser(web_contents());
  if (browser) {
    static_cast<content::WebContentsDelegate*>(browser)->NavigationStateChanged(
        web_contents(), changed_flags);
    // Notify the Vivaldi browser window about load state.
    VivaldiBrowserWindow* browser_window =
        VivaldiBrowserComponentWrapper::GetInstance()
            ->VivaldiBrowserWindowFromBrowser(browser);
    VivaldiBrowserComponentWrapper::GetInstance()
        ->NavigationStateChanged(browser_window, web_contents(), changed_flags);
  }
}

void WebViewGuest::SetIsFullscreen(bool is_fullscreen) {
  SetFullscreenState(is_fullscreen);
  ToggleFullscreenModeForTab(web_contents(), is_fullscreen);
}

void WebViewGuest::VisibleSecurityStateChanged(WebContents* source) {
  base::Value::Dict args;
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents());
  if (!helper) {
    return;
  }

  args.Set("SSLState", SSLStateToString(helper));

  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (entry) {
    scoped_refptr<net::X509Certificate> cert(
        helper->GetVisibleSecurityState()->certificate);

    // EV are required to have an organization name and country.
    if (cert.get() && (!cert.get()->subject().organization_names.empty() &&
                       !cert.get()->subject().country_name.empty())) {
      args.Set(
          "issuerstring",
          base::StringPrintf(
              "%s [%s]", cert.get()->subject().organization_names[0].c_str(),
              cert.get()->subject().country_name.c_str()));
    }
  }
  SendEventToView(*this, webview::kEventSSLStateChanged, std::move(args));
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
  base::Value::Dict args;
  args.Set(webview::kNewURL, url.spec());
  SendEventToView(*this, webview::kEventTargetURLChanged, std::move(args));
}

void WebViewGuest::CreateSearch(const base::Value::List& search) {
  if (search.size() < 2)
    return;
  const std::string* keyword = search[0].GetIfString();
  const std::string* url = search[1].GetIfString();
  if (!keyword || !url)
    return;

  base::Value::Dict args;
  args.Set(webview::kNewSearchName, *keyword);
  args.Set(webview::kNewSearchUrl, *url);
  SendEventToView(*this, webview::kEventCreateSearch, std::move(args));
}

void WebViewGuest::PasteAndGo(const base::Value::List& search) {
  if (search.size() < 3)
    return;
  const std::string* clipBoardText = search[0].GetIfString();
  const std::string* pasteTarget = search[1].GetIfString();
  const std::string* modifiers = search[2].GetIfString();
  if (!clipBoardText || !pasteTarget || !modifiers)
    return;

  base::Value::Dict args;
  args.Set(webview::kClipBoardText, *clipBoardText);
  args.Set(webview::kPasteTarget, *pasteTarget);
  args.Set(webview::kModifiers, *modifiers);
  SendEventToView(*this, webview::kEventPasteAndGo, std::move(args));
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
  VivaldiBrowserComponentWrapper::GetInstance()->AddGuestToTabStripModel(
      web_contents(), guest->web_contents(), windowId, activePage,
      inherit_opener, extension_host_ != nullptr);
}

void WebViewGuest::OnContentAllowed(ContentSettingsType settings_type) {
  base::Value::Dict args;
  args.Set("allowedType", ContentSettingsTypeToString(settings_type));
  SendEventToView(*this, webview::kEventContentAllowed, std::move(args));
}

void WebViewGuest::OnContentBlocked(ContentSettingsType settings_type) {
  base::Value::Dict args;
  args.Set("blockedType", ContentSettingsTypeToString(settings_type));
  SendEventToView(*this, webview::kEventContentBlocked, std::move(args));
}

void WebViewGuest::OnWindowBlocked(
    const GURL& window_target_url,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features) {
  base::Value::Dict args;
  args.Set(webview::kTargetURL, window_target_url.spec());
  if (features.has_height) {
    args.Set(webview::kInitialHeight, features.bounds.height());
  }
  if (features.has_width) {
    args.Set(webview::kInitialWidth, features.bounds.width());
  }
  if (features.has_x) {
    args.Set(webview::kInitialLeft, features.bounds.x());
  }
  if (features.has_y) {
    args.Set(webview::kInitialTop, features.bounds.y());
  }
  args.Set(webview::kName, frame_name);
  args.Set(webview::kWindowOpenDisposition,
           WindowOpenDispositionToString(disposition));

  SendEventToView(*this, webview::kEventWindowBlocked, std::move(args));
}

void WebViewGuest::AllowRunningInsecureContent() {
  VivaldiBrowserComponentWrapper::GetInstance()->AllowRunningInsecureContent(
      web_contents());
}

bool WebViewGuest::ShouldAllowRunningInsecureContent(WebContents* web_contents,
                                                     bool allowed_per_prefs,
                                                     const url::Origin& origin,
                                                     const GURL& resource_url) {
  Browser* browser = GetBrowser(web_contents);
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
  VivaldiBrowserComponentWrapper::GetInstance()->ShowRepostFormWarningDialog(
      source);
}

content::PictureInPictureResult WebViewGuest::EnterPictureInPicture(
    WebContents* web_contents) {
  return VivaldiBrowserComponentWrapper::GetInstance()
      ->EnterPictureInPicture(web_contents);
}

void WebViewGuest::ExitPictureInPicture() {
  VivaldiBrowserComponentWrapper::GetInstance()->ExitPictureInPicture();
}

std::unique_ptr<content::EyeDropper> WebViewGuest::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return VivaldiBrowserComponentWrapper::GetInstance()->OpenEyeDropper(
      frame,
      listener);
}

void WebViewGuest::CapturePaintPreviewOfSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    const base::UnguessableToken& guid,
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
  auto* client =
      paint_preview::PaintPreviewClient::FromWebContents(web_contents);
  if (client) {
    client->CaptureSubframePaintPreview(guid, rect, render_frame_host);
  }
#endif
}

void WebViewGuest::LoadTabContentsIfNecessary() {
  web_contents()->GetController().LoadIfNecessary();

  VivaldiBrowserComponentWrapper::GetInstance()->LoadTabContentsIfNecessary(
      web_contents());

  // Make sure security state is updated.
  VisibleSecurityStateChanged(web_contents());
}

content::WebContentsDelegate* WebViewGuest::GetDevToolsConnector() {
  if (::vivaldi::IsVivaldiRunning() && connector_item_)
    return connector_item_;
  return this;
}

bool WebViewGuest::ShortcutFoundInPrefs(std::string shortcut_text) {
  Browser* browser = GetBrowser(web_contents());
  if (!browser || !browser->is_vivaldi())
    return false;

  PrefService* prefs = browser->profile()->GetPrefs();
  const PrefService::Preference* actions =
      prefs->FindPreference(vivaldiprefs::kActions);

  const base::Value::List& action_list = actions->GetValue()->GetList();
  auto* shortcut_dict = &(action_list[0].GetDict());
  for (auto dict_entry = shortcut_dict->begin();
     dict_entry != shortcut_dict->end(); ++dict_entry) {
    auto* shortcuts = dict_entry->second.GetDict().Find("shortcuts");
    if (shortcuts) {
      const auto* shortcut_list = &(shortcuts[0].GetList());
      if (shortcut_list && shortcut_list->size()) {
        for (auto list_entry = shortcut_list->begin();
             list_entry != shortcut_list->end(); ++list_entry) {
          if (list_entry->GetString() == shortcut_text) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

// Website has shortcut priority set to 'browser' and shortcut matches browser
// shortcut.
bool WebViewGuest::ShouldForwardShortcutToBrowser(
    const input::NativeWebKeyboardEvent& event) {
  Browser* browser = GetBrowser(web_contents());
  if (!browser || !browser->is_vivaldi()) {
    return false;
  }

  std::string shortcut_text = ::vivaldi::ShortcutTextFromEvent(event);
  std::transform(shortcut_text.begin(), shortcut_text.end(),
                 shortcut_text.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  PrefService* prefs = browser->profile()->GetPrefs();

  // TODO (daniel): This is getting a little complicated and should soon be
  // split up and possibly put into its own file. It also remains to check how
  // much of this is also done in the js side and can be simplified over there.

  // Handling for single key shortcuts.
  int c = event.windows_key_code;
  if (shortcut_text.length() == 1 || c == ui::VKEY_BACK ||
      (c >= ui::VKEY_NUMPAD0 && c <= ui::VKEY_NUMPAD9)) {
    const PrefService::Preference* single_key_pref =
        prefs->FindPreference(vivaldiprefs::kKeyboardShortcutsEnableSingleKey);
    if (single_key_pref) {
       if (!single_key_pref->GetValue()->GetBool()) {
         return false;
       }
       if (web_contents()->IsFocusedElementEditable()) {
         return false;
       }
    }
  }

  const PrefService::Preference* browser_priority_keys =
      prefs->FindPreference(
          vivaldiprefs::kKeyboardShortcutsBrowserPriorityList);
  const base::Value::List& browser_priority_list =
      browser_priority_keys->GetValue()->GetList();
  for (auto entry = browser_priority_list.begin();
       entry != browser_priority_list.end(); ++entry) {
    if (entry->GetString() == shortcut_text) {
      return true;
    }
  }
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  bool browser_priority = VivaldiBrowserComponentWrapper::GetInstance()
      ->HasBrowserShortcutPriority(profile, web_contents()->GetURL());
  return browser_priority && ShortcutFoundInPrefs(shortcut_text);
}

content::KeyboardEventProcessingResult WebViewGuest::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  DCHECK(source == web_contents());

  // No need to do anything here since char events are just for typing.
  if (event.GetType() == blink::WebInputEvent::Type::kChar) {
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  // We need override this at an early stage since |KeyboardEventManager| will
  // block the delegate(WebViewGuest::HandleKeyboardEvent) if the page does
  // event.preventDefault
  if (event.windows_key_code == ui::VKEY_ESCAPE) {
    bool handled = false;
    // Go out of fullscreen or mouse-lock and pass the event as
    // handled if any of these modes are ended.
    Browser* browser = GetBrowser(web_contents());
    if (browser && browser->is_vivaldi()) {
      // If we have both an html5 full screen and a mouse lock, follow Chromium
      // and unlock both.
      //
      // TODO(igor@vivaldi.com): Find out if we should check for
      // rwhv->IsKeyboardLocked() here and unlock the keyboard as well.
      content::RenderWidgetHostView* rwhv =
          web_contents()->GetPrimaryMainFrame()->GetView();
      if (rwhv->IsPointerLocked()) {
        rwhv->UnlockPointer();
        handled = true;
      }
      if (IsFullscreenForTabOrPending(web_contents())) {
        // Go out of fullscreen if this was a webpage caused fullscreen.
        ExitFullscreenModeForTab(web_contents());
        handled = true;
      }
    }
    if (handled)
      return content::KeyboardEventProcessingResult::HANDLED;
  }

  // Check if our shortcut has browser priority, i.e. should be handled by
  // the browser and not by the website.
  Browser* browser = GetBrowser(web_contents());
  if (browser && ShouldForwardShortcutToBrowser(event)) {
    extensions::VivaldiUIEvents::SendKeyboardShortcutEvent(
        browser->session_id().id(), browser->profile(), event, false, true);
    return content::KeyboardEventProcessingResult::HANDLED;
  }

  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

void WebViewGuest::SetIsNavigatingAwayFromVivaldiUI(bool away) {
  is_navigating_away_from_vivaldi_ui_ = away;
}

void WebViewGuest::VivaldiCreateWebContents(
    std::unique_ptr<GuestViewBase> owned_this,
    const base::Value::Dict& create_params,
    GuestPageCreatedCallback guestpage_created_callback) {

  Profile* profile = Profile::FromBrowserContext(browser_context());
  content::BrowserContext* context = browser_context();
  std::unique_ptr<WebContents> new_contents;

  parent_tab_id_ = std::nullopt;

  // Optimize for the most common path.
  if (auto tab_id = create_params.FindInt("tab_id")) {
    // If we created the WebContents through CreateNewWindow and created this
    // guest with InitWithWebContents we cannot delete the tabstrip contents,
    // and we don't need to recreate the webcontents either. Just use the
    // WebContents owned by the tab-strip.
    content::WebContents* tabstrip_contents =
        VivaldiBrowserComponentWrapper::GetInstance()
            ->GetWebContentsFromTabStrip(browser_context(), *tab_id, nullptr);

    if (tabstrip_contents) {
      new_contents.reset(
          tabstrip_contents);  // Tabstrip must not lose ownership. Will
                               // override and release in
                               // ClearOwnedGuestContents
      // Make sure we clean up WebViewguests with the same webcontents.
      extensions::WebViewGuest* web_view_guest =
          extensions::WebViewGuest::FromWebContents(tabstrip_contents);
      if (web_view_guest) {
        zoom::ZoomController::FromWebContents(tabstrip_contents)
            ->RemoveObserver(web_view_guest);

        static_cast<content::WebContentsImpl*>(tabstrip_contents)
          ->CancelActiveAndPendingDialogs();

        web_view_guest->WebContentsDestroyed();
      }

      parent_tab_id_ = create_params.FindInt("parent_tab_id");
      if (parent_tab_id_) {
        auto* tab_data = ::vivaldi::ParentTabUserData::GetParentTabUserData(
            new_contents.get());
        tab_data->SetParentTabId(*parent_tab_id_);
      }

      CreatePluginGuest(new_contents.get());

      // Fire a WebContentsCreated event informing the client that script-
      // injection can be done.
      SendEventToView(*this, webview::kEventWebContentsCreated,
                      base::Value::Dict());

      AttachWebContentsObservers(new_contents.get());

      std::move(guestpage_created_callback)
          .Run(std::move(owned_this), std::move(new_contents));

      return;

    }
    // Should not happen that a tab-id lookup should fail.
    // Investigate any reports as soon as possible. The tabstrip
    // must have the index it has reported it has.
    LOG(ERROR)
        << "WebViewGuest::VivaldiCreateWebContents lookup failed for tab_id: "
        << *tab_id;
    new_contents.reset(); // Just to be on the safe side, to make sure it is nullptr
    std::move(guestpage_created_callback)
        .Run(std::move(owned_this), std::move(new_contents));
    return;
  }

  RenderProcessHost* owner_render_process_host =
      owner_web_contents()->GetPrimaryMainFrame()->GetProcess();
  // browser_context_ is always owner_web_contents->GetBrowserContext().
  DCHECK_EQ(browser_context(), owner_render_process_host->GetBrowserContext());

  std::string storage_partition_id;
  bool persist_storage = false;
  ParsePartitionParam(create_params, &storage_partition_id, &persist_storage);
  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!base::IsStringUTF8(storage_partition_id)) {
    bad_message::ReceivedBadMessage(owner_render_process_host,
                                    bad_message::WVG_PARTITION_ID_NOT_UTF8);
    std::move(guestpage_created_callback)
        .Run(std::move(owned_this), std::unique_ptr<content::WebContents>());
    return;
  }
  std::string partition_domain = GetOwnerSiteURL().host();
  auto partition_config = content::StoragePartitionConfig::Create(
      browser_context(), partition_domain, storage_partition_id,
      !persist_storage /* in_memory */);

  if (GetOwnerSiteURL().SchemeIs(extensions::kExtensionScheme)) {
    auto owner_config =
        extensions::util::GetStoragePartitionConfigForExtensionId(
            GetOwnerSiteURL().host(), browser_context());
    if (browser_context()->IsOffTheRecord()) {
      DCHECK(owner_config.in_memory());
    }
    if (!owner_config.is_default()) {
      partition_config.set_fallback_to_partition_domain_for_blob_urls(
          owner_config.in_memory()
              ? content::StoragePartitionConfig::FallbackMode::
                    kFallbackPartitionInMemory
              : content::StoragePartitionConfig::FallbackMode::
                    kFallbackPartitionOnDisk);
      DCHECK(owner_config == partition_config.GetFallbackForBlobUrls().value());
    }
  }

  GURL guest_site;
  if (IsVivaldiApp(owner_host())) {
    const std::string* new_url = create_params.FindString(webview::kNewURL);
    if (new_url) {
      guest_site = GURL(*new_url);
    } else {
      // NOTE(espen@vivaldi.com): This is a workaround for web panels. We can
      // not use GetSiteForGuestPartitionConfig() as that will prevent loading
      // local files later (VB-40707).
      // In NavigationRequest::OnStartChecksComplete() we use the Starting Site
      // Instance which is the same site as set here. Navigating from
      // chrome-guest://mpognobbkildjkofajifpdfhcoklimli/?" which
      // GetSiteForGuestPartitionConfig() returns fails for local file urls.
      guest_site = GURL("file:///");
    }
  }

  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  auto* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  scoped_refptr<content::SiteInstance> guest_site_instance =
      guest_view_manager->GetGuestSiteInstance(partition_config);
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that webview tags are also not allowed to send messages across
    // different partitions.
    guest_site_instance = content::SiteInstance::CreateForGuest(
        browser_context(), partition_config);
  }

  if (auto tab_id = create_params.FindInt("inspect_tab_id")) {
    // We want to attach this guest view to the already existing WebContents
    // currently used for DevTools.
    if (inspecting_tab_id_ == 0 || inspecting_tab_id_ != *tab_id) {
      content::WebContents* inspected_contents =
          VivaldiBrowserComponentWrapper::GetInstance()
              ->GetWebContentsFromTabStrip(browser_context(), *tab_id, nullptr);
      if (inspected_contents) {
        // NOTE(david@vivaldi.com): This returns always the |main_web_contents_|
        // which is required when the dev tools window is undocked.

        content::WebContents* devtools_contents = nullptr;

        // NOTE(david@vivaldi.com): Each docking state has it's own dedicated
        // webview now (VB-42802). We need to make sure that we attach this
        // guest view either to the already existing |toolbox_web_contents_|
        // which is required for undocked dev tools or to the
        // |main_web_contents_| when docked. Each guest view will be reattached
        // after docking state was changed.
        // VB-94370 introduced replacement of the docked/undocked webviews.
        if (auto* paramstr = create_params.FindString("name")) {
          if (*paramstr == "vivaldi-devtools-undocked") {
            // Make sure we always use the toolbox_contents_ from
            // DevtoolsWindow.
            devtools_contents = VivaldiBrowserComponentWrapper::GetInstance()
                                    ->DevToolsWindowGetInTabWebContents(
                                        inspected_contents, nullptr);
          } else if (*paramstr == "vivaldi-devtools-main") {
            // Make sure we always use the main_contents_ from DevtoolsWindow.
            devtools_contents =
                VivaldiBrowserComponentWrapper::GetInstance()
                    ->DevToolsWindowGetDevtoolsWebContentsForInspectedWebContents(
                    inspected_contents);
          }
        }
        DCHECK(devtools_contents);
        if (!devtools_contents) {
          // TODO(tomas@vivaldi.com): Band-aid for VB-48293
          std::move(guestpage_created_callback)
              .Run(std::move(owned_this),
                   std::unique_ptr<content::WebContents>());
          return;
        }
        content::WebContentsImpl* contents =
            static_cast<content::WebContentsImpl*>(devtools_contents);

        connector_item_ = VivaldiBrowserComponentWrapper::GetInstance()
            ->ConnectDevToolsWindow(browser_context(), *tab_id,
                                    inspected_contents, this);

        VivaldiTabCheck::MarkAsDevToolContents(devtools_contents);

        // Make sure we clean up WebViewguests with the same webcontents.
        extensions::WebViewGuest* web_view_guest =
            extensions::WebViewGuest::FromWebContents(devtools_contents);
        if (web_view_guest) {
          zoom::ZoomController::FromWebContents(devtools_contents)
              ->RemoveObserver(web_view_guest);
          web_view_guest->WebContentsDestroyed();
        }

        new_contents.reset(contents);
        CreatePluginGuest(new_contents.get());
        inspecting_tab_id_ = *tab_id;
        SetAttachParams(create_params);
      }
    }
  } else {
    // This is for opening content for webviews used in various parts in our ui.
    // Devtools and extension popups.
    if (auto* window_id = create_params.FindString(webview::kWindowID)) {
      Browser* browser = VivaldiBrowserComponentWrapper::GetInstance()
          ->FindBrowserWithWindowId(atoi(window_id->c_str()));
      if (browser) {
        context = browser->profile();
        if (auto* src_string = create_params.FindString("src")) {
          guest_site = GURL(*src_string);
          guest_site_instance =
            content::SiteInstance::CreateForURL(context, guest_site);
        }
      }
    }
    if (profile->IsOffTheRecord()) {
      // If storage_partition_id is set to a extension id, this is
      // an extension popup.
      ExtensionRegistry* registry = ExtensionRegistry::Get(context);
      const Extension* extension = registry->GetExtensionById(
          storage_partition_id, extensions::ExtensionRegistry::EVERYTHING);
      if (extension && !IncognitoInfo::IsSplitMode(extension)) {
        // If it's not split-mode, we need to use the original
        // profile.  See CreateViewHostForIncognito.
        context = profile->GetOriginalProfile();
      }
    }

    const std::string* view_name =
        create_params.FindString("vivaldi_view_type");

    if (view_name) {
      if (*view_name == "extension_popup") {
        // 1. Create an ExtensionFrameHelper for the viewtype.
        // 2. take a WebContents as parameter.
        if (auto* src_string = create_params.FindString("src")) {
          GURL popup_url = guest_site = GURL(*src_string);

          scoped_refptr<content::SiteInstance> site_instance =
              ProcessManager::Get(context)->GetSiteInstanceForURL(popup_url);
          WebContents::CreateParams params(context, std::move(site_instance));
          params.guest_delegate = this;
          new_contents = WebContents::Create(params);
          extension_host_ = std::make_unique<::vivaldi::VivaldiExtensionHost>(
              context, popup_url, mojom::ViewType::kExtensionPopup,
              new_contents.get());
          VivaldiBrowserComponentWrapper::GetInstance()
              ->TaskManagerCreateForTabContents(new_contents.get());
        }
      }
    }

    if (!new_contents) {
      // If the guest is embedded inside Vivaldi we cannot set siteinstance on
      // creation since we want to be able to navigate away from the initial url
      // and communicate with the content with script injection and
      // sendMessage. This was bug VB-87237, caused by
      // https://source.chromium.org/chromium/chromium/src/+/5ce2763c03762e7b84fede080ebca1f5b033967e
      // Note this is also triggered for OpenURLFromTab code paths. Background
      // tabs, ctrl+click middleclick.
      if (IsVivaldiApp(owner_host())) {
        WebContents::CreateParams params(context);
        params.guest_delegate = this;
        new_contents = WebContents::Create(params);

        // Let us register protocol handlers from webpanels. Tabs are set up in
        // TabHelpers::AttachTabHelpers.
        VivaldiBrowserComponentWrapper::GetInstance()
            ->PageSpecificContentSettingsCreateForTabContents(
                  new_contents.get());
        // TODO: Is this used for panels now that it is owned by the tabstrip?
        if (view_name && IsPanelId(*view_name)) {
          VivaldiPanelHelper::CreateForWebContents(new_contents.get(), *view_name);
        }
      } else {
        WebContents::CreateParams params(context,
                                         std::move(guest_site_instance));
        params.guest_delegate = this;
        new_contents = WebContents::Create(params);
      }
    }
  }
  DCHECK(new_contents);
  if (owner_web_contents()->IsAudioMuted()) {
    // Note: We have earlier been using
    // LastMuteMetadata::FromWebContents(owner_web_contents())->extension_id)
    // to get the ext id. Probably not needed.
    //
    // NOTE(pettern@vivaldi.com): If the owner is muted it means the webcontents
    // of the AppWindow has been muted due to thumbnail capturing, so we also
    // mute the webview webcontents.
    VivaldiBrowserComponentWrapper::GetInstance()
        ->SetTabAudioMuted(new_contents.get(), true, TabMutedReason::EXTENSION,
              ::vivaldi::kVivaldiAppId);
  }
  // Grant access to the origin of the embedder to the guest process. This
  // allows blob: and filesystem: URLs with the embedder origin to be created
  // inside the guest. It is possible to do this by running embedder code
  // through webview accessible_resources.
  //
  // TODO(dcheng): Is granting commit origin really the right thing to do here?
  if (new_contents->GetPrimaryMainFrame()) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantCommitOrigin(
        new_contents->GetPrimaryMainFrame()->GetProcess()->GetID().value(),
        url::Origin::Create(GetOwnerSiteURL()));
  }

  AttachWebContentsObservers(new_contents.get());

  std::move(guestpage_created_callback)
      .Run(std::move(owned_this), std::move(new_contents));
}

blink::mojom::DisplayMode WebViewGuest::GetDisplayMode(
    const content::WebContents* source) {
  if (!owner_web_contents() || !owner_web_contents()->GetDelegate()) {
    return blink::mojom::DisplayMode::kBrowser;
  }
  return owner_web_contents()->GetDelegate()->GetDisplayMode(source);
}

void WebViewGuest::ActivateContents(content::WebContents* web_contents) {
  if (!attached() || !embedder_web_contents()->GetDelegate())
    return;

  if (VivaldiTabCheck::IsVivaldiTab(web_contents)) {
    Browser* browser = GetBrowser(web_contents);
    if (browser) {
      browser->ActivateContents(web_contents);
    }
    return;
  }

  // Fallback: will focus the embedder if attached, as in
  // GuestViewBase::ActivateContents
  embedder_web_contents()->GetDelegate()->ActivateContents(
      embedder_web_contents());
}

void WebViewGuest::VivaldiCanDownload(const GURL& url,
                                      const std::string& request_method,
                                      base::OnceCallback<void(bool)> callback) {
  GURL tab_url = web_contents()->GetURL();
  // Since we do not yet have a DownloadItem we need to mimic the behavior in
  // |GetInsecureDownloadStatusForDownload|

  bool is_redirect_chain_secure = true;
  // Was the download initiated by a secure origin, but delivered insecurely?
  bool is_mixed_content = false;
  // Was the download initiated by an insecure origin or delivered insecurely?
  bool is_insecure_download = false;

  url::Origin initiator = url::Origin::Create(tab_url);

  // Skip over the final URL so that we can investigate it separately below.
  // The redirect chain always contains the final URL, so this is always safe
  // in Chrome, but some tests don't plan for it, so we check here.
  if (download_info_.redirect_chain.size() > 1) {
    for (unsigned i = 0; i < download_info_.redirect_chain.size() - 1; ++i) {
      const GURL& last_url = download_info_.redirect_chain[i];
      if (!network::IsUrlPotentiallyTrustworthy(last_url)) {
        is_redirect_chain_secure = false;
        break;
      }
    }
  }
  // Whether or not the download was securely delivered, ignoring where we got
  // the download URL from (i.e. ignoring the initiator).
  bool download_delivered_securely =
      is_redirect_chain_secure && (network::IsUrlPotentiallyTrustworthy(url) ||
                                   url.SchemeIsBlob() || url.SchemeIsFile());

  // Mixed downloads are those initiated by a secure initiator but not
  // delivered securely.
  is_mixed_content = (initiator.GetURL().SchemeIsCryptographic() &&
                      !download_delivered_securely);

  is_insecure_download =
      (((!initiator.opaque() &&
         !network::IsUrlPotentiallyTrustworthy(initiator.GetURL()))) ||
       !download_delivered_securely) &&
      !net::IsLocalhost(url);

  download_info_.blocked_mixed = is_insecure_download || is_mixed_content;

  // If the download was started by a page mechanism, direct download etc. Allow
  // the download, the user will be asked by the download interceptor.
  // When the download is content_initiated and there is still no suggested
  // target filename we assume this is a cors-preflight request.

  std::u16string default_filename(
      l10n_util::GetStringUTF16(IDS_DEFAULT_DOWNLOAD_FILENAME));

  if (download_info_.content_initiated &&
      download_info_.suggested_filename == default_filename) {
    // Start the download directly without asking.
    std::move(callback).Run(true /*allow*/);
    return;
  }

  web_view_permission_helper_->SetDownloadInformation(download_info_);
  web_view_permission_helper_->CanDownload(url, request_method,
                                           std::move(callback));
}

void WebViewGuest::RegisterProtocolHandler(
    content::RenderFrameHost* requesting_frame,
    const std::string& protocol,
    const GURL& url,
    bool user_gesture) {
  web_view_permission_helper_->RegisterProtocolHandler(
      requesting_frame, protocol, url, user_gesture);
}

bool WebViewGuest::IsVivaldiGuestView() {
  return true;
}

void WebViewGuest::VivaldiSanitizeUrl(GURL& url) {
  if (!IsVivaldiApp(owner_host())) {
    return;
  }

  // Is it a private window?
  if (!browser_context()->IsOffTheRecord()) {
    return;
  }

  // Is it a panel?
  if (!parent_tab_id_) {
    return;
  }

  // Is it panel-sensitive url?
  if (!IsPanelSensitiveUrl(url)) {
    return;
  }

  url = GURL("about:blank");
}
}  // namespace extensions
