// Copyright (c) 2016-2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/tabs/tabs_private_api.h"

#include <math.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/numerics/clamped_math.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/translate/vivaldi_translate_client.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/performance_controls/memory_saver_utils.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"  // nogncheck
#include "content/browser/web_contents/web_contents_impl.h"  // nogncheck
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/video_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/url_constants.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/cld_3/src/src/nnet_language_identifier.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/drag_utils.h"

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "browser/vivaldi_browser_finder.h"
#include "browser/startup_vivaldi_browser.h"
#include "components/capture/capture_page.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_pref_enums.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "prefs/vivaldi_tab_zoom_pref.h"
#include "ui/content/vivaldi_event_hooks.h"
#include "ui/content/vivaldi_tab_check.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

#if BUILDFLAG(IS_WIN)
#include "ui/display/win/screen_win.h"
#endif  // BUILDFLAG(IS_WIN)
#include "url/origin.h"

using content::WebContents;


#include "components/send_tab_to_self/send_tab_to_self_bridge.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"

#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace extensions {


namespace {

/// Converts the permission content_type to string matchable in our JS
/// customized version of PermissionUtil::GetPermissionString
std::string VivaldiGetPermissionString(
    ContentSettingsType content_type) {
  blink::PermissionType permission;
  bool success = permissions::PermissionUtil::GetPermissionType(content_type, &permission);
  DCHECK(success);

  // Fixup for permissions with non-matching strings
  switch (permission) {
  case blink::PermissionType::AUDIO_CAPTURE: return "microphone";
  case blink::PermissionType::MIDI: return "midi";
  case blink::PermissionType::MIDI_SYSEX: return "midi-sysex";
  case blink::PermissionType::IDLE_DETECTION: return "idle_detection";
  case blink::PermissionType::CLIPBOARD_READ_WRITE: return "clipboard";
  default:
    return blink::GetPermissionString(permission);
  }
}

} // namespace

const char kVivaldiTabZoom[] = "vivaldi_tab_zoom";
const char kVivaldiTabMuted[] = "vivaldi_tab_muted";
// Note. This flag is used in vivaldi_session_util.cc
// TODO: Get rid of this duplication.
const char kVivaldiWorkspace[] = "workspaceId";

const int& VivaldiPrivateTabObserver::kUserDataKey =
    VivaldiTabCheck::kVivaldiTabObserverContextKey;

namespace tabs_private = vivaldi::tabs_private;

bool IsTabMuted(const WebContents* web_contents) {
  std::string viv_extdata = web_contents->GetVivExtData();
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
      base::JSONReader::Read(viv_extdata, options);
  std::optional<bool> mute = std::nullopt;
  if (json && json->is_dict()) {
    mute = json->GetDict().FindBool(kVivaldiTabMuted);
  }
  return mute ? *mute : false;
}

bool IsTabInAWorkspace(const WebContents* web_contents) {
  return IsTabInAWorkspace(web_contents->GetVivExtData());
}

bool IsTabInAWorkspace(const std::string& viv_extdata) {
  return GetTabWorkspaceId(viv_extdata).has_value();
}

std::optional<double> GetTabWorkspaceId(const std::string& viv_extdata) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> json =
      base::JSONReader::Read(viv_extdata, options);
  std::optional<double> value;
  if (json && json->is_dict()) {
    value = json->GetDict().FindDouble(kVivaldiWorkspace);
  }
  return value;
}

Browser* GetWorkspaceBrowser(const double workspace_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == nullptr) {
      continue;
    }
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      auto tabWorkspaceId = GetTabWorkspaceId(web_contents->GetVivExtData());
      if (
        tabWorkspaceId.has_value() &&
        workspace_id == tabWorkspaceId.value()
      ) {
        return browser;
      }
    }
  }
  return nullptr;
}

int CountTabsInWorkspace(TabStripModel* tab_strip,
                         const double workspace_id) {
  int counter = 0;
  for (int i = 0; i < tab_strip->count(); ++i) {
    WebContents* web_contents = tab_strip->GetWebContentsAt(i);
    auto tabWorkspaceId = GetTabWorkspaceId(web_contents->GetVivExtData());
    if (tabWorkspaceId.has_value() && workspace_id == tabWorkspaceId.value()) {
      counter++;
    }
  }
  return counter;
}

base::Value::List getLinkRoutes(content::WebContents* contents) {
  Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  base::Value::List link_routes =
    prefs->GetList(vivaldiprefs::kWorkspacesLinkRoutes).Clone();
  return link_routes;
}

bool IsWorkspacesEnabled(content::WebContents* contents) {
  Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(vivaldiprefs::kWorkspacesEnabled);
}

namespace {

class JSDialogObserver : public javascript_dialogs::AppModalDialogObserver {
 public:
  JSDialogObserver() = default;

  // This is never called
  ~JSDialogObserver() override = default;

  static void Init() {
    // There could be at max one AppModalDialogObserver, so use a static
    // singleton for the implementation.
    static base::NoDestructor<JSDialogObserver> instance;
  }

  // javascript_dialogs::AppModalDialogObserver implementation
  void Notify(javascript_dialogs::AppModalDialogController* dialog) override {
    if (!dialog->is_before_unload_dialog())
      return;

    // We notify the UI which tab opened a beforeunload dialog so
    // it can make the tab active.
    int id = sessions::SessionTabHelper::IdForTab(dialog->web_contents()).id();
    Browser* browser = ::vivaldi::FindBrowserWithTab(dialog->web_contents());
    int window_id = 0;
    if (browser) {
      window_id = browser->session_id().id();
    }
    ::vivaldi::BroadcastEvent(
        vivaldi::window_private::OnBeforeUnloadDialogOpened::kEventName,
        vivaldi::window_private::OnBeforeUnloadDialogOpened::Create(window_id,
                                                                    id),
        dialog->web_contents()->GetBrowserContext());
  }
};

static const std::vector<tabs_private::TabAlertState> ConvertTabAlertState(
    const std::vector<TabAlertState>& states) {
  std::vector<tabs_private::TabAlertState> types;

  for (auto status : states) {
    switch (status) {
      case TabAlertState::MEDIA_RECORDING:
        types.push_back(tabs_private::TabAlertState::kRecording);
        break;
      case TabAlertState::TAB_CAPTURING:
        types.push_back(tabs_private::TabAlertState::kCapturing);
        break;
      case TabAlertState::AUDIO_PLAYING:
        types.push_back(tabs_private::TabAlertState::kPlaying);
        break;
      case TabAlertState::AUDIO_MUTING:
        types.push_back(tabs_private::TabAlertState::kMuting);
        break;
      case TabAlertState::BLUETOOTH_CONNECTED:
        types.push_back(tabs_private::TabAlertState::kBluetooth);
        break;
      case TabAlertState::USB_CONNECTED:
        types.push_back(tabs_private::TabAlertState::kUsb);
        break;
      case TabAlertState::PIP_PLAYING:
        types.push_back(tabs_private::TabAlertState::kPip);
        break;
      case TabAlertState::DESKTOP_CAPTURING:
        types.push_back(
            tabs_private::TabAlertState::kDesktopCapturing);
        break;
      case TabAlertState::VR_PRESENTING_IN_HEADSET:
        types.push_back(tabs_private::TabAlertState::kVrPresentingInHeadset);
        break;
      case TabAlertState::SERIAL_CONNECTED:
        types.push_back(
            tabs_private::TabAlertState::kSerialConnected);
        break;
      case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
        types.push_back(tabs_private::TabAlertState::kBluetoothScan);
        break;
      case TabAlertState::HID_CONNECTED:
        types.push_back(tabs_private::TabAlertState::kHidConnected);
        break;
      case TabAlertState::AUDIO_RECORDING:
        types.push_back(tabs_private::TabAlertState::kAudioRecording);
        break;
      case TabAlertState::VIDEO_RECORDING:
        types.push_back(tabs_private::TabAlertState::kVideoRecording);
        break;
    }
  }

  return types;
}

}  // namespace

