// Copyright (c) 2016-2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/tabs/tabs_private_api.h"

#include <math.h>
#include <memory>
#include <utility>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "browser/translate/vivaldi_translate_client.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/command.h"
#include "components/capture/capture_page.h"
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
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/api/access_keys/access_keys_api.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_pref_enums.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "prefs/vivaldi_tab_zoom_pref.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/content/vivaldi_event_hooks.h"
#include "ui/content/vivaldi_tab_check.h"
#include "ui/display/screen.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/drag_utils.h"
#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif  // defined(OS_WIN)
#include "ui/strings/grit/ui_strings.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

using content::WebContents;
using vivaldiprefs::TabsAutoMutingValues;

namespace extensions {

const char kVivaldiTabZoom[] = "vivaldi_tab_zoom";
const char kVivaldiTabMuted[] = "vivaldi_tab_muted";

const int& VivaldiPrivateTabObserver::kUserDataKey =
    VivaldiTabCheck::kVivaldiTabObserverContextKey;

namespace tabs_private = vivaldi::tabs_private;

bool IsTabMuted(const WebContents* web_contents) {
  std::string extdata = web_contents->GetExtData();
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  base::Optional<base::Value> json = base::JSONReader::Read(extdata, options);
  base::Optional<bool> mute = base::nullopt;
  if (json && json->is_dict()) {
    mute = json->FindBoolKey(kVivaldiTabMuted);
  }
  return mute ? *mute : false;
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
    Browser* browser =
        ::vivaldi::FindBrowserWithWebContents(dialog->web_contents());
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
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_RECORDING);
        break;
      case TabAlertState::TAB_CAPTURING:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_CAPTURING);
        break;
      case TabAlertState::AUDIO_PLAYING:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_PLAYING);
        break;
      case TabAlertState::AUDIO_MUTING:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_MUTING);
        break;
      case TabAlertState::BLUETOOTH_CONNECTED:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_BLUETOOTH);
        break;
      case TabAlertState::USB_CONNECTED:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_USB);
        break;
      case TabAlertState::PIP_PLAYING:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_PIP);
        break;
      case TabAlertState::DESKTOP_CAPTURING:
        types.push_back(
            tabs_private::TabAlertState::TAB_ALERT_STATE_DESKTOP_CAPTURING);
        break;
      case TabAlertState::VR_PRESENTING_IN_HEADSET:
        types.push_back(tabs_private::TabAlertState::
                            TAB_ALERT_STATE_VR_PRESENTING_IN_HEADSET);
        break;
      case TabAlertState::SERIAL_CONNECTED:
        types.push_back(
            tabs_private::TabAlertState::TAB_ALERT_STATE_SERIAL_CONNECTED);
        break;
      default:
        NOTREACHED() << "Unknown TabAlertState Status:" << (int)status;
        break;
    }
  }

  return types;
}

}  // namespace

