// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/tabs/tabs_private_api.h"

#include <vector>
#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/memory/tab_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/prefs/pref_service.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/drop_data.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/tabs_private.h"
#include "prefs/vivaldi_pref_names.h"
#include "prefs/vivaldi_tab_zoom_pref.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/display/screen.h"
#include "ui/display/win/dpi.h"
#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif  // defined(OS_WIN)

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::VivaldiPrivateTabObserver);

using content::WebContents;

namespace extensions {

namespace tabs_private = vivaldi::tabs_private;


namespace {

content::WebContents* GetWebContentsFromTabStrip(int tab_id, Profile *profile) {
  content::WebContents *contents = nullptr;
  bool include_incognito = true;
  Browser* browser;
  int tab_index;
  extensions::ExtensionTabUtil::GetTabById(tab_id, profile, include_incognito,
                                           &browser, NULL, &contents,
                                           &tab_index);
  return contents;
}

bool IsOutsideAppWindow(int screen_x, int screen_y, Profile* profile) {
  gfx::Point screen_point(screen_x, screen_y);

  AppWindowRegistry* app_window_registry =
    AppWindowRegistry::Factory::GetForBrowserContext(profile, false);
  AppWindowRegistry::AppWindowList list =
    app_window_registry->GetAppWindowsForApp(::vivaldi::kVivaldiAppId);

  bool outside = true;
  for (auto& win : list) {
    gfx::Rect rect = win->GetBaseWindow()->GetBounds();
    if (rect.Contains(screen_point)) {
      outside = false;
      break;
    }
  }
  return outside;
}

}  // namespace

TabsPrivateAPI::TabsPrivateAPI(content::BrowserContext* context)
  : browser_context_(context) {
  event_router_.reset(new TabsPrivateEventRouter(
      Profile::FromBrowserContext(context)));
  EventRouter::Get(Profile::FromBrowserContext(context))
      ->RegisterObserver(this, tabs_private::OnDragEnd::kEventName);
}

TabsPrivateAPI::~TabsPrivateAPI() {
}

void TabsPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TabsPrivateAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TabsPrivateAPI> *
TabsPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void TabsPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// VivaldiAppHelper::TabDrag interface. We only care about the dragend event,
// the rest is passed as HTML 5 dnd events anyway.
void TabsPrivateEventRouter::OnDragEnter(const TabDragDataCollection &data) {}

void TabsPrivateEventRouter::OnDragOver(const TabDragDataCollection& data) {
}

void TabsPrivateEventRouter::OnDragLeave(const TabDragDataCollection& data) {
}

void TabsPrivateEventRouter::OnDrop(const TabDragDataCollection& data) {
}

namespace {

void PopulateDragData(const TabDragDataCollection& data,
                      vivaldi::tabs_private::DragData& drag_data) {
  for (TabDragDataCollection::const_iterator it = data.begin();
       it != data.end(); ++it) {
    drag_data.mime_type.append(base::UTF16ToUTF8(it->first));
    drag_data.custom_data.append(base::UTF16ToUTF8(it->second));

    // Should be only a single element
    break;
  }
}

}  // namespace

blink::WebDragOperationsMask TabsPrivateEventRouter::OnDragEnd(
    int screen_x, int screen_y, blink::WebDragOperationsMask ops,
    const TabDragDataCollection& data, bool cancelled) {
  vivaldi::tabs_private::DragData drag_data;
  bool outside = IsOutsideAppWindow(screen_x, screen_y, profile_);
  if (data.empty()) {
    // Drop happened before drop data was filled out in dragenter, get
    // the backup data as js has not been able to add to it.
    TabsPrivateAPI* api =
        TabsPrivateAPI::GetFactoryInstance()->Get(profile_);

    TabDragDataCollection& drop_data = api->GetDropDataBackup();

    PopulateDragData(drop_data, drag_data);
  } else {
    PopulateDragData(data, drag_data);
  }
  ::vivaldi::SetTabDragInProgress(false);

  std::unique_ptr<base::ListValue> args =
      vivaldi::tabs_private::OnDragEnd::Create(drag_data, cancelled, outside,
                                               screen_x, screen_y);

  DispatchEvent(extensions::events::VIVALDI_EXTENSION_EVENT,
                vivaldi::tabs_private::OnDragEnd::kEventName,
                args);

  return outside ? blink::WebDragOperationNone : ops;
}

blink::WebDragOperationsMask TabsPrivateEventRouter::OnDragCursorUpdating(
    int screen_x, int screen_y, blink::WebDragOperationsMask ops) {
  return blink::WebDragOperationsMask::WebDragOperationMove;
}

TabsPrivateEventRouter::TabsPrivateEventRouter(Profile* profile)
  : profile_(profile) {
}

TabsPrivateEventRouter::~TabsPrivateEventRouter() {
}

void TabsPrivateEventRouter::DispatchEvent(
    events::HistogramValue histogram_value, const std::string &event_name,
    std::unique_ptr<base::ListValue>& args) {
  EventRouter *event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;

  std::unique_ptr<Event> event(
    new Event(histogram_value, event_name, std::move(args)));
  event_router->BroadcastEvent(std::move(event));
}


/*static*/
void VivaldiPrivateTabObserver::BroadcastEvent(
    const std::string& eventname,
    std::unique_ptr<base::ListValue>& args,
    content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args)));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