class TabMutingHandler : public content_settings::Observer {
  using TabsAutoMutingValues = vivaldiprefs::TabsAutoMutingValues;

  const raw_ptr<Profile> profile_;
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  PrefChangeRegistrar prefs_registrar_;
  TabsAutoMutingValues muteRule_ = TabsAutoMutingValues::kOff;
  // NOTE(andre@vivaldi.com) : This is per profile so make sure the handler
  // takes this into account.
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      observer_{this};

  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override {
    if (content_type != ContentSettingsType::SOUND)
      return;

    UpdateMuting();
  }

  bool ContentSettingIsMuted(WebContents* web_contents) {
    GURL url = web_contents->GetLastCommittedURL();
    bool contentsetting_says_mute =
        host_content_settings_map_->GetContentSetting(
            url, url, ContentSettingsType::SOUND) == CONTENT_SETTING_BLOCK;
    return contentsetting_says_mute;
  }

  WebContents* FindActiveTabContentsInThisProfile() {
    BrowserList* browser_list = BrowserList::GetInstance();
    for (BrowserList::const_reverse_iterator browser_iterator =
             browser_list->begin_browsers_ordered_by_activation();
         browser_iterator != browser_list->end_browsers_ordered_by_activation();
         ++browser_iterator) {
      Browser* browser = *browser_iterator;
      // TODO: Make this into an utility-method.
      bool is_vivaldi_settings =
          (browser->is_vivaldi() &&
           static_cast<VivaldiBrowserWindow*>(browser->window())->type() ==
               VivaldiBrowserWindow::WindowType::SETTINGS);
      if (browser->profile()->GetOriginalProfile() == profile_ &&
          !is_vivaldi_settings) {
        return browser->tab_strip_model()->GetActiveWebContents();
      }
    }
    return nullptr;
  }

  void OnPrefsChanged(const std::string& path) {
    if (path == vivaldiprefs::kTabsAutoMuting) {
      UpdateMuting();
    }
  }

 public:
  TabMutingHandler(Profile* profile)
      : profile_(profile),
        host_content_settings_map_(
            HostContentSettingsMapFactory::GetForProfile(profile_)) {
    observer_.Observe(host_content_settings_map_);

    prefs_registrar_.Init(profile_->GetPrefs());
    // NOTE(andre@vivaldi.com) : Unretained is safe as this will live along
    // prefs_registrar_.
    prefs_registrar_.Add(vivaldiprefs::kTabsAutoMuting,
                         base::BindRepeating(&TabMutingHandler::OnPrefsChanged,
                                             base::Unretained(this)));
  }
  ~TabMutingHandler() override {}

  void NotifyTabSelectionChange(WebContents* active_contents) {
    UpdateMuting(active_contents);
  }

  // Called when a tabs audio-state might have changed, or when the active tab
  // is changed.
  void UpdateMuting(WebContents* active_contents = nullptr) {
    if (!active_contents) {
      active_contents = FindActiveTabContentsInThisProfile();
    }

    const TabsAutoMutingValues muteRule = static_cast<TabsAutoMutingValues>(
        profile_->GetPrefs()->GetInteger(vivaldiprefs::kTabsAutoMuting));
    // active muteRule
    if ((muteRule_ == TabsAutoMutingValues::kOff && (muteRule_ == muteRule)) ||
        !active_contents) {
      return;
    }
    muteRule_ = muteRule;
    RecentlyAudibleHelper* audible_helper =
        active_contents
            ? RecentlyAudibleHelper::FromWebContents(active_contents)
            : nullptr;

    bool active_is_audible =
        audible_helper ? audible_helper->WasRecentlyAudible() : false;

    for (Browser* browser : *BrowserList::GetInstance()) {
      if (browser->profile()->GetOriginalProfile() == profile_) {
        for (int i = 0, tab_count = browser->tab_strip_model()->count();
             i < tab_count; ++i) {
          WebContents* tab = browser->tab_strip_model()->GetWebContentsAt(i);
          if (!ContentSettingIsMuted(tab) && !IsTabMuted(tab)) {
            bool is_active = (tab == active_contents);
            bool mute = (muteRule_ != TabsAutoMutingValues::kOff);
            if (muteRule_ == TabsAutoMutingValues::kOnlyactive) {
              mute = !is_active;
            } else if (muteRule_ == TabsAutoMutingValues::kPrioritizeactive) {
              // Only unmute background tabs if the active is not audible.
              mute = (active_is_audible && !is_active);
            }
            tab->SetAudioMuted(mute);
          }
        }
      }
    }
  }
};

// static
TabsPrivateAPI* TabsPrivateAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  TabsPrivateAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  return api;
}

// static
BrowserContextKeyedAPIFactory<TabsPrivateAPI>*
TabsPrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<TabsPrivateAPI>>
      instance;
  return instance.get();
}

TabsPrivateAPI::TabsPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  tabmuting_handler_.reset(new TabMutingHandler(profile_));
}

TabsPrivateAPI::~TabsPrivateAPI() {}

void TabsPrivateAPI::UpdateMuting(
    content::WebContents* active_contents = nullptr) {
  tabmuting_handler_->UpdateMuting(active_contents);
}

void TabsPrivateAPI::NotifyTabSelectionChange(
    content::WebContents* active_contents) {
  // |active_contents| will be null if opening a settingswindow.
  if (active_contents) {
    tabmuting_handler_->NotifyTabSelectionChange(active_contents);
  }
}

void TabsPrivateAPI::NotifyTabChange(content::WebContents* web_contents) {
  if (!web_contents || !static_cast<content::WebContentsImpl*>(web_contents)
                            ->GetPrimaryMainFrame()
                            ->GetProcess()) {
    return;
  }
  // Sound state might have changed, check if any tabs should play or be muted.
  UpdateMuting();

  std::vector<tabs_private::TabAlertState> states =
      ConvertTabAlertState(GetTabAlertStatesForContents(web_contents));

  if (web_contents->IsBeingCaptured()) {
    states.push_back(tabs_private::TabAlertState::kCapturing);
  }

  int tabId = extensions::ExtensionTabUtil::GetTabId(web_contents);
  int windowId = extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents);

  ::vivaldi::BroadcastEvent(
      tabs_private::OnMediaStateChanged::kEventName,
      tabs_private::OnMediaStateChanged::Create(tabId, windowId, states),
      web_contents->GetBrowserContext());
}

// static
void TabsPrivateAPI::Init() {
  TabsPrivateAPI::GetFactoryInstance();
  JSDialogObserver::Init();
}

// ============================================================================

VivaldiPrivateTabObserver::VivaldiPrivateTabObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<VivaldiPrivateTabObserver>(*web_contents) {
  VivaldiTranslateClient* translate_client =
      VivaldiTranslateClient::FromWebContents(web_contents);
  if (translate_client) {
    translate_client->translate_driver()->AddTranslationObserver(this);
    translate_client->translate_driver()->AddLanguageDetectionObserver(this);
  }

  TabResourceUsageCollector::Get()->AddObserver(this);
}