class TabMutingHandler : public content_settings::Observer {
  HostContentSettingsMap* host_content_settings_map_;
  PrefChangeRegistrar prefs_registrar_;
  Profile* profile_;
  TabsAutoMutingValues muteRule_ = TabsAutoMutingValues::kOff;
  // NOTE(andre@vivaldi.com) : This is per profile so make sure the handler
  // takes this into account.
  ScopedObserver<HostContentSettingsMap, content_settings::Observer> observer_{
      this};

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
             browser_list->begin_last_active();
         browser_iterator != browser_list->end_last_active();
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
  TabMutingHandler(Profile* profile) : profile_(profile) {
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(profile_);
    observer_.Add(host_content_settings_map_);

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

    for (auto* browser : *BrowserList::GetInstance()) {
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
                            ->GetMainFrame()
                            ->GetProcess()) {
    return;
  }
  // Sound state might have changed, check if any tabs should play or be muted.
  UpdateMuting();

  std::vector<tabs_private::TabAlertState> states =
      ConvertTabAlertState(chrome::GetTabAlertStatesForContents(web_contents));

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
    : WebContentsObserver(web_contents), weak_ptr_factory_(this) {
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  if (zoom_controller) {
    zoom_controller->AddObserver(this);
  }
  prefs_registrar_.Init(
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs());

  prefs_registrar_.Add(vivaldiprefs::kWebpagesFocusTrap,
                       base::Bind(&VivaldiPrivateTabObserver::OnPrefsChanged,
                                  weak_ptr_factory_.GetWeakPtr()));
  prefs_registrar_.Add(vivaldiprefs::kWebpagesAccessKeys,
                       base::Bind(&VivaldiPrivateTabObserver::OnPrefsChanged,
                                  weak_ptr_factory_.GetWeakPtr()));

  VivaldiTranslateClient* translate_client =
      VivaldiTranslateClient::FromWebContents(web_contents);
  if (translate_client) {
    translate_client->translate_driver()->AddTranslationObserver(this);
    translate_client->translate_driver()->AddLanguageDetectionObserver(this);
  }
}

VivaldiPrivateTabObserver::~VivaldiPrivateTabObserver() {}

void VivaldiPrivateTabObserver::WebContentsDestroyed() {
  VivaldiTranslateClient* translate_client =
      VivaldiTranslateClient::FromWebContents(web_contents());
  if (translate_client) {
    translate_client->translate_driver()->RemoveTranslationObserver(this);
    translate_client->translate_driver()->RemoveLanguageDetectionObserver(this);
  }
}

void VivaldiPrivateTabObserver::OnPrefsChanged(const std::string& path) {
  if (path == vivaldiprefs::kWebpagesFocusTrap) {
    UpdateAllowTabCycleIntoUI();
    CommitSettings();
  } else if (path == vivaldiprefs::kWebpagesAccessKeys) {
    UpdateAllowAccessKeys();
    CommitSettings();
  }
}

void VivaldiPrivateTabObserver::BroadcastTabInfo(
    tabs_private::UpdateTabInfo& info) {
  int id = sessions::SessionTabHelper::IdForTab(web_contents()).id();
  int windowId = extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents());
  ::vivaldi::BroadcastEvent(
      tabs_private::OnTabUpdated::kEventName,
      tabs_private::OnTabUpdated::Create(id, windowId, info),
      web_contents()->GetBrowserContext());
}

const int kThemeColorBufferSize = 8;

void VivaldiPrivateTabObserver::DidChangeThemeColor() {
  base::Optional<SkColor> theme_color = web_contents()->GetThemeColor();
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

base::Optional<base::Value> GetDictValueFromExtData(std::string& extdata) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  base::Optional<base::Value> value = base::JSONReader::Read(extdata, options);
  if (value && value->is_dict()) {
    return value;
  }
  return base::nullopt;
}

void VivaldiPrivateTabObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  std::string ext = web_contents()->GetExtData();
  base::Optional<base::Value> json = GetDictValueFromExtData(ext);
  if (::vivaldi::IsTabZoomEnabled(web_contents())) {
    base::Optional<double> zoom =
        json ? json->FindDoubleKey(kVivaldiTabZoom) : base::nullopt;
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

  SetShowImages(show_images_);
  SetLoadFromCacheOnly(load_from_cache_only_);
  SetEnablePlugins(enable_plugins_);
  UpdateAllowTabCycleIntoUI();
  UpdateAllowAccessKeys();
  CommitSettings();

  const GURL& site = render_frame_host->GetSiteInstance()->GetSiteURL();
  if (::vivaldi::IsVivaldiApp(site.host())) {
    auto* security_policy = content::ChildProcessSecurityPolicy::GetInstance();
    int process_id = render_frame_host->GetProcess()->GetID();
    security_policy->GrantRequestScheme(process_id, url::kFileScheme);
    security_policy->GrantRequestScheme(process_id, content::kViewSourceScheme);
  }
}

void VivaldiPrivateTabObserver::SaveZoomLevelToExtData(double zoom_level) {
  std::string ext = web_contents()->GetExtData();
  base::Optional<base::Value> json = GetDictValueFromExtData(ext);
  if (json) {
    json->SetDoubleKey(kVivaldiTabZoom, zoom_level);
    std::string json_string;
    if (ValueToJSONString(*json, json_string)) {
      web_contents()->SetExtData(json_string);
    }
  }
}

void VivaldiPrivateTabObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  if (::vivaldi::IsTabZoomEnabled(web_contents())) {
    int render_view_id = new_host->GetRoutingID();
    int process_id = new_host->GetProcess()->GetID();

    content::HostZoomMap* host_zoom_map_ =
        content::HostZoomMap::GetForWebContents(web_contents());

    host_zoom_map_->SetTemporaryZoomLevel(process_id, render_view_id,
                                          tab_zoom_level_);
  }