VivaldiPrivateTabObserver::VivaldiPrivateTabObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents), favicon_scoped_observer_(this) {

  auto zoom_controller =
    zoom::ZoomController::FromWebContents(web_contents);
  if (zoom_controller) {
    zoom_controller->AddObserver(this);
  }

  favicon_scoped_observer_.Add(
      favicon::ContentFaviconDriver::FromWebContents(web_contents));
}

VivaldiPrivateTabObserver::~VivaldiPrivateTabObserver() {
}

void VivaldiPrivateTabObserver::WebContentsDestroyed() {
  favicon_scoped_observer_.Remove(
      favicon::ContentFaviconDriver::FromWebContents(web_contents()));
}

const int kThemeColorBufferSize = 8;

void VivaldiPrivateTabObserver::DidChangeThemeColor(SkColor theme_color) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  char rgb_buffer[kThemeColorBufferSize];
  base::snprintf(rgb_buffer, kThemeColorBufferSize, "#%02x%02x%02x",
                 SkColorGetR(theme_color), SkColorGetG(theme_color),
                 SkColorGetB(theme_color));
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());
  std::unique_ptr<base::ListValue> args =
      tabs_private::OnThemeColorChanged::Create(tab_id, rgb_buffer);
  BroadcastEvent(tabs_private::OnThemeColorChanged::kEventName, args, profile);
}

base::StringValue DictionaryToJSONString(
  const base::DictionaryValue& dict) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(dict, 0, &json_string);
  return base::StringValue(json_string);
}

void VivaldiPrivateTabObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (::vivaldi::isTabZoomEnabled(web_contents())) {
    std::string ext = web_contents()->GetExtData();

    base::JSONParserOptions options = base::JSON_PARSE_RFC;
    std::unique_ptr<base::Value> json = base::JSONReader::Read(ext, options);
    base::DictionaryValue* dict = NULL;
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    content::HostZoomMap* host_zoom_map =
        content::HostZoomMap::GetDefaultForBrowserContext(profile);
    double default_zoom_level = host_zoom_map->GetDefaultZoomLevel();

    if (json && json->GetAsDictionary(&dict)) {
      if (dict) {
        if (dict->HasKey("vivaldi_tab_zoom")) {
          dict->GetDouble("vivaldi_tab_zoom", &tab_zoom_level_);
        } else {
          tab_zoom_level_ = default_zoom_level;
        }
      }
    } else {
      tab_zoom_level_ = default_zoom_level;
    }
  }

  SetShowImages(show_images_);
  SetLoadFromCacheOnly(load_from_cache_only_);
  SetEnablePlugins(enable_plugins_);
  CommitSettings();
}

void VivaldiPrivateTabObserver::SaveZoomLevelToExtData(double zoom_level) {
  std::string ext = web_contents()->GetExtData();

  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::unique_ptr<base::Value> json =
    base::JSONReader::Read(ext, options);
  base::DictionaryValue* dict = NULL;
  if (json && json->GetAsDictionary(&dict)) {
    if (dict) {
      dict->SetDouble("vivaldi_tab_zoom", zoom_level);
      base::StringValue st = DictionaryToJSONString(*dict);
      web_contents()->SetExtData(*st.GetString());
    }
  }
}

void VivaldiPrivateTabObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host, content::RenderViewHost* new_host) {
  if (::vivaldi::isTabZoomEnabled(web_contents())) {
    int render_view_id = new_host->GetRoutingID();
    int process_id = new_host->GetProcess()->GetID();

    content::HostZoomMap* host_zoom_map_ =
      content::HostZoomMap::GetForWebContents(web_contents());

    host_zoom_map_->SetTemporaryZoomLevel(
      process_id, render_view_id, tab_zoom_level_);
  }

  // Set the setting on the new RenderViewHost too.
  SetShowImages(show_images_);
  SetLoadFromCacheOnly(load_from_cache_only_);
  SetEnablePlugins(enable_plugins_);
  CommitSettings();
}