VivaldiPrivateTabObserver::~VivaldiPrivateTabObserver() {
    TabResourceUsageCollector::Get()->RemoveObserver(this);
}

void VivaldiPrivateTabObserver::WebContentsDestroyed() {
  VivaldiTranslateClient* translate_client =
      VivaldiTranslateClient::FromWebContents(web_contents());
  if (translate_client) {
    translate_client->translate_driver()->RemoveTranslationObserver(this);
    translate_client->translate_driver()->RemoveLanguageDetectionObserver(this);
  }
}

void VivaldiPrivateTabObserver::OnTabResourceMetricsRefreshed() {
  int id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  uint64_t memory_usage;

  auto* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
          web_contents());

  bool not_yet_loaded =
      web_contents()->GetUserData(::vivaldi::kVivaldiStartupTabUserDataKey);

  bool was_discarded =
      web_contents()->WasDiscarded() ||
                       (tab_lifecycle_unit_external
                            ? tab_lifecycle_unit_external->GetTabState() ==
                                  ::mojom::LifecycleUnitState::DISCARDED
                                   : false) ||
      not_yet_loaded;

  // TODO: (andre@vivaldi.com) GetDiscardedMemorySavingsInBytes does not return
  // anything useful.
  if (was_discarded) {
    const auto* const pre_discard_resource_usage =
        performance_manager::user_tuning::UserPerformanceTuningManager::
            PreDiscardResourceUsage::FromWebContents(web_contents());
    memory_usage =
        pre_discard_resource_usage == nullptr
            ? 0
            : pre_discard_resource_usage->memory_footprint_estimate_kb() * 1024;

  } else {
    auto* const resource_tab_helper =
        TabResourceUsageTabHelper::FromWebContents(web_contents());
    memory_usage =
        (resource_tab_helper ? resource_tab_helper->GetMemoryUsageInBytes()
                             : 0);
  }

  tabs_private::TabPerformanceData info;
  info.tab_id = id;
  info.memory_usage = memory_usage;
  info.discarded = was_discarded;

  ::vivaldi::BroadcastEvent(
      tabs_private::OnTabResourceMetricsRefreshed::kEventName,
      tabs_private::OnTabResourceMetricsRefreshed::Create(info),
      web_contents()->GetBrowserContext());
}

/* static */
void VivaldiPrivateTabObserver::BroadcastTabInfo(
    tabs_private::UpdateTabInfo& info, content::WebContents* web_contents) {

  const SessionID sessionId = sessions::SessionTabHelper::IdForTab(web_contents);
  if (sessionId == SessionID::InvalidValue()) {
    // Not a tab, do we want to have a separate ui for this?
    return;
  }

  int id = sessionId.id();

  // todo: check sessionId.window_id()
  int windowId = extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents);

  ::vivaldi::BroadcastEvent(
      tabs_private::OnTabUpdated::kEventName,
      tabs_private::OnTabUpdated::Create(id, windowId, info),
      web_contents->GetBrowserContext());
}

const int kThemeColorBufferSize = 8;

void VivaldiPrivateTabObserver::DidChangeThemeColor() {
  std::optional<SkColor> theme_color = web_contents()->GetThemeColor();
  if (!theme_color)
    return;

  char rgb_buffer[kThemeColorBufferSize];
  base::snprintf(rgb_buffer, kThemeColorBufferSize, "#%02x%02x%02x",
                 SkColorGetR(*theme_color), SkColorGetG(*theme_color),
                 SkColorGetB(*theme_color));
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  ::vivaldi::BroadcastEvent(
      tabs_private::OnThemeColorChanged::kEventName,
      tabs_private::OnThemeColorChanged::Create(tab_id, rgb_buffer),
      web_contents()->GetBrowserContext());
}

bool ValueToJSONString(const base::Value& value, std::string& json_string) {
  JSONStringValueSerializer serializer(&json_string);
  return serializer.Serialize(value);
}

std::optional<base::Value> GetDictValueFromVivExtData(
    std::string& viv_extdata) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::optional<base::Value> value =
      base::JSONReader::Read(viv_extdata, options);
  if (value && value->is_dict()) {
    return value;
  }
  return std::nullopt;
}

bool SetTabWorkspaceId(content::WebContents* contents, double workspace_id) {
  auto viv_ext_data = contents->GetVivExtData();
  std::optional<base::Value> json =
    GetDictValueFromVivExtData(viv_ext_data);
  if (!json) {
    return false;
  }
  std::optional<double> value = json->GetDict().FindDouble(kVivaldiWorkspace);
  if (value.has_value() && value.value() == workspace_id) {
    return false;
  }
  json->GetDict().Set(kVivaldiWorkspace, workspace_id);
  json->GetDict().Remove("group");
  std::string json_string;
  if (!ValueToJSONString(*json, json_string)) {
    return false;
  }
  contents->SetVivExtData(json_string);
  return true;
}

void VivaldiPrivateTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  SetContentsMimeType(web_contents()->GetContentsMimeType());
  tabs_private::UpdateTabInfo info;
  info.mime_type = contents_mime_type();
  BroadcastTabInfo(info, web_contents());
}

void VivaldiPrivateTabObserver::GetAccessKeys(JSAccessKeysCallback callback) {
  DCHECK(callback);
  auto* rfhi = static_cast<content::RenderFrameHostImpl*>(
      web_contents()->GetPrimaryMainFrame());
  rfhi->GetVivaldiFrameService()->GetAccessKeysForPage(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&VivaldiPrivateTabObserver::AccessKeysReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          std::vector<::vivaldi::mojom::AccessKeyPtr>()));
}

void VivaldiPrivateTabObserver::AccessKeysReceived(
    JSAccessKeysCallback callback,
    std::vector<::vivaldi::mojom::AccessKeyPtr> access_keys) {
  std::move(callback).Run(std::move(access_keys));
}

void VivaldiPrivateTabObserver::AccessKeyAction(std::string access_key) {
  auto* rfh = static_cast<content::RenderFrameHostImpl*>(
      web_contents()->GetPrimaryMainFrame());
  rfh->GetVivaldiFrameService()->AccessKeyAction(access_key);
}

void VivaldiPrivateTabObserver::MoveSpatnavRect(
    ::vivaldi::mojom::SpatnavDirection direction,
    JSSpatnavRectCallback callback) {
  auto* rfhi = static_cast<content::RenderFrameHostImpl*>(
      web_contents()->GetPrimaryMainFrame());
  rfhi->GetVivaldiFrameService()->MoveSpatnavRect(
      direction,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&VivaldiPrivateTabObserver::SpatnavRectReceived,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          ::vivaldi::mojom::SpatnavRect::New()));
}

void VivaldiPrivateTabObserver::SpatnavRectReceived(
    JSSpatnavRectCallback callback,
    ::vivaldi::mojom::SpatnavRectPtr rect) {
  std::move(callback).Run(std::move(rect));
}

void VivaldiPrivateTabObserver::DetermineTextLanguage(
    const std::string& text,
    JSDetermineTextLanguageCallback callback) {
  DCHECK(callback);
  auto* rfhi = static_cast<content::RenderFrameHostImpl*>(
      web_contents()->GetPrimaryMainFrame());
  rfhi->GetVivaldiFrameService()->DetermineTextLanguage(
      text,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&VivaldiPrivateTabObserver::DetermineTextLanguageDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          chrome_lang_id::NNetLanguageIdentifier::kUnknown));
}

