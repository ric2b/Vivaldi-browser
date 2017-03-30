// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/tabs/tabs_private_api.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/prefs/pref_service.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/tabs_private.h"
#include "prefs/vivaldi_pref_names.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::VivaldiPrivateTabObserver);

namespace extensions {

namespace tabs_private = vivaldi::tabs_private;

/*static*/
void VivaldiPrivateTabObserver::BroadcastEvent(
    const std::string& eventname,
    scoped_ptr<base::ListValue>& args,
    content::BrowserContext* context) {
  scoped_ptr<extensions::Event> event(new extensions::Event(
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
    ui_zoom::ZoomController::FromWebContents(web_contents);
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
  scoped_ptr<base::ListValue> args =
      tabs_private::OnThemeColorChanged::Create(tab_id, rgb_buffer);
  BroadcastEvent(tabs_private::OnThemeColorChanged::kEventName, args, profile);
}

bool VivaldiPrivateTabObserver::IsVivaldiTabZoomEnabled() {
  Profile* profile = Profile::FromBrowserContext(
    web_contents()->GetBrowserContext());

  return profile->GetPrefs()->GetBoolean(vivaldiprefs::kVivaldiTabZoom);
}

base::StringValue DictionaryToJSONString(
  const base::DictionaryValue& dict) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(dict,0, &json_string);
  return base::StringValue(json_string);
}


void VivaldiPrivateTabObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (IsVivaldiTabZoomEnabled()) {
    std::string ext = web_contents()->GetExtData();

    base::JSONParserOptions options = base::JSON_PARSE_RFC;
    scoped_ptr<base::Value> json =
      base::JSONReader::Read(ext, options);
    base::DictionaryValue* dict = NULL;
    if (json && json->GetAsDictionary(&dict)) {
      if (dict) {
        dict->GetDouble("vivaldi_tab_zoom", &tab_zoom_level_);
      }
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
  scoped_ptr<base::Value> json =
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
  if (IsVivaldiTabZoomEnabled()) {
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

  if (load_from_cache_only_ && enable_plugins_) {
    render_prefs->should_ask_plugin_content = true;
  } else {
    render_prefs->should_ask_plugin_content = false;
  }
  web_contents()->GetRenderViewHost()->SyncRendererPrefs();
}

void VivaldiPrivateTabObserver::OnZoomChanged(
    const ui_zoom::ZoomController::ZoomChangedEventData& data) {
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
  scoped_ptr<tabs_private::Update::Params> params(
      tabs_private::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tabs_private::UpdateTabInfo* info = &params->tab_info;
  int tab_id = params->tab_id;

  content::WebContents* tabstrip_contents = NULL;
  bool include_incognito = true;
  Browser* browser;
  int tab_index;
  if (extensions::ExtensionTabUtil::GetTabById(tab_id, GetProfile(),
    include_incognito, &browser, NULL, &tabstrip_contents, &tab_index)) {

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
  return true;
}

TabsPrivateGetFunction::TabsPrivateGetFunction() {
}

TabsPrivateGetFunction::~TabsPrivateGetFunction() {
}

bool TabsPrivateGetFunction::RunAsync() {
  scoped_ptr<tabs_private::Get::Params> params(
      tabs_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  tabs_private::UpdateTabInfo info;

  content::WebContents* tabstrip_contents = NULL;
  bool include_incognito = true;
  Browser* browser;
  int tab_index;
  if (extensions::ExtensionTabUtil::GetTabById(tab_id, GetProfile(),
    include_incognito, &browser, NULL, &tabstrip_contents, &tab_index)) {

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
  scoped_ptr<base::ListValue> args =
      vivaldi::tabs_private::OnFaviconUpdated::Create(info);

  Profile *profile = Profile::FromBrowserContext(
      content_favicon_driver->web_contents()->GetBrowserContext());

  BroadcastEvent(tabs_private::OnFaviconUpdated::kEventName, args, profile);
}

}  // namespace extensions