void VivaldiPrivateTabObserver::SetShowImages(bool show_images) {
  show_images_ = show_images;

  content::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->should_show_images = show_images;
}

void VivaldiPrivateTabObserver::SetLoadFromCacheOnly(
    bool load_from_cache_only) {
  load_from_cache_only_ = load_from_cache_only;

  content::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->serve_resources_only_from_cache = load_from_cache_only;
}

void VivaldiPrivateTabObserver::SetEnablePlugins(bool enable_plugins) {
  enable_plugins_ = enable_plugins;
  content::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  render_prefs->should_enable_plugin_content = enable_plugins;
}

void VivaldiPrivateTabObserver::CommitSettings() {
  content::RendererPreferences* render_prefs =
      web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);

  // We must update from system settings otherwise many settings would fallback
  // to default values when syncing below.
  Profile *profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(render_prefs, profile,
                                                      web_contents());

  if (load_from_cache_only_ && enable_plugins_) {
    render_prefs->should_ask_plugin_content = true;
  } else {
    render_prefs->should_ask_plugin_content = false;
  }
  web_contents()->GetRenderViewHost()->SyncRendererPrefs();
}

void VivaldiPrivateTabObserver::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  SetZoomLevelForTab(data.new_zoom_level);
}

void VivaldiPrivateTabObserver::SetZoomLevelForTab(double level) {
  if (level != tab_zoom_level_) {
    tab_zoom_level_ = level;
    SaveZoomLevelToExtData(level);
  }
}

TabsPrivateUpdateFunction::TabsPrivateUpdateFunction() {
}

TabsPrivateUpdateFunction::~TabsPrivateUpdateFunction() {
}

