// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/tabs/tabs_private_api.h"

#include <math.h>
#include <memory>
#include <utility>
#include <vector>

#include "app/vivaldi_constants.h"
#include "app/vivaldi_apptools.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/command.h"
#include "components/permissions/permission_util.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h" // nogncheck
#include "content/browser/web_contents/web_contents_impl.h" // nogncheck
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/api/access_keys/access_keys_api.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/api/thumbnails/thumbnails_api.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
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

namespace extensions {

const char kVivaldiTabZoom[] = "vivaldi_tab_zoom";
const char kVivaldiTabMuted[] = "vivaldi_tab_muted";

const int& VivaldiPrivateTabObserver::kUserDataKey =
    VivaldiTabCheck::kVivaldiTabObserverContextKey;

namespace tabs_private = vivaldi::tabs_private;

TabsPrivateAPI::TabsPrivateAPI(content::BrowserContext* context) {}

TabsPrivateAPI::~TabsPrivateAPI() {}

// static
TabsPrivateAPI* TabsPrivateAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  TabsPrivateAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  return api;
}

void TabsPrivateAPI::Shutdown() {}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TabsPrivateAPI>>::
    DestructorAtExit g_factory_tabs = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TabsPrivateAPI>*
TabsPrivateAPI::GetFactoryInstance() {
  return g_factory_tabs.Pointer();
}

namespace {

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
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_DESKTOP_CAPTURING);
        break;
      case TabAlertState::VR_PRESENTING_IN_HEADSET:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_VR_PRESENTING_IN_HEADSET);
        break;
      case TabAlertState::SERIAL_CONNECTED:
        types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_SERIAL_CONNECTED);
        break;
      default:
        NOTREACHED() << "Unknown TabAlertState Status:" << (int)status;
        break;
    }
  }

  // NOTE(andre@vivaldi.com) : We should only use the first type returned, if
  // any. See comment for GetTabAlertStatesForContents.
  if (types.empty()) {
    types.push_back(tabs_private::TabAlertState::TAB_ALERT_STATE_EMPTY);
  }

  return types;
}

}  // namespace

void TabsPrivateAPI::TabChangedAt(content::WebContents* web_contents,
                                  int index,
                                  TabChangeType change_type) {
  if (!web_contents || !static_cast<content::WebContentsImpl*>(web_contents)
                            ->GetMainFrame()
                            ->GetProcess())
    return;

  std::vector<tabs_private::TabAlertState> states =
      ConvertTabAlertState(chrome::GetTabAlertStatesForContents(web_contents));

  int tabId = extensions::ExtensionTabUtil::GetTabId(web_contents);
  int windowId = extensions::ExtensionTabUtil::GetWindowIdOfTab(web_contents);

  ::vivaldi::BroadcastEvent(
    tabs_private::OnMediaStateChanged::kEventName,
    tabs_private::OnMediaStateChanged::Create(
      tabId, windowId, states),
    web_contents->GetBrowserContext());
}

void TabsPrivateAPI::Notify(
    javascript_dialogs::AppModalDialogController* dialog) {
  if (dialog->is_before_unload_dialog()) {
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
}

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
}

VivaldiPrivateTabObserver::~VivaldiPrivateTabObserver() {}

void VivaldiPrivateTabObserver::WebContentsDestroyed() {}

void VivaldiPrivateTabObserver::OnPrefsChanged(const std::string& path) {
  if (path == vivaldiprefs::kWebpagesFocusTrap) {
    UpdateAllowTabCycleIntoUI();
    CommitSettings();
  } else if (path == vivaldiprefs::kWebpagesAccessKeys) {
    UpdateAllowAccessKeys();
    CommitSettings();
  }
}