  // Set the setting on the new RenderViewHost too.
  SetShowImages(show_images_);
  SetLoadFromCacheOnly(load_from_cache_only_);
  SetEnablePlugins(enable_plugins_);
  UpdateAllowTabCycleIntoUI();
  UpdateAllowAccessKeys();
  CommitSettings();
}

void VivaldiPrivateTabObserver::UpdateAllowTabCycleIntoUI() {
  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  render_prefs->allow_tab_cycle_from_webpage_into_ui =
      !profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesFocusTrap);
}

void VivaldiPrivateTabObserver::UpdateAllowAccessKeys() {
  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  render_prefs->allow_access_keys =
      profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesAccessKeys);
}

void VivaldiPrivateTabObserver::SetShowImages(bool show_images) {
  show_images_ = show_images;

  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->should_show_images = show_images;
}

void VivaldiPrivateTabObserver::SetLoadFromCacheOnly(
    bool load_from_cache_only) {
  load_from_cache_only_ = load_from_cache_only;

  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->serve_resources_only_from_cache = load_from_cache_only;
}

void VivaldiPrivateTabObserver::SetEnablePlugins(bool enable_plugins) {
  enable_plugins_ = enable_plugins;
  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->should_enable_plugin_content = enable_plugins;
}

void VivaldiPrivateTabObserver::SetMuted(bool mute) {
  mute_ = mute;
  std::string ext = web_contents()->GetExtData();
  base::Optional<base::Value> json = GetDictValueFromExtData(ext);
  if (json) {
    base::Optional<bool> existing = json->FindBoolKey(kVivaldiTabMuted);
    if ((existing && *existing != mute) || (!existing && mute)) {
      json->SetBoolKey(kVivaldiTabMuted, mute);
      std::string json_string;
      if (ValueToJSONString(*json, json_string)) {
        web_contents()->SetExtData(json_string);
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
  chrome::SetTabAudioMuted(web_contents(), mute, TabMutedReason::EXTENSION,
                           ::vivaldi::kVivaldiAppId);
}

void VivaldiPrivateTabObserver::CommitSettings() {
  blink::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);

  // We must update from system settings otherwise many settings would fallback
  // to default values when syncing below.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(render_prefs, profile);

  if (load_from_cache_only_ && enable_plugins_) {
    render_prefs->should_ask_plugin_content = true;
  } else {
    render_prefs->should_ask_plugin_content = false;
  }
  web_contents()->SyncRendererPrefs();
}

void VivaldiPrivateTabObserver::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  content::WebContents* web_contents = data.web_contents;
  content::StoragePartition* current_partition =
      content::BrowserContext::GetStoragePartition(
          web_contents->GetBrowserContext(), web_contents->GetSiteInstance(),
          false);
  if (!::vivaldi::IsTabZoomEnabled(web_contents) || tab_zoom_level_ == -1) {
    return;
  }

  if (current_partition &&
      current_partition == content::BrowserContext::GetDefaultStoragePartition(
                               web_contents->GetBrowserContext()))
    SetZoomLevelForTab(data.new_zoom_level, data.old_zoom_level);
}

void VivaldiPrivateTabObserver::SetZoomLevelForTab(double new_level,
                                                   double old_level) {
  // Only update zoomlevel to new level if the tab_level is in sync. This was
  // added because restoring a tab from a session would fire a zoom-update when
  // the document finished loading through
  // ZoomController::DidFinishNavigation().
  if (old_level == tab_zoom_level_ && new_level != tab_zoom_level_) {
    tab_zoom_level_ = new_level;
    SaveZoomLevelToExtData(new_level);
  } else if (old_level != tab_zoom_level_) {
    // Make sure the view has the correct zoom level set.
    content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
    int render_view_id = rvh->GetRoutingID();
    int process_id = rvh->GetProcess()->GetID();

    content::HostZoomMap* host_zoom_map_ =
        content::HostZoomMap::GetForWebContents(web_contents());

    host_zoom_map_->SetTemporaryZoomLevel(process_id, render_view_id,
                                          tab_zoom_level_);
  }
}

bool VivaldiPrivateTabObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiPrivateTabObserver, message)
    IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_GetAccessKeysForPage_ACK,
                        OnGetAccessKeysForPageResponse)
    IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_GetSpatialNavigationRects_ACK,
                        OnGetSpatialNavigationRectsResponse)
    IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_GetScrollPosition_ACK,
                        OnGetScrollPositionResponse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VivaldiPrivateTabObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  SetContentsMimeType(web_contents()->GetContentsMimeType());
  tabs_private::UpdateTabInfo info;
  info.mime_type.reset(new std::string(contents_mime_type()));
  BroadcastTabInfo(info);
}