bool TabsPrivateUpdateFunction::RunAsync() {
  std::unique_ptr<tabs_private::Update::Params> params(
      tabs_private::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tabs_private::UpdateTabInfo* info = &params->tab_info;
  int tab_id = params->tab_id;

  content::WebContents *tabstrip_contents =
      GetWebContentsFromTabStrip(tab_id, GetProfile());
  if (tabstrip_contents) {
    VivaldiPrivateTabObserver* tab_api =
        VivaldiPrivateTabObserver::FromWebContents(tabstrip_contents);
    DCHECK(tab_api);
    if (tab_api) {
      if (info->show_images) {
        tab_api->SetShowImages(*info->show_images.get());
      }
      if (info->load_from_cache_only) {
        tab_api->SetLoadFromCacheOnly(*info->load_from_cache_only.get());
      }
      if (info->enable_plugins) {
        tab_api->SetEnablePlugins(*info->enable_plugins.get());
      }
      tab_api->CommitSettings();
    }
  }
  SendResponse(true);
  return true;
}

TabsPrivateGetFunction::TabsPrivateGetFunction() {
}

TabsPrivateGetFunction::~TabsPrivateGetFunction() {
}

bool TabsPrivateGetFunction::RunAsync() {
  std::unique_ptr<tabs_private::Get::Params> params(
      tabs_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  tabs_private::UpdateTabInfo info;

  content::WebContents *tabstrip_contents =
      GetWebContentsFromTabStrip(tab_id, GetProfile());
  if (tabstrip_contents) {
    VivaldiPrivateTabObserver* tab_api =
        VivaldiPrivateTabObserver::FromWebContents(tabstrip_contents);
    DCHECK(tab_api);
    if (tab_api) {
      info.show_images.reset(new bool(tab_api->show_images()));
      info.load_from_cache_only.reset(
          new bool(tab_api->load_from_cache_only()));
      info.enable_plugins.reset(new bool(tab_api->enable_plugins()));

      results_ = tabs_private::Get::Results::Create(info);
      SendResponse(true);
      return true;
    }
  }
  return false;
}

void VivaldiPrivateTabObserver::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {

  favicon::ContentFaviconDriver* content_favicon_driver =
        static_cast<favicon::ContentFaviconDriver*>(favicon_driver);

  vivaldi::tabs_private::FaviconInfo info;
  info.tab_id = extensions::ExtensionTabUtil::GetTabId(
      content_favicon_driver->web_contents());
  if (!image.IsEmpty()) {
    info.fav_icon =
        *extensions::ExtensionActionUtil::EncodeBitmapToPng(image.ToSkBitmap());
  }
  std::unique_ptr<base::ListValue> args =
      vivaldi::tabs_private::OnFaviconUpdated::Create(info);

  Profile *profile = Profile::FromBrowserContext(
      content_favicon_driver->web_contents()->GetBrowserContext());

  BroadcastEvent(tabs_private::OnFaviconUpdated::kEventName, args, profile);
}

TabsPrivateInsertTextFunction::TabsPrivateInsertTextFunction() {
}

TabsPrivateInsertTextFunction::~TabsPrivateInsertTextFunction() {
}

bool TabsPrivateInsertTextFunction::RunAsync() {
  std::unique_ptr<tabs_private::InsertText::Params> params(
    tabs_private::InsertText::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  bool success = false;

  base::string16 text = base::UTF8ToUTF16(params->text);

  content::WebContents *tabstrip_contents =
    GetWebContentsFromTabStrip(tab_id, GetProfile());

  content::RenderFrameHost* focused_frame =
    tabstrip_contents->GetFocusedFrame();

  if (focused_frame) {
    success = true;
    tabstrip_contents->GetRenderViewHost()->Send(
      new VivaldiMsg_InsertText(
        tabstrip_contents->GetRenderViewHost()->GetRoutingID(), text));
  }

  SendResponse(success);
  return success;
}

TabsPrivateStartDragFunction::TabsPrivateStartDragFunction() {
}

TabsPrivateStartDragFunction::~TabsPrivateStartDragFunction() {
}

bool TabsPrivateStartDragFunction::RunAsync() {
  std::unique_ptr<tabs_private::StartDrag::Params> params(
    tabs_private::StartDrag::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  SkBitmap bitmap;
  gfx::Vector2d image_offset;
  if (params->drag_image.get()) {
    std::string string_data;
    if (base::Base64Decode(params->drag_image->image, &string_data)) {
      const unsigned char *data =
          reinterpret_cast<const unsigned char *>(string_data.c_str());
      // Try PNG first.
      if (!gfx::PNGCodec::Decode(data, string_data.length(), &bitmap)) {
        // Try JPEG.
        std::unique_ptr<SkBitmap> decoded_jpeg(
            gfx::JPEGCodec::Decode(data, string_data.length()));
        if (decoded_jpeg) {
          bitmap = *decoded_jpeg;
        }
      }
    }
    image_offset.set_x(params->drag_image->cursor_x);
    image_offset.set_y(params->drag_image->cursor_y);
  }

  AppWindow *window = AppWindowRegistry::Get(GetProfile())
                          ->GetCurrentAppWindowForApp(::vivaldi::kVivaldiAppId);
  DCHECK(window);
  content::RenderViewHostImpl *rvh = static_cast<content::RenderViewHostImpl *>(
      window->web_contents()->GetRenderViewHost());
  DCHECK(rvh);
  content::RenderViewHostDelegateView *view =
      rvh->GetDelegate()->GetDelegateView();

  content::DropData drop_data;
  const base::string16 identifier(
      base::UTF8ToUTF16(params->drag_data.mime_type));
  const base::string16 custom_data(
      base::UTF8ToUTF16(params->drag_data.custom_data));

  drop_data.custom_data.insert(std::make_pair(
      identifier, custom_data));

  drop_data.url = GURL(params->drag_data.url);
  drop_data.url_title = base::UTF8ToUTF16(params->drag_data.title);

  blink::WebDragOperationsMask allowed_ops =
    static_cast<blink::WebDragOperationsMask>(
      blink::WebDragOperation::WebDragOperationMove);

  gfx::ImageSkia image(gfx::ImageSkiaRep(bitmap, 1));
  content::DragEventSourceInfo event_info;

  event_info.event_source =
      params->is_from_touch.get() && *params->is_from_touch.get()
          ? ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH
          : ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  event_info.event_location =
      display::Screen::GetScreen()->GetCursorScreenPoint();

  TabsPrivateAPI* api = TabsPrivateAPI::GetFactoryInstance()->Get(GetProfile());

  TabDragDataCollection backup_drop_data;
  backup_drop_data.insert(std::make_pair(identifier, custom_data));

  api->SetDropDataBackup(backup_drop_data);

  ::vivaldi::SetTabDragInProgress(true);
  view->StartDragging(drop_data, allowed_ops, image, image_offset, event_info);
  SendResponse(true);
  return true;
}

}  // namespace extensions