void VivaldiPrivateTabObserver::DetermineTextLanguageDone(
    JSDetermineTextLanguageCallback callback,
    const std::string& langCode) {
  std::move(callback).Run(std::move(langCode));
}

void VivaldiPrivateTabObserver::OnPermissionAccessed(
    ContentSettingsType content_settings_type,
    std::string origin,
    ContentSetting content_setting) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());

  std::string type_name = base::ToLowerASCII(
      VivaldiGetPermissionString(content_settings_type));

  std::string setting;
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      setting = "allow";
      break;
    case CONTENT_SETTING_ASK:
      setting = "ask";
      break;
    case CONTENT_SETTING_BLOCK:
      setting = "block";
      break;
    case CONTENT_SETTING_DEFAULT:
    default:
      setting = "default";
      break;
  }

  ::vivaldi::BroadcastEvent(tabs_private::OnPermissionAccessed::kEventName,
                            tabs_private::OnPermissionAccessed::Create(
                                tab_id, type_name, origin, setting),
                            web_contents()->GetBrowserContext());
}

void VivaldiPrivateTabObserver::WebContentsDidDetach() {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  ::vivaldi::BroadcastEvent(
      tabs_private::OnTabIsDetached::kEventName,
      tabs_private::OnTabIsDetached::Create(
          tab_id, ExtensionTabUtil::GetWindowIdOfTab(web_contents())),
      web_contents()->GetBrowserContext());
}

void VivaldiPrivateTabObserver::WebContentsDidAttach() {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  ::vivaldi::BroadcastEvent(
      tabs_private::OnTabIsAttached::kEventName,
      tabs_private::OnTabIsAttached::Create(
          tab_id, ExtensionTabUtil::GetWindowIdOfTab(web_contents())),
      web_contents()->GetBrowserContext());
}

void VivaldiPrivateTabObserver::BeforeUnloadFired(bool proceed) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  Browser* browser = ::vivaldi::FindBrowserWithTab(web_contents());
  int window_id = browser->session_id().id();

  ::vivaldi::BroadcastEvent(
      tabs_private::OnBeforeUnloadDialogClosed::kEventName,
      tabs_private::OnBeforeUnloadDialogClosed::Create(window_id, tab_id,
                                                       proceed),
      web_contents()->GetBrowserContext());
}

VivaldiPrivateTabObserver* VivaldiPrivateTabObserver::FromTabId(
    content::BrowserContext* browser_context,
    int tab_id,
    std::string* error) {
  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, browser_context,
                                                      error);
  if (!tabstrip_contents)
    return nullptr;

  VivaldiPrivateTabObserver* observer =
      VivaldiPrivateTabObserver::FromWebContents(tabstrip_contents);
  if (!observer) {
    *error = "Cannot locate VivaldiPrivateTabObserver for tab " +
             std::to_string(tab_id);
    return nullptr;
  }

  return observer;
}

void VivaldiPrivateTabObserver::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  TabsPrivateAPI::FromBrowserContext(web_contents()->GetBrowserContext())
      ->UpdateMuting();
}

// translate::ContentTranslateDriver::Observer implementation
void VivaldiPrivateTabObserver::OnLanguageDetermined(
    const translate::LanguageDetectionDetails& details) {
  VivaldiTranslateClient* translate_client =
      VivaldiTranslateClient::FromWebContents(web_contents());
  if (translate_client && !translate_client->IsTranslatableURL(details.url)) {
    return;
  }
  extensions::vivaldi::tabs_private::LanguageDetectionDetails lang_details;

  lang_details.url = details.url.spec();
  lang_details.content_language = details.content_language;
  lang_details.cld_language = details.model_detected_language;
  lang_details.is_cld_reliable = details.is_model_reliable;
  lang_details.has_no_translate = details.has_notranslate;
  lang_details.html_root_language = details.html_root_language;
  lang_details.adopted_language = details.adopted_language;

  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  if (tab_id) {
    ::vivaldi::BroadcastEvent(
        extensions::vivaldi::tabs_private::OnLanguageDetermined::kEventName,
        extensions::vivaldi::tabs_private::OnLanguageDetermined::Create(
            tab_id, std::move(lang_details)),
        web_contents()->GetBrowserContext());
  }
}

void VivaldiPrivateTabObserver::ActivateTab(content::WebContents* contents) {
  Browser* browser = ::vivaldi::FindBrowserWithTab(contents);
  if (!browser) {
    return;
  }
  int index = browser->tab_strip_model()->GetIndexOfWebContents(contents);
  if (index == TabStripModel::kNoTab) {
    return;
  }
  browser->tab_strip_model()->ActivateTabAt(index);
  browser->window()->Activate();
}

void VivaldiPrivateTabObserver::OnPageTranslated(
    const std::string& original_lang,
    const std::string& translated_lang,
    translate::TranslateErrors error_type) {
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  if (tab_id) {
    ::vivaldi::BroadcastEvent(
        extensions::vivaldi::tabs_private::OnPageTranslated::kEventName,
        extensions::vivaldi::tabs_private::OnPageTranslated::Create(
            tab_id, original_lang, translated_lang,
            ToVivaldiTranslateError(error_type)),
        web_contents()->GetBrowserContext());
  }
}

void VivaldiPrivateTabObserver::OnIsPageTranslatedChanged(
    content::WebContents* source) {
  VivaldiTranslateClient* translate_client =
      VivaldiTranslateClient::FromWebContents(web_contents());
  if (!translate_client) {
    return;
  }
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  bool isTranslated = translate_client->GetLanguageState().IsPageTranslated();

  // OnPageTranslated is typically the main event, but it's not fired when
  // revert has been used, so the client will need this event to know.
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::tabs_private::OnIsPageTranslatedChanged::kEventName,
      extensions::vivaldi::tabs_private::OnIsPageTranslatedChanged::Create(
          tab_id, isTranslated),
      web_contents()->GetBrowserContext());
}

void VivaldiPrivateTabObserver::CaptureStarted() {
  TabsPrivateAPI::FromBrowserContext(web_contents()->GetBrowserContext())
      ->NotifyTabChange(web_contents());
}

void VivaldiPrivateTabObserver::CaptureFinished() {
  TabsPrivateAPI::FromBrowserContext(web_contents()->GetBrowserContext())
      ->NotifyTabChange(web_contents());
}

void VivaldiPrivateTabObserver::MediaPictureInPictureChanged(
    bool is_picture_in_picture) {
  content::PictureInPictureWindowController* pip_controller =
      content::PictureInPictureWindowController::
          GetOrCreateVideoPictureInPictureController(web_contents());
  DCHECK(pip_controller);
  pip_controller->SetVivaldiDelegate(weak_ptr_factory_.GetWeakPtr());
}

///////////////////////////////////////////////////////////////////////////////

namespace {

::vivaldi::mojom::VivaldiFrameService* GetPrimaryFrameService(
    ExtensionFunction* function,
    int tab_id,
    std::string* error) {
  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          tab_id, function->browser_context(), error);
  if (!tabstrip_contents)
    return nullptr;

  auto* primary_frame = static_cast<content::RenderFrameHostImpl*>(
      tabstrip_contents->GetPrimaryMainFrame());
  return primary_frame->GetVivaldiFrameService().get();
}

}  // namespace