void VivaldiPrivateTabObserver::GetAccessKeys(AccessKeysCallback callback) {
  access_keys_callback_ = std::move(callback);
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->Send(new VivaldiViewMsg_GetAccessKeysForPage(rfh->GetRoutingID()));
}

void VivaldiPrivateTabObserver::OnGetAccessKeysForPageResponse(
    std::vector<VivaldiViewMsg_AccessKeyDefinition> access_keys) {
  if (!access_keys_callback_.is_null()) {
    std::move(access_keys_callback_).Run(std::move(access_keys));
  }
}

void VivaldiPrivateTabObserver::GetScrollPosition(
    GetScrollPositionCallback callback) {
  scroll_position_callback_ = std::move(callback);
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->Send(new VivaldiViewMsg_GetScrollPosition(rfh->GetRoutingID()));
}

void VivaldiPrivateTabObserver::OnGetScrollPositionResponse(int x, int y) {
  if (!scroll_position_callback_.is_null()) {
    std::move(scroll_position_callback_).Run(x, y);
  }
}

void VivaldiPrivateTabObserver::AccessKeyAction(std::string access_key) {
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->Send(
      new VivaldiViewMsg_AccessKeyAction(rfh->GetRoutingID(), access_key));
}

void VivaldiPrivateTabObserver::GetSpatialNavigationRects(
    GetSpatialNavigationRectsCallback callback) {
  spatnav_callback_ = std::move(callback);
  content::RenderFrameHost* rfh = web_contents()->GetMainFrame();
  rfh->Send(new VivaldiViewMsg_GetSpatialNavigationRects(rfh->GetRoutingID()));
}

void VivaldiPrivateTabObserver::OnGetSpatialNavigationRectsResponse(
    std::vector<VivaldiViewMsg_NavigationRect> rects) {
  if (!spatnav_callback_.is_null()) {
    std::move(spatnav_callback_).Run(std::move(rects));
  }
}

void VivaldiPrivateTabObserver::OnPermissionAccessed(
    ContentSettingsType content_settings_type,
    std::string origin,
    ContentSetting content_setting) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());

  std::string type_name = base::ToLowerASCII(
      permissions::PermissionUtil::GetPermissionString(content_settings_type));

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
  const std::vector<tabs_private::TabAlertState> states = ConvertTabAlertState(
      chrome::GetTabAlertStatesForContents(web_contents()));

  ::vivaldi::BroadcastEvent(
      tabs_private::OnTabIsAttached::kEventName,
      tabs_private::OnTabIsAttached::Create(
          tab_id, ExtensionTabUtil::GetWindowIdOfTab(web_contents()), states),
      web_contents()->GetBrowserContext());
}

