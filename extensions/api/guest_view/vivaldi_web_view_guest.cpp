// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
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
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "net/cert/x509_certificate.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "browser/startup_vivaldi_browser.h"
#include "browser/vivaldi_browser_finder.h"
#include "browser/vivaldi_webcontents_util.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/guest_view/vivaldi_web_view_constants.h"
#include "extensions/helper/vivaldi_init_helpers.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

#if defined(USE_AURA)
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/window.h"
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

// local function copied from web_view_guest.cc
/*
std::string GetStoragePartitionIdFromPartitionConfig(
    const content::StoragePartitionConfig& storage_partition_config) {
  const auto& partition_id = storage_partition_config.partition_name();
  bool persist_storage = !storage_partition_config.in_memory();
  return (persist_storage ? webview::kPersistPrefix : "") + partition_id;
}*/

void ParsePartitionParam(const base::DictionaryValue& create_params,
                         std::string* storage_partition_id,
                         bool* persist_storage) {
  std::string partition_str;
  if (!create_params.GetString(webview::kStoragePartitionId, &partition_str)) {
    return;
  }

  // Since the "persist:" prefix is in ASCII, base::StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (base::StartsWith(partition_str,
                       "persist:", base::CompareCase::SENSITIVE)) {
    size_t index = partition_str.find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    *storage_partition_id = partition_str.substr(index + 1);

    if (storage_partition_id->empty()) {
      // TODO(lazyboy): Better way to deal with this error.
      return;
    }
    *persist_storage = true;
  } else {
    *storage_partition_id = partition_str;
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
      return "ignore";
  }
}

void SetAllowRunningInsecureContent(content::RenderFrameHost* frame) {
  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent>
      renderer;
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
    case ContentSettingsType::SOUND:
      return "sound";
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
  hide_timer_.Start(FROM_HERE, base::Milliseconds(TIME_BEFORE_HIDING_MS), this,
                    &CursorHider::Hide);
}

WebViewGuest::CursorHider::~CursorHider() {
  cursor_client_->ShowCursor();
}
#endif  // USE_AURA

void WebViewGuest::VivaldiSetLoadProgressEventExtraArgs(
    base::Value& dictionary) {
  if (!IsVivaldiRunning())
    return;
  const content::PageImpl& page =
      static_cast<const content::PageImpl&>(web_contents()->GetPrimaryPage());
  dictionary.SetDoubleKey(webview::kLoadedBytes, page.vivaldi_loaded_bytes());
  dictionary.SetIntKey(webview::kLoadedElements,
                       page.vivaldi_loaded_resources());
  dictionary.SetIntKey(webview::kTotalElements, page.vivaldi_total_resources());
}

void WebViewGuest::ToggleFullscreenModeForTab(
    content::WebContents* web_contents,
    bool enter_fullscreen) {
  if (enter_fullscreen == is_fullscreen_)
    return;
  is_fullscreen_ = enter_fullscreen;

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

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
  args->SetInteger("windowId", browser ? browser->session_id().id() : -1);
  args->SetBoolean("enterFullscreen", enter_fullscreen);
  DispatchEventToView(base::WrapUnique(
      new GuestViewEvent(webview::kEventOnFullscreen, std::move(args))));
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
    browser->window()->VivaldiShowWebsiteSettingsAt(profile, web_contents(),
                                                    url, pos);
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

void WebViewGuest::SetIsFullscreen(bool is_fullscreen) {
  SetFullscreenState(is_fullscreen);
  ToggleFullscreenModeForTab(web_contents(), is_fullscreen);
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
  add_types |= TabStripModel::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;
  if (inherit_opener) {
    add_types |= TabStripModel::ADD_INHERIT_OPENER;
  }

  NavigateParams navigate_params(
      browser, std::unique_ptr<WebContents>(guest->web_contents()));
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
    mixed_content_settings->AllowRunningOfInsecureContent(
        *web_contents()->GetOpener());
  }

  web_contents()->ForEachFrame(
      base::BindRepeating(&SetAllowRunningInsecureContent));
}