ExtensionFunction::ResponseAction TabsPrivateUpdateFunction::Run() {
  using tabs_private::Update::Params;
  namespace Results = tabs_private::Update::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_private::UpdateTabInfo* info = &params->tab_info;
  std::string error;
  VivaldiGuestViewContentObserver* guestviewcontentobserver =
      VivaldiGuestViewContentObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!guestviewcontentobserver) {
    return RespondNow(Error(error));
  }

  if (info->show_images.has_value()) {
    guestviewcontentobserver->SetShowImages(info->show_images.value());
  }
  if (info->load_from_cache_only.has_value()) {
    guestviewcontentobserver->SetLoadFromCacheOnly(
        info->load_from_cache_only.value());
  }
  if (info->mute_tab.has_value()) {
    guestviewcontentobserver->SetMuted(info->mute_tab.value());
  }
  // Set the prefs.
  guestviewcontentobserver->CommitSettings();

  // Inform the JS layer.
  VivaldiPrivateTabObserver::BroadcastTabInfo(*info, guestviewcontentobserver->web_contents());

  return RespondNow(ArgumentList(Results::Create(*info)));
}

ExtensionFunction::ResponseAction TabsPrivateGetFunction::Run() {
  using tabs_private::Get::Params;
  namespace Results = tabs_private::Get::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  VivaldiGuestViewContentObserver* tab_api =
      VivaldiGuestViewContentObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));

  tabs_private::UpdateTabInfo info;
  info.show_images = tab_api->show_images();
  info.load_from_cache_only = tab_api->load_from_cache_only();
  return RespondNow(ArgumentList(Results::Create(info)));
}

ExtensionFunction::ResponseAction TabsPrivateInsertTextFunction::Run() {
  using tabs_private::InsertText::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  ::vivaldi::mojom::VivaldiFrameService* frame_service =
      GetPrimaryFrameService(this, params->tab_id, &error);
  if (!frame_service)
    return RespondNow(Error(error));

  frame_service->InsertText(params->text);

  return RespondNow(NoArguments());
}

TabsPrivateStartDragFunction::TabsPrivateStartDragFunction() = default;
TabsPrivateStartDragFunction::~TabsPrivateStartDragFunction() = default;

ExtensionFunction::ResponseAction TabsPrivateStartDragFunction::Run() {
  using tabs_private::StartDrag::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  SessionID::id_type window_id = params->drag_data.window_id;
  VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromId(window_id);
  if (!window) {
    return RespondNow(Error("No such window"));
  }

  const std::u16string identifier(
      base::UTF8ToUTF16(params->drag_data.mime_type));
  const std::u16string custom_data(
      base::UTF8ToUTF16(params->drag_data.custom_data));

  drop_data_.custom_data.emplace(identifier, custom_data);

  drop_data_.url = GURL(params->drag_data.url);
  drop_data_.url_title = base::UTF8ToUTF16(params->drag_data.title);

  event_info_.source =
      (params->drag_data.is_from_touch && *params->drag_data.is_from_touch)
          ? ui::mojom::DragEventSource::kTouch
          : ui::mojom::DragEventSource::kMouse;
  event_info_.location = display::Screen::GetScreen()->GetCursorScreenPoint();

  image_offset_.set_x(params->drag_data.cursor_x);
  image_offset_.set_y(params->drag_data.cursor_y);

  const url::Origin source_origin = url::Origin::Create(drop_data_.url);

  double width = params->drag_data.width;
  double height = params->drag_data.height;
  if (width <= 0.0 || height <= 0.0 || width > 10000.0 || height > 10000)
    return RespondNow(Error("Invalid image size"));

  if (params->drag_data.image_data) {
    int w = static_cast<int>(width);
    int h = static_cast<int>(height);
    int size = base::ClampedNumeric<int>(w) * base::ClampedNumeric<int>(h) * 4;
    if (static_cast<size_t>(size) != params->drag_data.image_data->size())
      return RespondNow(Error("Invalid image bitmap size"));
    if (kN32_SkColorType != kRGBA_8888_SkColorType) {
      static_assert(kN32_SkColorType == kBGRA_8888_SkColorType ||
                        kN32_SkColorType == kRGBA_8888_SkColorType,
                    "only two native orders exists");
      // The native order is BGRA and we must use that to construct SkBitmap
      // that is passed to gfx::ImageSkia(). Swap red and blue.
      // TODO(igor@vivaldi.com): Find out if there is a utility in Chromium for
      // doing this conversion in place. SkOpts::RGBA_to_BGRA() is not suitable
      // as it does the conversion when copying the bytes.
      uint8_t* p = params->drag_data.image_data->data();
      for (uint8_t* end = p + size; p < end; p += 4) {
        std::swap(p[0], p[2]);
      }
    }
    SkBitmap* bitmap = new SkBitmap;
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(w, h);

    // SkBitmap::installPixels takes ownership of data and calls the release
    // callback to delete them in all cases including on errors.
    using ImageVector = std::vector<uint8_t>;
    ImageVector raw_image = params->drag_data.image_data.value();
    auto release_pixels = [](void* addr, void* context) {
      // Let unique_ptr to call delete.
      std::unique_ptr<ImageVector> image_data(
          static_cast<ImageVector*>(context));
      CHECK(addr == image_data->data());
    };
    bool success = bitmap->installPixels(image_info, raw_image.data(),
                                         image_info.minRowBytes(),
                                         release_pixels, &raw_image);
    OnCaptureDone(window_id, source_origin, success, 1.0, *bitmap);
    return AlreadyResponded();
  }

  double x = params->drag_data.pos_x ? *params->drag_data.pos_x : 0.0;
  double y = params->drag_data.pos_y ? *params->drag_data.pos_y : 0.0;
  gfx::RectF rect(x, y, width, height);
  ::vivaldi::FromUICoordinates(window->web_contents(), &rect);
  ::vivaldi::CapturePage::CaptureVisible(
      window->web_contents(), rect,
      base::BindOnce(&TabsPrivateStartDragFunction::OnCaptureDone, this,
                     window_id, source_origin));
  return RespondLater();
}

void TabsPrivateStartDragFunction::OnCaptureDone(SessionID::id_type window_id,
                                                 const url::Origin source_origin,
                                                 bool success,
                                                 float device_scale_factor,
                                                 const SkBitmap& bitmapref) {
  SkBitmap bitmap = bitmapref;
  do {
    if (!success)
      break;

    VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromId(window_id);
    if (!window) {
      // The capture finished after the user closed the browser window.
      LOG(ERROR) << "No such window";
      break;
    }

    content::RenderViewHostImpl* rvh =
        static_cast<content::RenderViewHostImpl*>(
            window->web_contents()->GetRenderViewHost());
    if (!rvh) {
      LOG(ERROR) << "RenderViewHostImpl is null";
      break;
    }

    content::RenderViewHostDelegateView* view =
        rvh->GetDelegate()->GetDelegateView();
    blink::DragOperationsMask allowed_ops =
        static_cast<blink::DragOperationsMask>(ui::mojom::DragOperation::kMove);
    gfx::ImageSkia image(gfx::ImageSkiaRep(bitmap, device_scale_factor));

    // On Linux and Windows StartDragging is synchronous, so enable tab dragging
    // mode before calling it.
    ::vivaldi::SetTabDragInProgress(true);
    view->StartDragging(drop_data_, source_origin, allowed_ops, image,
                        image_offset_, {},
                        event_info_, rvh->GetWidget());
  } while (false);

  Respond(NoArguments());
}