void VivaldiPrivateTabObserver::BeforeUnloadFired(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  Browser* browser = ::vivaldi::FindBrowserWithWebContents(web_contents());
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

void VivaldiPrivateTabObserver::OnPageTranslated(
    const std::string& original_lang,
    const std::string& translated_lang,
    translate::TranslateErrors::Type error_type) {
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

////////////////////////////////////////////////////////////////////////////////

namespace {

content::RenderFrameHost* GetFocusedRenderFrameHost(ExtensionFunction* function,
                                                    int tab_id,
                                                    std::string* error) {
  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          tab_id, function->browser_context(), error);
  if (!tabstrip_contents)
    return nullptr;

  content::RenderFrameHost* focused_frame =
      tabstrip_contents->GetFocusedFrame();
  if (!focused_frame) {
    *error = "GetFocusedFrame() is null";
    return nullptr;
  }
  return focused_frame;
}

}  // namespace

ExtensionFunction::ResponseAction TabsPrivateUpdateFunction::Run() {
  using tabs_private::Update::Params;
  namespace Results = tabs_private::Update::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tabs_private::UpdateTabInfo* info = &params->tab_info;
  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));

  if (info->show_images) {
    tab_api->SetShowImages(*info->show_images.get());
  }
  if (info->load_from_cache_only) {
    tab_api->SetLoadFromCacheOnly(*info->load_from_cache_only.get());
  }
  if (info->enable_plugins) {
    tab_api->SetEnablePlugins(*info->enable_plugins.get());
  }
  if (info->mute_tab) {
    tab_api->SetMuted(*info->mute_tab.get());
  }
  tab_api->CommitSettings();
  tab_api->BroadcastTabInfo(*info);

  return RespondNow(ArgumentList(Results::Create(*info)));
}

ExtensionFunction::ResponseAction TabsPrivateGetFunction::Run() {
  using tabs_private::Get::Params;
  namespace Results = tabs_private::Get::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));

  tabs_private::UpdateTabInfo info;
  info.show_images.reset(new bool(tab_api->show_images()));
  info.load_from_cache_only.reset(new bool(tab_api->load_from_cache_only()));
  info.enable_plugins.reset(new bool(tab_api->enable_plugins()));
  return RespondNow(ArgumentList(Results::Create(info)));
}

ExtensionFunction::ResponseAction TabsPrivateInsertTextFunction::Run() {
  using tabs_private::InsertText::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  content::RenderFrameHost* rfh =
      GetFocusedRenderFrameHost(this, params->tab_id, &error);
  if (!rfh)
    return RespondNow(Error(error));

  std::u16string text = base::UTF8ToUTF16(params->text);
  rfh->Send(new VivaldiMsg_InsertText(rfh->GetRoutingID(), text));

  return RespondNow(NoArguments());
}

TabsPrivateStartDragFunction::TabsPrivateStartDragFunction() = default;
TabsPrivateStartDragFunction::~TabsPrivateStartDragFunction() = default;

ExtensionFunction::ResponseAction TabsPrivateStartDragFunction::Run() {
  using tabs_private::StartDrag::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

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
    SkBitmap bitmap;
    SkImageInfo image_info = SkImageInfo::MakeN32Premul(w, h);

    // SkBitmap::installPixels takes ownership of data and calls the release
    // callback to delete them in all cases including on errors.
    using ImageVector = std::vector<uint8_t>;
    ImageVector* raw_image = params->drag_data.image_data.release();
    auto release_pixels = [](void* addr, void* context) {
      // Let unique_ptr to call delete.
      std::unique_ptr<ImageVector> image_data(
          static_cast<ImageVector*>(context));
      CHECK(addr == image_data->data());
    };
    bool success = bitmap.installPixels(image_info, raw_image->data(),
                                        image_info.minRowBytes(),
                                        release_pixels, raw_image);
    OnCaptureDone(success, 1.0, bitmap);
    return AlreadyResponded();
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error("null sender"));
  double x = params->drag_data.pos_x ? *params->drag_data.pos_x : 0.0;
  double y = params->drag_data.pos_y ? *params->drag_data.pos_y : 0.0;
  gfx::RectF rect(x, y, width, height);
  ::vivaldi::FromUICoordinates(web_contents, &rect);
  ::vivaldi::CapturePage::CaptureVisible(
      web_contents, rect,
      base::BindOnce(&TabsPrivateStartDragFunction::OnCaptureDone, this));
  return RespondLater();
}