bool WebViewGuest::ShouldAllowRunningInsecureContent(WebContents* web_contents,
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
  TabModalConfirmDialog::Create(
      std::make_unique<RepostFormWarningController>(source), source);
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

std::unique_ptr<content::EyeDropper> WebViewGuest::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return ShowEyeDropper(frame, listener);
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

void WebViewGuest::VivaldiCreateWebContents(
    const base::DictionaryValue& create_params,
    WebContentsCreatedCallback callback) {
  RenderProcessHost* owner_render_process_host =
      owner_web_contents()->GetMainFrame()->GetProcess();
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
    std::move(callback).Run(nullptr);
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
    std::string new_url;
    if (create_params.GetString(webview::kNewURL, &new_url)) {
      guest_site = GURL(new_url);
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
  WebContents::CreateParams params(browser_context(),
                                   std::move(guest_site_instance));
  params.guest_delegate = this;

  WebContents* new_contents = nullptr;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  content::BrowserContext* context = browser_context();
  int tab_id;
  if (create_params.GetInteger("tab_id", &tab_id)) {
    // If we created the WebContents through CreateNewWindow and created this
    // guest with InitWithWebContents we cannot delete the tabstrip contents,
    // and we don't need to recreate the webcontents either. Just use the
    // WebContents owned by the tab-strip.
    content::WebContents* tabstrip_contents = NULL;
    bool include_incognito = true;
    Browser* browser;
    int tab_index;

    if (extensions::ExtensionTabUtil::GetTabById(
            tab_id, profile, include_incognito, &browser, NULL,
            &tabstrip_contents, &tab_index)) {
      new_contents = tabstrip_contents;
      content::WebContentsImpl* contentsimpl =
          static_cast<content::WebContentsImpl*>(new_contents);

      CreatePluginGuest(new_contents);

      // Set the owners blobregistry as fallback when accessing blob-urls.
      // VB-72847.
      StoragePartition* partition =
          contentsimpl->GetBrowserContext()->GetStoragePartition(
              contentsimpl->GetSiteInstance());

      StoragePartition* owner_partition =
          Profile::FromBrowserContext(owner_web_contents()->GetBrowserContext())
              ->GetStoragePartition(owner_web_contents()->GetSiteInstance());

      static_cast<content::StoragePartitionImpl*>(partition)
          ->UpdateBlobRegistryWithParentAsFallback(
              static_cast<content::StoragePartitionImpl*>(owner_partition));

      // Fire a WebContentsCreated event informing the client that script-
      // injection can be done.
      auto args = std::make_unique<base::DictionaryValue>();
      DispatchEventToView(std::make_unique<GuestViewEvent>(
          webview::kEventWebContentsCreated, std::move(args)));

    } else {
      // NOTE(jarle): Get the |new_contents| using the original Chromium way.
      // This makes Chrome apps work again, ref. VB-22908.

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
        guest_site_instance =
            content::SiteInstance::CreateForURL(browser_context(), guest_site);
      }
      WebContents::CreateParams params(browser_context(),
                                       std::move(guest_site_instance));
      params.guest_delegate = this;
      new_contents = WebContents::Create(params).release();
    }
  } else if (create_params.GetInteger("inspect_tab_id", &tab_id)) {
    // We want to attach this guest view to the already existing WebContents
    // currently used for DevTools.
    if (inspecting_tab_id_ == 0 || inspecting_tab_id_ != tab_id) {
      content::WebContents* inspected_contents =
          ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
              tab_id, Profile::FromBrowserContext(browser_context()));
      if (inspected_contents) {
        // NOTE(david@vivaldi.com): This returns always the |main_web_contents_|
        // which is required when the dev tools window is undocked.
        content::WebContents* devtools_contents =
            DevToolsWindow::GetDevtoolsWebContentsForInspectedWebContents(
                inspected_contents);
        if (devtools_contents == nullptr) {
          // This can happen when going between docked and undocked.
          DevToolsWindow::InspectElement(inspected_contents->GetMainFrame(), 0,
                                         0);
          devtools_contents =
              DevToolsWindow::GetDevtoolsWebContentsForInspectedWebContents(
                  inspected_contents);
        }
        // NOTE(david@vivaldi.com): Each docking state has it's own dedicated
        // webview now (VB-42802). We need to make sure that we attach this
        // guest view either to the already existing |toolbox_web_contents_|
        // wich is required for undocked dev tools or to the
        // |main_web_contents_| when docked. Each guest view will be reattached
        // after docking state was changed.
        std::string paramstr;
        DevToolsWindow* devWindow =
            DevToolsWindow::GetInstanceForInspectedWebContents(
                inspected_contents);
        if (create_params.GetString("name", &paramstr) &&
            !paramstr.find("vivaldi-devtools")) {
          DevToolsContentsResizingStrategy strategy;
          devtools_contents = DevToolsWindow::GetInTabWebContents(
              inspected_contents, &strategy);
        }
        DCHECK(devtools_contents);
        if (!devtools_contents) {
          // TODO(tomas@vivaldi.com): Band-aid for VB-48293
          return;
        }
        content::WebContentsImpl* contents =
            static_cast<content::WebContentsImpl*>(devtools_contents);

        DevtoolsConnectorAPI* api =
            DevtoolsConnectorAPI::GetFactoryInstance()->Get(
                Profile::FromBrowserContext(browser_context()));
        DCHECK(api);

        connector_item_ =
            base::WrapRefCounted<extensions::DevtoolsConnectorItem>(
                api->GetOrCreateDevtoolsConnectorItem(tab_id));
        DCHECK(connector_item_.get());
        connector_item_->set_guest_delegate(this);
        connector_item_->set_devtools_delegate(devWindow);

        new_contents = contents;
        CreatePluginGuest(new_contents);
        inspecting_tab_id_ = tab_id;
        SetAttachParams(create_params);
      }
    }
  } else {
    std::string window_id;
    if (create_params.GetString(webview::kWindowID, &window_id)) {
      int windowId = atoi(window_id.c_str());
      BrowserList* list = BrowserList::GetInstance();
      for (size_t i = 0; i < list->size(); i++) {
        if (list->get(i)->session_id().id() == windowId) {
          context = list->get(i)->profile();
          std::string src_string;
          if (create_params.GetString("src", &src_string)) {
            guest_site = GURL(src_string);
            guest_site_instance =
                content::SiteInstance::CreateForURL(context, guest_site);
          }
          break;
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

    if (IsVivaldiRunning()) {
      std::string view_name;
      if (create_params.GetString("vivaldi_view_type", &view_name)) {
        if (view_name == "extension_popup") {
          // 1. Create an ExtensionFrameHelper for the viewtype.
          // 2. take a WebContents as parameter.
          std::string src_string;
          if (create_params.GetString("src", &src_string)) {
            GURL popup_url = guest_site = GURL(src_string);

            scoped_refptr<content::SiteInstance> site_instance =
                ProcessManager::Get(context)->GetSiteInstanceForURL(popup_url);
            WebContents::CreateParams params(context, std::move(site_instance));
            params.guest_delegate = this;
            new_contents = WebContents::Create(params).release();
            extension_host_ = std::make_unique<::vivaldi::VivaldiExtensionHost>(
                context, popup_url, mojom::ViewType::kExtensionPopup,
                new_contents);
            task_manager::WebContentsTags::CreateForTabContents(new_contents);
          }
        }
      }
    }
    if (!new_contents) {
      WebContents::CreateParams params(context, std::move(guest_site_instance));
      params.guest_delegate = this;
      // TODO(erikchen): Fix ownership semantics for guest views.
      // https://crbug.com/832879.
      new_contents = WebContents::Create(params).release();
    }
    if (!new_contents) {
      WebContents::CreateParams params(context, std::move(guest_site_instance));
      params.guest_delegate = this;
      // TODO(erikchen): Fix ownership semantics for guest views.
      // https://crbug.com/832879.
      new_contents = WebContents::Create(params).release();
    }
  }
  DCHECK(new_contents);
  if (owner_web_contents()->IsAudioMuted()) {
    // NOTE(pettern@vivaldi.com): If the owner is muted it means the webcontents
    // of the AppWindow has been muted due to thumbnail capturing, so we also
    // mute the webview webcontents.
    chrome::SetTabAudioMuted(
        new_contents, true, TabMutedReason::EXTENSION,
        LastMuteMetadata::FromWebContents(owner_web_contents())->extension_id);
  }

  // Grant access to the origin of the embedder to the guest process. This
  // allows blob: and filesystem: URLs with the embedder origin to be created
  // inside the guest. It is possible to do this by running embedder code
  // through webview accessible_resources.
  //
  // TODO(dcheng): Is granting commit origin really the right thing to do here?
  content::ChildProcessSecurityPolicy::GetInstance()->GrantCommitOrigin(
      new_contents->GetMainFrame()->GetProcess()->GetID(),
      url::Origin::Create(GetOwnerSiteURL()));

  std::move(callback).Run(new_contents);
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