ExtensionFunction::ResponseAction TabsPrivateScrollPageFunction::Run() {
  using tabs_private::ScrollPage::Params;
  namespace Results = tabs_private::ScrollPage::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<::vivaldi::mojom::ScrollType> scroll_type;
  static const std::pair<std::string_view, ::vivaldi::mojom::ScrollType>
      scroll_type_names[] = {
          {"up", ::vivaldi::mojom::ScrollType::kUp},
          {"down", ::vivaldi::mojom::ScrollType::kDown},
          {"left", ::vivaldi::mojom::ScrollType::kLeft},
          {"right", ::vivaldi::mojom::ScrollType::kRight},
          {"pageup", ::vivaldi::mojom::ScrollType::kPageUp},
          {"pagedown", ::vivaldi::mojom::ScrollType::kPageDown},
          {"pageleft", ::vivaldi::mojom::ScrollType::kPageLeft},
          {"pageright", ::vivaldi::mojom::ScrollType::kPageRight},
          {"top", ::vivaldi::mojom::ScrollType::kTop},
          {"bottom", ::vivaldi::mojom::ScrollType::kBottom},
      };
  for (const auto& name_value : scroll_type_names) {
    if (name_value.first == params->scroll_type) {
      scroll_type = name_value.second;
    }
  }
  if (!scroll_type)
    return RespondNow(Error("Unknown scroll_type - " + params->scroll_type));

  std::string error;
  ::vivaldi::mojom::VivaldiFrameService* frame_service =
      GetPrimaryFrameService(this, params->tab_id, &error);
  if (!frame_service)
    return RespondNow(Error(error));

  int scroll_amount = (params->scroll_amount) ? *(params->scroll_amount) : 0;
  frame_service->ScrollPage(*scroll_type, scroll_amount);
  return RespondNow(ArgumentList(Results::Create()));
}

void TabsPrivateMoveSpatnavRectFunction::SpatnavRectReceived(
    ::vivaldi::mojom::SpatnavRectPtr rect) {
  namespace Results = tabs_private::MoveSpatnavRect::Results;
  tabs_private::NavigationRect results;

  results.left = rect->x;
  results.right = rect->x + rect->width;
  results.top = rect->y;
  results.bottom = rect->y + rect->height;
  results.width = rect->width;
  results.height = rect->height;
  results.href = rect->href;

  Respond(ArgumentList(Results::Create(results)));
}

ExtensionFunction::ResponseAction TabsPrivateMoveSpatnavRectFunction::Run() {
  using tabs_private::MoveSpatnavRect::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ::vivaldi::mojom::SpatnavDirection dir;
  switch (params->direction) {
    case vivaldi::tabs_private::NavigationDirection::kLeft:
      dir = ::vivaldi::mojom::SpatnavDirection::kLeft;
      break;
    case vivaldi::tabs_private::NavigationDirection::kRight:
      dir = ::vivaldi::mojom::SpatnavDirection::kRight;
      break;
    case vivaldi::tabs_private::NavigationDirection::kUp:
      dir = ::vivaldi::mojom::SpatnavDirection::kUp;
      break;
    case vivaldi::tabs_private::NavigationDirection::kDown:
      dir = ::vivaldi::mojom::SpatnavDirection::kDown;
      break;
    default:
      dir = ::vivaldi::mojom::SpatnavDirection::kNone;
  }

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));
  tab_api->MoveSpatnavRect(
      dir, base::BindOnce(
               &TabsPrivateMoveSpatnavRectFunction::SpatnavRectReceived, this));
  return RespondLater();
}

ExtensionFunction::ResponseAction
TabsPrivateActivateSpatnavElementFunction::Run() {
  using tabs_private::ActivateSpatnavElement::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int modifiers = params->modifiers;

  std::string error;
  ::vivaldi::mojom::VivaldiFrameService* frame_service =
      GetPrimaryFrameService(this, params->tab_id, &error);
  if (!frame_service)
    return RespondNow(Error(error));

  frame_service->ActivateSpatnavElement(modifiers);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
TabsPrivateCloseSpatnavOrCurrentOpenMenuFunction::Run() {
  using tabs_private::CloseSpatnavOrCurrentOpenMenu::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  ::vivaldi::mojom::VivaldiFrameService* frame_service =
      GetPrimaryFrameService(this, params->tab_id, &error);
  if (!frame_service)
    return RespondNow(Error(error));

  frame_service->CloseSpatnavOrCurrentOpenMenu(base::BindOnce(
      &TabsPrivateCloseSpatnavOrCurrentOpenMenuFunction::CloseSpatnavDone,
      this));
  return RespondLater();
}

void TabsPrivateCloseSpatnavOrCurrentOpenMenuFunction::CloseSpatnavDone(
    bool layout_changed,
    bool current_element_valid) {
  namespace Results = tabs_private::CloseSpatnavOrCurrentOpenMenu::Results;
  vivaldi::tabs_private::CloseSpatnavParams params;
  params.layout_changed = layout_changed;
  params.current_element_valid = current_element_valid;

  return Respond(ArgumentList(Results::Create(params)));
}

ExtensionFunction::ResponseAction
TabsPrivateHasBeforeUnloadOrUnloadFunction::Run() {
  using tabs_private::HasBeforeUnloadOrUnload::Params;
  namespace Results = tabs_private::HasBeforeUnloadOrUnload::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), nullptr);
  return RespondNow(ArgumentList(Results::Create(
      contents && contents->NeedToFireBeforeUnloadOrUnloadEvents())));
}

ExtensionFunction::ResponseAction TabsPrivateTranslatePageFunction::Run() {
  using tabs_private::TranslatePage::Params;
  namespace Results = tabs_private::TranslatePage::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string source = params->src_lang;
  std::string dest = params->dest_lang;

  if (dest.empty()) {
    return RespondNow(Error("Missing destination language"));
  }
  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), nullptr);

  std::unique_ptr<translate::TranslateUIDelegate> ui_delegate =
      std::make_unique<translate::TranslateUIDelegate>(
          VivaldiTranslateClient::GetManagerFromWebContents(contents)
              ->GetWeakPtr(),
          source, dest);
  ui_delegate->Translate();

  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
TabsPrivateRevertTranslatePageFunction::Run() {
  using tabs_private::RevertTranslatePage::Params;
  namespace Results = tabs_private::RevertTranslatePage::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), nullptr);

  translate::TranslateManager* translate_manager =
      VivaldiTranslateClient::GetManagerFromWebContents(contents);
  if (translate_manager) {
    translate_manager->RevertTranslation();
  }
  return RespondNow(ArgumentList(Results::Create(true)));
}

ExtensionFunction::ResponseAction
TabsPrivateDetermineTextLanguageFunction::Run() {
  using tabs_private::DetermineTextLanguage::Params;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));
  tab_api->DetermineTextLanguage(
      params->text,
      base::BindOnce(
          &TabsPrivateDetermineTextLanguageFunction::DetermineTextLanguageDone,
          this));

  return RespondLater();
}

void TabsPrivateDetermineTextLanguageFunction::DetermineTextLanguageDone(
    const std::string& langCode) {
  namespace Results = tabs_private::DetermineTextLanguage::Results;

  Respond(ArgumentList(Results::Create(langCode)));
}