void TabsPrivateStartDragFunction::OnCaptureDone(bool success,
                                                 float device_scale_factor,
                                                 const SkBitmap& bitmap) {
  do {
    if (!success)
      break;

    content::WebContents* web_contents = GetSenderWebContents();
    if (!web_contents) {
      // The capture finished after the user closed the browser window.
      LOG(ERROR) << "WebContents is null";
      break;
    }

    content::RenderViewHostImpl* rvh =
        static_cast<content::RenderViewHostImpl*>(
            web_contents->GetRenderViewHost());
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
    view->StartDragging(drop_data_, allowed_ops, image, image_offset_,
                        event_info_, rvh->GetWidget());
  } while (false);

  Respond(NoArguments());
}

ExtensionFunction::ResponseAction TabsPrivateScrollPageFunction::Run() {
  using tabs_private::ScrollPage::Params;
  namespace Results = tabs_private::ScrollPage::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  content::RenderFrameHost* rfh =
      GetFocusedRenderFrameHost(this, params->tab_id, &error);
  if (!rfh)
    return RespondNow(Error(error));

  std::string scroll_type = params->scroll_type;
  int scroll_amount = (params->scroll_amount) ? *(params->scroll_amount) : 0;
  rfh->Send(new VivaldiViewMsg_ScrollPage(rfh->GetRoutingID(), scroll_type,
                                          scroll_amount));

  return RespondNow(ArgumentList(Results::Create()));
}

void TabsPrivateGetSpatialNavigationRectsFunction::
    GetSpatialNavigationRectsResponse(
        std::vector<VivaldiViewMsg_NavigationRect> navigation_rects) {
  namespace Results = tabs_private::GetSpatialNavigationRects::Results;
  std::vector<tabs_private::NavigationRect> results;

  for (auto& nav_rect : navigation_rects) {
    tabs_private::NavigationRect rect;
    rect.left = nav_rect.x;
    rect.top = nav_rect.y;
    rect.width = nav_rect.width;
    rect.height = nav_rect.height;
    rect.right = nav_rect.x + nav_rect.width;
    rect.bottom = nav_rect.y + nav_rect.height;
    rect.href = nav_rect.href;
    rect.path = nav_rect.path;
    results.push_back(std::move(rect));
  }

  Respond(ArgumentList(Results::Create(results)));
}

ExtensionFunction::ResponseAction
TabsPrivateGetSpatialNavigationRectsFunction::Run() {
  using tabs_private::GetSpatialNavigationRects::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));
  tab_api->GetSpatialNavigationRects(
      base::BindOnce(&TabsPrivateGetSpatialNavigationRectsFunction::
                         GetSpatialNavigationRectsResponse,
                     this));

  return RespondLater();
}

void TabsPrivateGetScrollPositionFunction::GetScrollPositionResponse(int x,
                                                                     int y) {
  namespace Results = tabs_private::GetScrollPosition::Results;

  Respond(ArgumentList(Results::Create(x, y)));
}

ExtensionFunction::ResponseAction TabsPrivateGetScrollPositionFunction::Run() {
  using tabs_private::GetScrollPosition::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));
  tab_api->GetScrollPosition(base::BindOnce(
      &TabsPrivateGetScrollPositionFunction::GetScrollPositionResponse, this));

  return RespondLater();
}

ExtensionFunction::ResponseAction
TabsPrivateActivateElementFromPointFunction::Run() {
  using tabs_private::ActivateElementFromPoint::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int x = params->x;
  int y = params->y;
  int modifiers = params->modifiers;

  std::string error;
  content::RenderFrameHost* rfh =
      GetFocusedRenderFrameHost(this, params->tab_id, &error);
  if (!rfh)
    return RespondNow(Error(error));

  rfh->Send(new VivaldiViewMsg_ActivateElementFromPoint(rfh->GetRoutingID(), x,
                                                        y, modifiers));

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
TabsPrivateHasBeforeUnloadOrUnloadFunction::Run() {
  using tabs_private::HasBeforeUnloadOrUnload::Params;
  namespace Results = tabs_private::HasBeforeUnloadOrUnload::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
          params->tab_id, browser_context(), nullptr);
  return RespondNow(ArgumentList(Results::Create(
      contents && contents->NeedToFireBeforeUnloadOrUnloadEvents())));
}

ExtensionFunction::ResponseAction TabsPrivateTranslatePageFunction::Run() {
  using tabs_private::TranslatePage::Params;
  namespace Results = tabs_private::TranslatePage::Results;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

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

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

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

}  // namespace extensions