void VivaldiPrivateTabObserver::BroadcastTabInfo() {
  tabs_private::UpdateTabInfo info;
  info.show_images.reset(new bool(show_images()));
  info.load_from_cache_only.reset(new bool(load_from_cache_only()));
  info.enable_plugins.reset(new bool(enable_plugins()));
  info.mime_type.reset(new std::string(contents_mime_type()));
  info.mute_tab.reset(new bool(mute()));
  int id = sessions::SessionTabHelper::IdForTab(web_contents()).id();

  ::vivaldi::BroadcastEvent(tabs_private::OnTabUpdated::kEventName,
                            tabs_private::OnTabUpdated::Create(id, info),
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

void VivaldiPrivateTabObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  std::string ext = web_contents()->GetExtData();
  base::Optional<base::Value> json = GetDictValueFromExtData(ext);
  if (::vivaldi::isTabZoomEnabled(web_contents())) {
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

  base::Optional<bool> mute =
      json ? json->FindBoolKey(kVivaldiTabMuted) : base::nullopt;
  if (mute) {
    mute_ = *mute;
  }

  // This is not necessary for each RVH-change.
  SetMuted(mute_);

  SetShowImages(show_images_);
  SetLoadFromCacheOnly(load_from_cache_only_);
  SetEnablePlugins(enable_plugins_);
  UpdateAllowTabCycleIntoUI();
  UpdateAllowAccessKeys();
  CommitSettings();

  const GURL& site = render_view_host->GetSiteInstance()->GetSiteURL();
  std::string renderviewhost = site.host();
  if (::vivaldi::IsVivaldiApp(renderviewhost)) {
    auto* security_policy = content::ChildProcessSecurityPolicy::GetInstance();
    int process_id = render_view_host->GetProcess()->GetID();
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
  if (::vivaldi::isTabZoomEnabled(web_contents())) {
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
  blink::mojom::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  render_prefs->allow_tab_cycle_from_webpage_into_ui =
      !profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesFocusTrap);
}

void VivaldiPrivateTabObserver::UpdateAllowAccessKeys() {
  blink::mojom::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  render_prefs->allow_access_keys =
      profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesAccessKeys);
}

void VivaldiPrivateTabObserver::SetShowImages(bool show_images) {
  show_images_ = show_images;

  blink::mojom::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->should_show_images = show_images;
}

void VivaldiPrivateTabObserver::SetLoadFromCacheOnly(
    bool load_from_cache_only) {
  load_from_cache_only_ = load_from_cache_only;

  blink::mojom::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->serve_resources_only_from_cache = load_from_cache_only;
}

void VivaldiPrivateTabObserver::SetEnablePlugins(bool enable_plugins) {
  enable_plugins_ = enable_plugins;
  blink::mojom::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->should_enable_plugin_content = enable_plugins;
}

void VivaldiPrivateTabObserver::SetMuted(bool mute) {
  mute_ = mute;
  std::string ext = web_contents()->GetExtData();
  base::Optional<base::Value> json = GetDictValueFromExtData(ext);
  if (json) {
    json->SetBoolKey(kVivaldiTabMuted, mute);
    std::string json_string;
    if (ValueToJSONString(*json, json_string)) {
      web_contents()->SetExtData(json_string);
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
  blink::mojom::RendererPreferences* render_prefs =
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
  if (!::vivaldi::isTabZoomEnabled(web_contents) || tab_zoom_level_ == -1) {
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

bool VivaldiPrivateTabObserver::OnMessageReceived(const IPC::Message& message) {
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
  BroadcastTabInfo();
}

void VivaldiPrivateTabObserver::GetAccessKeys(AccessKeysCallback callback) {
  access_keys_callback_ = std::move(callback);
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  rvh->Send(new VivaldiViewMsg_GetAccessKeysForPage(rvh->GetRoutingID()));
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
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  rvh->Send(new VivaldiViewMsg_GetScrollPosition(rvh->GetRoutingID()));
}

void VivaldiPrivateTabObserver::OnGetScrollPositionResponse(int x, int y) {
  if (!scroll_position_callback_.is_null()) {
    std::move(scroll_position_callback_).Run(x, y);
  }
}

void VivaldiPrivateTabObserver::AccessKeyAction(std::string access_key) {
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  rvh->Send(
      new VivaldiViewMsg_AccessKeyAction(rvh->GetRoutingID(), access_key));
}

void VivaldiPrivateTabObserver::GetSpatialNavigationRects(
    GetSpatialNavigationRectsCallback callback) {
  spatnav_callback_ = std::move(callback);
  content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
  rvh->Send(new VivaldiViewMsg_GetSpatialNavigationRects(rvh->GetRoutingID()));
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
          tab_id, ExtensionTabUtil::GetWindowIdOfTab(web_contents()),
          states[0]),
      web_contents()->GetBrowserContext());
}

void VivaldiPrivateTabObserver::BeforeUnloadFired(
    bool proceed,
    const base::TimeTicks& proceed_time) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  Browser* browser =
    ::vivaldi::FindBrowserWithWebContents(web_contents());
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

////////////////////////////////////////////////////////////////////////////////

namespace {

content::RenderViewHost* GetFocusedRenderViewHost(ExtensionFunction* function,
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

  content::RenderViewHost* rvh = tabstrip_contents->GetRenderViewHost();
  if (!rvh) {
    *error = "GetRenderViewHost() is null";
    return nullptr;
  }
  return rvh;
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
  tab_api->BroadcastTabInfo();

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
  content::RenderViewHost* rvh =
      GetFocusedRenderViewHost(this, params->tab_id, &error);
  if (!rvh)
    return RespondNow(Error(error));

  base::string16 text = base::UTF8ToUTF16(params->text);
  rvh->Send(new VivaldiMsg_InsertText(rvh->GetRoutingID(), text));

  return RespondNow(NoArguments());
}

TabsPrivateStartDragFunction::TabsPrivateStartDragFunction() = default;
TabsPrivateStartDragFunction::~TabsPrivateStartDragFunction() = default;

ExtensionFunction::ResponseAction TabsPrivateStartDragFunction::Run() {
  using tabs_private::StartDrag::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::string16 identifier(
      base::UTF8ToUTF16(params->drag_data.mime_type));
  const base::string16 custom_data(
      base::UTF8ToUTF16(params->drag_data.custom_data));

  drop_data_.custom_data.emplace(identifier, custom_data);

  drop_data_.url = GURL(params->drag_data.url);
  drop_data_.url_title = base::UTF8ToUTF16(params->drag_data.title);

  event_info_.event_source =
      (params->drag_data.is_from_touch && *params->drag_data.is_from_touch)
          ? ui::mojom::DragEventSource::kTouch
          : ui::mojom::DragEventSource::kMouse;
  event_info_.event_location =
      display::Screen::GetScreen()->GetCursorScreenPoint();

  image_offset_.set_x(params->drag_data.cursor_x);
  image_offset_.set_y(params->drag_data.cursor_y);

  StartUICapture(
      GetSenderWebContents(), params->drag_data.pos_x, params->drag_data.pos_y,
      params->drag_data.width, params->drag_data.height,
      base::BindOnce(&TabsPrivateStartDragFunction::OnCaptureDone, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
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
    blink::WebDragOperationsMask allowed_ops =
        static_cast<blink::WebDragOperationsMask>(
            blink::WebDragOperation::kWebDragOperationMove);
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
  content::RenderViewHost* rvh =
      GetFocusedRenderViewHost(this, params->tab_id, &error);
  if (!rvh)
    return RespondNow(Error(error));

  std::string scroll_type = params->scroll_type;
  int scroll_amount = (params->scroll_amount) ? *(params->scroll_amount): 0;
  rvh->Send(new VivaldiViewMsg_ScrollPage(rvh->GetRoutingID(), scroll_type,
                                          scroll_amount));

  return RespondNow(ArgumentList(Results::Create()));
}

void TabsPrivateGetSpatialNavigationRectsFunction::
    GetSpatialNavigationRectsResponse(
        std::vector<VivaldiViewMsg_NavigationRect> navigation_rects) {
  namespace Results = tabs_private::GetSpatialNavigationRects::Results;
  std::vector<tabs_private::NavigationRect> results;

  for (auto& nav_rect: navigation_rects) {
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

void TabsPrivateGetScrollPositionFunction::
    GetScrollPositionResponse(int x, int y) {
  namespace Results = tabs_private::GetScrollPosition::Results;

  Respond(ArgumentList(Results::Create(x, y)));
}

ExtensionFunction::ResponseAction
TabsPrivateGetScrollPositionFunction::Run() {
  using tabs_private::GetScrollPosition::Params;

  std::unique_ptr<Params> params = Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string error;
  VivaldiPrivateTabObserver* tab_api = VivaldiPrivateTabObserver::FromTabId(
      browser_context(), params->tab_id, &error);
  if (!tab_api)
    return RespondNow(Error(error));
  tab_api->GetScrollPosition(
      base::BindOnce(&TabsPrivateGetScrollPositionFunction::
                     GetScrollPositionResponse, this));

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
  content::RenderViewHost* rvh =
      GetFocusedRenderViewHost(this, params->tab_id, &error);
  if (!rvh)
    return RespondNow(Error(error));

  rvh->Send(new VivaldiViewMsg_ActivateElementFromPoint(rvh->GetRoutingID(),
                                                        x, y, modifiers));

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
  return RespondNow(ArgumentList(
      Results::Create(contents && contents->NeedToFireBeforeUnloadOrUnloadEvents())));
}

}  // namespace extensions