ExtensionFunction::ResponseAction
TabsPrivateLoadViaLifeCycleUnitFunction::Run() {
  using tabs_private::LoadViaLifeCycleUnit::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  content::WebContents* tab_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), nullptr);

  for (resource_coordinator::LifecycleUnit* lifecycle_unit :
       g_browser_process->GetTabManager()->GetSortedLifecycleUnits()) {
    resource_coordinator::TabLifecycleUnitExternal*
        tab_lifecycle_unit_external =
            lifecycle_unit->AsTabLifecycleUnitExternal();
    if (tab_lifecycle_unit_external->GetWebContents() == tab_contents) {
      lifecycle_unit->Load();
      break;
    }
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
TabsPrivateGetTabPerformanceDataFunction::Run() {
  using tabs_private::GetTabPerformanceData::Params;
  namespace Results = tabs_private::GetTabPerformanceData::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const int tabcount = params->tab_ids.size();
  std::vector<tabs_private::TabPerformanceData> data(tabcount);

  for (int i = 0; i < tabcount; i++) {

    content::WebContents* tab_contents =
        ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
            params->tab_ids[i], browser_context(), nullptr);

    if (tab_contents) {

      tabs_private::TabPerformanceData* pdata = &data[i];
      pdata->tab_id = params->tab_ids[i];

      auto* tab_lifecycle_unit_external =
          resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
              tab_contents);

      bool notYetLoaded = tab_contents->GetUserData(
                  ::vivaldi::kVivaldiStartupTabUserDataKey);

      pdata->discarded = tab_contents->WasDiscarded() ||
                         tab_lifecycle_unit_external->GetTabState() ==
                             ::mojom::LifecycleUnitState::DISCARDED ||
                         notYetLoaded;

      if (pdata->discarded) {
        const auto* const pre_discard_resource_usage =
            performance_manager::user_tuning::UserPerformanceTuningManager::
                PreDiscardResourceUsage::FromWebContents(tab_contents);
        pdata->memory_usage = pre_discard_resource_usage == nullptr
                   ? 0
                   : pre_discard_resource_usage->memory_footprint_estimate_kb() *
                         1024;
      } else {
        auto* const resource_tab_helper =
            TabResourceUsageTabHelper::FromWebContents(tab_contents);
        pdata->memory_usage = (resource_tab_helper->GetMemoryUsageInBytes());
      }
    }

  }
  return RespondNow(ArgumentList(Results::Create(data)));
}

/// <summary>
/// VivaldiGuestViewContentObserver will make sure all renderer prefs are
/// updated when a renderer is created and navigates.
/// </summary>

VivaldiGuestViewContentObserver::VivaldiGuestViewContentObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents),
      content::WebContentsUserData<VivaldiGuestViewContentObserver>(
          *web_contents) {
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  if (zoom_controller) {
    zoom_controller->AddObserver(this);
  }
  prefs_registrar_.Init(
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs());

  prefs_registrar_.Add(
      vivaldiprefs::kWebpagesFocusTrap,
      base::BindRepeating(&VivaldiGuestViewContentObserver::OnPrefsChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

VivaldiGuestViewContentObserver::~VivaldiGuestViewContentObserver() {}

void VivaldiGuestViewContentObserver::WebContentsDestroyed() {}

void VivaldiGuestViewContentObserver::OnPrefsChanged(const std::string& path) {
  if (path == vivaldiprefs::kWebpagesFocusTrap) {
    UpdateAllowTabCycleIntoUI();
    CommitSettings();
  }
}

void VivaldiGuestViewContentObserver::CommitSettings() {
  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);

  // We must update from system settings otherwise many settings would fallback
  // to default values when syncing below.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(render_prefs, profile);

  web_contents()->SyncRendererPrefs();
  web_contents()->OnWebPreferencesChanged();
}

void VivaldiGuestViewContentObserver::UpdateAllowTabCycleIntoUI() {
  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  render_prefs->allow_tab_cycle_from_webpage_into_ui =
      !profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesFocusTrap);
}

void VivaldiGuestViewContentObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  std::string viv_ext_data = web_contents()->GetVivExtData();
  std::optional<base::Value> json = GetDictValueFromVivExtData(viv_ext_data);
  if (::vivaldi::IsTabZoomEnabled(web_contents())) {
    std::optional<double> zoom =
        json ? json->GetDict().FindDouble(kVivaldiTabZoom) : std::nullopt;
    if (zoom) {
      tab_zoom_level_ = *zoom;
    } else {
      content::HostZoomMap* host_zoom_map =
          content::HostZoomMap::GetDefaultForBrowserContext(
              web_contents()->GetBrowserContext());
      tab_zoom_level_ = host_zoom_map->GetDefaultZoomLevel();
    }
  }

  // This is not necessary for each RVH-change. And only set when the tab is
  // marked as muted to avoid interfering with site-settings.
  if (IsTabMuted(web_contents())) {
    SetMuted(true);
  }

  SetLoadFromCacheOnly(load_from_cache_only_);
  UpdateAllowTabCycleIntoUI();
  CommitSettings();

  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (::vivaldi::IsVivaldiApp(site.host())) {
    auto* security_policy = content::ChildProcessSecurityPolicy::GetInstance();
    int process_id = render_frame_host->GetProcess()->GetID();
    security_policy->GrantRequestScheme(process_id, url::kFileScheme);
    security_policy->GrantRequestScheme(process_id, content::kViewSourceScheme);
  }
}

void VivaldiGuestViewContentObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if (::vivaldi::IsTabZoomEnabled(web_contents())) {
    content::HostZoomMap* host_zoom_map_ =
        content::HostZoomMap::GetForWebContents(web_contents());
    auto* rfhi = static_cast<content::RenderFrameHostImpl*>(
        web_contents()->GetPrimaryMainFrame());
    if (rfhi) {
      content::GlobalRenderFrameHostId rfh_id = rfhi->GetGlobalId();
      host_zoom_map_->SetTemporaryZoomLevel(rfh_id, tab_zoom_level_);
    }
  }

  // Set the renderer prefs on the new RenderViewHost too.
  SetLoadFromCacheOnly(load_from_cache_only_);
  UpdateAllowTabCycleIntoUI();
  CommitSettings();
}

void VivaldiGuestViewContentObserver::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  DCHECK(zoom_controller);
  zoom_controller->RemoveObserver(this);
}

void VivaldiGuestViewContentObserver::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  content::WebContents* web_contents = data.web_contents;
  content::StoragePartition* current_partition =
      web_contents->GetBrowserContext()->GetStoragePartition(
          web_contents->GetSiteInstance(), false);
  if (!::vivaldi::IsTabZoomEnabled(web_contents) || tab_zoom_level_ == -1) {
    return;
  }

  if (current_partition &&
      current_partition ==
          web_contents->GetBrowserContext()->GetDefaultStoragePartition())
    SetZoomLevelForTab(data.new_zoom_level, data.old_zoom_level);
}

void VivaldiGuestViewContentObserver::SetZoomLevelForTab(double new_level,
                                                   double old_level) {
  // Only update zoomlevel to new level if the tab_level is in sync. This was
  // added because restoring a tab from a session would fire a zoom-update when
  // the document finished loading through
  // ZoomController::DidFinishNavigation().
  if (old_level == tab_zoom_level_ && new_level != tab_zoom_level_) {
    tab_zoom_level_ = new_level;
    SaveZoomLevelToExtData(new_level);
  } else if (!called_host_zoom_ && old_level != tab_zoom_level_) {
    called_host_zoom_ = true;
    // Make sure the view has the correct zoom level set.
    content::HostZoomMap* host_zoom_map_ =
        content::HostZoomMap::GetForWebContents(web_contents());

    host_zoom_map_->SetTemporaryZoomLevel(
        web_contents()->GetPrimaryMainFrame()->GetGlobalId(), tab_zoom_level_);
    called_host_zoom_ = false;
  }
}

VivaldiGuestViewContentObserver* VivaldiGuestViewContentObserver::FromTabId(
    content::BrowserContext* browser_context,
    int tab_id,
    std::string* error) {
  content::WebContents* guest_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, browser_context,
                                                      error);
  if (!guest_contents) {
    return nullptr;
  }

  VivaldiGuestViewContentObserver* guestcontent_observer =
      VivaldiGuestViewContentObserver::FromWebContents(guest_contents);
  if (!guestcontent_observer) {
    *error = "Failed to lookup VivaldiGuestViewContentObserver for id:" +
             std::to_string(tab_id);
    return nullptr;
  }

  return guestcontent_observer;
}

void VivaldiGuestViewContentObserver::SaveZoomLevelToExtData(
    double zoom_level) {
  std::string viv_ext_data = web_contents()->GetVivExtData();
  std::optional<base::Value> json = GetDictValueFromVivExtData(viv_ext_data);
  if (json) {
    json->GetDict().Set(kVivaldiTabZoom, zoom_level);
    std::string json_string;
    if (ValueToJSONString(*json, json_string)) {
      web_contents()->SetVivExtData(json_string);
    }
  }
}

void VivaldiGuestViewContentObserver::SetShowImages(bool show_images) {
  if (show_images_ == show_images) {
    // Make sure we do not call SetImagesEnabled uneccesary. OnUpdated is called
    // numerous times.
    return;
  }

  show_images_ = show_images;

  // This will loop over all images and show or hide them.
  web_contents()->GetRenderViewHost()->SetImagesEnabled(show_images);
}

void VivaldiGuestViewContentObserver::SetLoadFromCacheOnly(
    bool load_from_cache_only) {
  load_from_cache_only_ = load_from_cache_only;

  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->serve_resources_only_from_cache = load_from_cache_only;
  web_contents()->GetRenderViewHost()->SetServeResourceFromCacheOnly(
      load_from_cache_only);
}

void VivaldiGuestViewContentObserver::SetMuted(bool mute) {
  mute_ = mute;
  std::string viv_ext_data = web_contents()->GetVivExtData();
  std::optional<base::Value> json = GetDictValueFromVivExtData(viv_ext_data);
  if (json) {
    std::optional<bool> existing = json->GetDict().FindBool(kVivaldiTabMuted);
    if ((existing && *existing != mute) || (!existing && mute)) {
      json->GetDict().Set(kVivaldiTabMuted, mute);
      std::string json_string;
      if (ValueToJSONString(*json, json_string)) {
        web_contents()->SetVivExtData(json_string);
      }
    }
  }
  if (mute_ == web_contents()->IsAudioMuted()) {
    // NOTE(andre@vivaldi.com) : contentsettings will not be used if muting
    // reason is set to extension. So only set muting reason to extension when
    // we actually change the muting state. See
    // |SoundContentSettingObserver::MuteOrUnmuteIfNecessary|
    return;
  }
  SetTabAudioMuted(web_contents(), mute, TabMutedReason::EXTENSION,
                   ::vivaldi::kVivaldiAppId);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VivaldiGuestViewContentObserver);

ExtensionFunction::ResponseAction
TabsPrivateGetSendTabToSelfEntriesFunction::Run() {
  using tabs_private::GetSendTabToSelfEntries::Params;
  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  namespace Results = tabs_private::GetSendTabToSelfEntries::Results;

  std::vector<extensions::vivaldi::tabs_private::SendTabToSelfEntry> list;

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  send_tab_to_self::SendTabToSelfModel* model =
    SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel();
  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  if (model->IsReady() && device_info_tracker) {
    std::vector<std::string> guids = model->GetAllGuids();
    for (auto guid : guids) {
      const send_tab_to_self::SendTabToSelfEntry* entry =
        model->GetEntryByGUID(guid);
      if (entry) {
        if (device_info_tracker->IsRecentLocalCacheGuid(
                entry->GetTargetDeviceSyncCacheGuid()) &&
            !entry->GetNotificationDismissed() && !entry->IsOpened()) {
          extensions::vivaldi::tabs_private::SendTabToSelfEntry item;
          item.guid = entry->GetGUID();
          item.url = entry->GetURL().spec();
          item.title = entry->GetTitle();
          item.device_name = entry->GetDeviceName();
          item.shared_time = entry->GetSharedTime().InMillisecondsFSinceUnixEpoch();
          list.push_back(std::move(item));
        }
      }
    }
  } else if (!params->silent) {
    // For simplicity we allow to mute this warning. Will happen on startup if
    // we call this function too early. Once the model loads an event is sent
    // to JS and we load again (then without silent set).
    LOG(ERROR) << "SendTabToSelf. Model not ready";
  }

  return RespondNow(ArgumentList(Results::Create(list)));
}

ExtensionFunction::ResponseAction
TabsPrivateDismissSendTabToSelfEntriesFunction::Run() {
  using tabs_private::DismissSendTabToSelfEntries::Params;
  namespace Results = tabs_private::DismissSendTabToSelfEntries::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile)
                    ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    for (const std::string& guid : params->guids) {
      model->DeleteEntry(guid);
    }
  }

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction
TabsPrivateExecSendTabToSelfActionFunction::Run() {
  using tabs_private::ExecSendTabToSelfAction::Params;
  namespace Results = tabs_private::ExecSendTabToSelfAction::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  auto* model = SendTabToSelfSyncServiceFactory::GetForProfile(profile)
                    ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    switch (params->action) {
      case tabs_private::SendTabToSelfAction::kDelete:
        for (const std::string& guid : params->guids) {
          model->DeleteEntry(guid);
        }
        break;
      case vivaldi::tabs_private::SendTabToSelfAction::kDismiss:
        for (const std::string& guid : params->guids) {
          model->DismissEntry(guid);
        }
        break;
      case vivaldi::tabs_private::SendTabToSelfAction::kNone:
      default:
        break;
    }
  }
  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction
TabsPrivateGetSendTabToSelfTargetsFunction::Run() {
  namespace Results = tabs_private::GetSendTabToSelfTargets::Results;

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  extensions::vivaldi::tabs_private::SendTabToSelfTargets result;

  // Use the readyness of the model to dermine if system has been enbled or not.
  send_tab_to_self::SendTabToSelfModel* model =
    SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->GetSendTabToSelfModel();
  if (!model || !model->IsReady()) {
    result.valid = false;
    return RespondNow(ArgumentList(Results::Create(result)));
  }

  for (auto device : model->GetTargetDeviceInfoSortedList()) {
    extensions::vivaldi::tabs_private::SendTabToSelfTarget item;
    item.guid = device.cache_guid;
    item.name = device.full_name;
    if (device.form_factor == syncer::DeviceInfo::FormFactor::kPhone) {
      item.type = vivaldi::tabs_private::SendTabToSelfTargetType::kPhone;
    } else if (device.form_factor == syncer::DeviceInfo::FormFactor::kTablet) {
      item.type = vivaldi::tabs_private::SendTabToSelfTargetType::kTablet;
    } else {
      item.type = vivaldi::tabs_private::SendTabToSelfTargetType::kDesktop;
    }
    result.entries.push_back(std::move(item));
  }

  result.valid = true;
  return RespondNow(ArgumentList(Results::Create(result)));
}

ExtensionFunction::ResponseAction
TabsPrivateSendSendTabToSelfTargetFunction::Run() {
  using tabs_private::SendSendTabToSelfTarget::Params;
  namespace Results = tabs_private::SendSendTabToSelfTarget::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile);
  service->GetSendTabToSelfModel()->AddEntry(
      GURL(params->url), params->title, params->guid);

  return RespondNow(ArgumentList(Results::Create()));
}

}  // namespace extensions
