// Copyright (c) 2016-2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/tabs/tabs_private_api.h"

#include <memory>
#include <utility>
#include <vector>

#include "app/vivaldi_apptools.h"
#include "app/vivaldi_constants.h"
#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/command.h"
#include "components/prefs/pref_service.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/drop_data.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/schema/tabs_private.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "prefs/vivaldi_pref_names.h"
#include "prefs/vivaldi_tab_zoom_pref.h"
#include "renderer/vivaldi_render_messages.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/display/win/dpi.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/views/drag_utils.h"
#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif  // defined(OS_WIN)
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"
#include "ui/strings/grit/ui_strings.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::VivaldiPrivateTabObserver);

using content::WebContents;

namespace extensions {

namespace tabs_private = vivaldi::tabs_private;

TabsPrivateAPI::TabsPrivateAPI(content::BrowserContext* context)
    : browser_context_(context) {
  event_router_.reset(
      new TabsPrivateEventRouter(Profile::FromBrowserContext(context)));
  EventRouter::Get(Profile::FromBrowserContext(context))
      ->RegisterObserver(this, tabs_private::OnDragEnd::kEventName);
}

TabsPrivateAPI::~TabsPrivateAPI() {}

void TabsPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TabsPrivateAPI> >::
    DestructorAtExit g_factory_tabs = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TabsPrivateAPI>*
TabsPrivateAPI::GetFactoryInstance() {
  return g_factory_tabs.Pointer();
}

void TabsPrivateAPI::OnListenerAdded(const EventListenerInfo& details) {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

namespace {
static tabs_private::MediaType ConvertTabAlertState(TabAlertState status) {
  switch (status) {
  case TabAlertState::NONE:
    return tabs_private::MediaType::MEDIA_TYPE_EMPTY;
  case TabAlertState::MEDIA_RECORDING:
    return tabs_private::MediaType::MEDIA_TYPE_RECORDING;
  case TabAlertState::TAB_CAPTURING:
    return tabs_private::MediaType::MEDIA_TYPE_CAPTURING;
  case TabAlertState::AUDIO_PLAYING:
    return tabs_private::MediaType::MEDIA_TYPE_PLAYING;
  case TabAlertState::AUDIO_MUTING:
    return tabs_private::MediaType::MEDIA_TYPE_MUTING;
  case TabAlertState::BLUETOOTH_CONNECTED:
    return tabs_private::MediaType::MEDIA_TYPE_BLUETOOTH;
  case TabAlertState::USB_CONNECTED:
    return tabs_private::MediaType::MEDIA_TYPE_USB;
  case TabAlertState::PIP_PLAYING:
    return tabs_private::MediaType::MEDIA_TYPE_PIP;
  case TabAlertState::DESKTOP_CAPTURING:
    return tabs_private::MediaType::MEDIA_TYPE_DESKTOP_CAPTURING;
  }
  NOTREACHED() << "Unknown TabAlertState Status.";
  return tabs_private::MediaType::MEDIA_TYPE_NONE;
}
}  // namespace

void TabsPrivateAPI::TabChangedAt(content::WebContents* contents,
                                  int index,
                                  TabChangeType change_type) {
  tabs_private::MediaType type =
      ConvertTabAlertState(chrome::GetTabAlertStateForContents(contents));

  std::unique_ptr<base::ListValue> args =
      tabs_private::OnMediaStateChanged::Create(
          extensions::ExtensionTabUtil::GetTabId(contents), type);

  VivaldiPrivateTabObserver::BroadcastEvent(
      tabs_private::OnMediaStateChanged::kEventName, std::move(args),
      browser_context_);
}

base::string16 TabsPrivateAPI::KeyCodeToName(ui::KeyboardCode key_code) const {
  int string_id = 0;
  switch (key_code) {
    case ui::VKEY_TAB:
      string_id = IDS_APP_TAB_KEY;
      break;
    case ui::VKEY_RETURN:
      string_id = IDS_APP_ENTER_KEY;
      break;
    case ui::VKEY_SPACE:
      string_id = IDS_APP_SPACE_KEY;
      break;
    case ui::VKEY_PRIOR:
      string_id = IDS_APP_PAGEUP_KEY;
      break;
    case ui::VKEY_NEXT:
      string_id = IDS_APP_PAGEDOWN_KEY;
      break;
    case ui::VKEY_END:
      string_id = IDS_APP_END_KEY;
      break;
    case ui::VKEY_HOME:
      string_id = IDS_APP_HOME_KEY;
      break;
    case ui::VKEY_INSERT:
      string_id = IDS_APP_INSERT_KEY;
      break;
    case ui::VKEY_DELETE:
      string_id = IDS_APP_DELETE_KEY;
      break;
    case ui::VKEY_LEFT:
      string_id = IDS_APP_LEFT_ARROW_KEY;
      break;
    case ui::VKEY_RIGHT:
      string_id =IDS_APP_RIGHT_ARROW_KEY;
      break;
    case ui::VKEY_UP:
      string_id = IDS_APP_UP_ARROW_KEY;
      break;
    case ui::VKEY_DOWN:
      string_id = IDS_APP_DOWN_ARROW_KEY;
      break;
    case ui::VKEY_ESCAPE:
      string_id = IDS_APP_ESC_KEY;
      break;
    case ui::VKEY_BACK:
      string_id = IDS_APP_BACKSPACE_KEY;
      break;
    case ui::VKEY_F1:
      string_id = IDS_APP_F1_KEY;
      break;
    case ui::VKEY_F11:
      string_id = IDS_APP_F11_KEY;
      break;
    case ui::VKEY_OEM_COMMA:
      string_id = IDS_APP_COMMA_KEY;
      break;
    case ui::VKEY_OEM_PERIOD:
      string_id = IDS_APP_PERIOD_KEY;
      break;
    case ui::VKEY_MEDIA_NEXT_TRACK:
      string_id = IDS_APP_MEDIA_NEXT_TRACK_KEY;
      break;
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      string_id = IDS_APP_MEDIA_PLAY_PAUSE_KEY;
      break;
    case ui::VKEY_MEDIA_PREV_TRACK:
      string_id = IDS_APP_MEDIA_PREV_TRACK_KEY;
      break;
    case ui::VKEY_MEDIA_STOP:
      string_id = IDS_APP_MEDIA_STOP_KEY;
      break;
    default:
      break;
  }
  return string_id ? l10n_util::GetStringUTF16(string_id) : base::string16();
}


std::string TabsPrivateAPI::ShortcutText(
    const content::NativeWebKeyboardEvent& event) {

  // We'd just use Accelerator::GetShortcutText to get the shortcut text but
  // it translates the modifiers when the system language is set to
  // non-English (since it's used for display). We can't match something
  // like 'Strg+G' however, so we do the modifiers manually.
  //
  // AcceleratorToString gets the shortcut text, but doesn't localize
  // like Accelerator::GetShortcutText() does, so it's suitable for us.
  // It doesn't handle all keys, however, and doesn't work with ctrl+alt
  // shortcuts so we're left with doing a little tweaking.
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(event.windows_key_code);
  ui::Accelerator accelerator =
      ui::Accelerator(key_code, 0, ui::Accelerator::KeyState::PRESSED);

  std::string shortcutText = "";
  if (event.GetModifiers() & content::NativeWebKeyboardEvent::kControlKey) {
    shortcutText += "Ctrl+";
  }
  if (event.GetModifiers() & content::NativeWebKeyboardEvent::kShiftKey) {
    shortcutText += "Shift+";
  }
  if (event.GetModifiers() & content::NativeWebKeyboardEvent::kAltKey) {
    shortcutText += "Alt+";
  }
  if (event.GetModifiers() & content::NativeWebKeyboardEvent::kMetaKey) {
    shortcutText += "Meta+";
  }

  std::string key_from_accelerator =
      extensions::Command::AcceleratorToString(accelerator);
  if (!key_from_accelerator.empty()) {
    shortcutText += key_from_accelerator;
  } else if (event.windows_key_code >= ui::VKEY_F1 &&
             event.windows_key_code <= ui::VKEY_F24) {
    char buf[4];
    base::snprintf(buf, 4, "F%d", event.windows_key_code - ui::VKEY_F1 + 1);
    shortcutText += buf;

  } else if (event.windows_key_code >= ui::VKEY_NUMPAD0 &&
             event.windows_key_code <= ui::VKEY_NUMPAD9) {
    char buf[8];
    base::snprintf(buf, 8, "Numpad%d",
                   event.windows_key_code - ui::VKEY_NUMPAD0);
    shortcutText += buf;

  // Enter is somehow not covered anywhere else.
  } else if (event.windows_key_code == ui::VKEY_RETURN) {
    shortcutText += "Enter";
  // GetShortcutText doesn't translate numbers and digits but
  // 'does' translate backspace
  } else if (event.windows_key_code == ui::VKEY_BACK) {
    shortcutText += "Backspace";
  // Escape was being translated as well in some languages
  } else if (event.windows_key_code == ui::VKEY_ESCAPE) {
    shortcutText += "Esc";
  } else {
#if defined(OS_MACOSX)
  // This is equivalent to js event.code and deals with a few
  // MacOS keyboard shortcuts like cmd+alt+n that fall through
  // in some languages, i.e. AcceleratorToString returns a blank.
  // Cmd+Alt shortcuts seem to be the only case where this fallback
  // is required.
  if (event.GetModifiers() & content::NativeWebKeyboardEvent::kAltKey &&
      event.GetModifiers() & content::NativeWebKeyboardEvent::kMetaKey) {
    shortcutText += ui::DomCodeToUsLayoutCharacter(
        static_cast<ui::DomCode>(event.dom_code), 0);
  } else {
    // With chrome 67 accelerator.GetShortcutText() will return Mac specific
    // symbols (like '⎋' for escape). All is private so we bypass that by
    // testing with KeyCodeToName first.
    base::string16 shortcut = KeyCodeToName(key_code);
    if (shortcut.empty())
      shortcut = accelerator.GetShortcutText();
    shortcutText += base::UTF16ToUTF8(shortcut);
  }
#else
    shortcutText += base::UTF16ToUTF8(accelerator.GetShortcutText());
#endif // OS_MACOSX
  }
  return shortcutText;
}

void TabsPrivateAPI::SendKeyboardShortcutEvent(
    const content::NativeWebKeyboardEvent& event, bool is_auto_repeat) {
  if (event.GetType() == blink::WebInputEvent::kRawKeyDown) {
    std::unique_ptr<base::ListValue> args =
        vivaldi::tabs_private::OnKeyboardChanged::Create(
            true, event.GetModifiers(), event.windows_key_code);
    event_router_->DispatchEvent(
          extensions::events::VIVALDI_EXTENSION_EVENT,
          extensions::vivaldi::tabs_private::OnKeyboardChanged::kEventName,
          std::move(args));
  } else if(event.GetType() == blink::WebInputEvent::kKeyUp) {
    std::unique_ptr<base::ListValue> args =
        vivaldi::tabs_private::OnKeyboardChanged::Create(
            false, event.GetModifiers(), event.windows_key_code);
    event_router_->DispatchEvent(
          extensions::events::VIVALDI_EXTENSION_EVENT,
          extensions::vivaldi::tabs_private::OnKeyboardChanged::kEventName,
          std::move(args));
  }

  // Return here as we don't allow AltGr keyboard shortcuts
  if (event.GetModifiers() & blink::WebInputEvent::kAltGrKey) {
      return;
  }

  // Don't send if event contains only modifiers.
  int key_code = event.windows_key_code;
  if (key_code != ui::VKEY_CONTROL && key_code != ui::VKEY_SHIFT &&
      key_code != ui::VKEY_MENU) {
    std::string shortcut_text = ShortcutText(event);
    // If the event wasn't prevented we'll get a rawKeyDown event. In some
    // exceptional cases we'll never get that, so we let these through
    // unconditionally
    std::vector<std::string> exceptions =
        {"Up", "Down", "Shift+Delete", "Meta+Shift+V", "Esc"};
    bool is_exception = std::find(exceptions.begin(), exceptions.end(),
                                  shortcut_text) != exceptions.end();
    if (event.GetType() == blink::WebInputEvent::kRawKeyDown || is_exception) {
      std::unique_ptr<base::ListValue> args =
          vivaldi::tabs_private::OnKeyboardShortcut::Create(shortcut_text,
                                                            is_auto_repeat);
      event_router_->DispatchEvent(
          extensions::events::VIVALDI_EXTENSION_EVENT,
          extensions::vivaldi::tabs_private::OnKeyboardShortcut::kEventName,
          std::move(args));
    }
  }
}

// VivaldiAppHelper::TabDrag interface. We only care about the dragend event,
// the rest is passed as HTML 5 dnd events anyway.
void TabsPrivateEventRouter::OnDragEnter(const TabDragDataCollection& data) {}

void TabsPrivateEventRouter::OnDragOver(const TabDragDataCollection& data) {}

void TabsPrivateEventRouter::OnDragLeave(const TabDragDataCollection& data) {}

void TabsPrivateEventRouter::OnDrop(const TabDragDataCollection& data) {}

namespace {

void PopulateDragData(const TabDragDataCollection& data,
                      vivaldi::tabs_private::DragData* drag_data) {
  for (TabDragDataCollection::const_iterator it = data.begin();
       it != data.end(); ++it) {
    drag_data->mime_type.append(base::UTF16ToUTF8(it->first));
    drag_data->custom_data.append(base::UTF16ToUTF8(it->second));

    // Should be only a single element
    break;
  }
}

}  // namespace

blink::WebDragOperationsMask TabsPrivateEventRouter::OnDragEnd(
    int screen_x,
    int screen_y,
    blink::WebDragOperationsMask ops,
    const TabDragDataCollection& data,
    bool cancelled) {
  vivaldi::tabs_private::DragData drag_data;
  bool outside =
      ::vivaldi::ui_tools::IsOutsideAppWindow(screen_x, screen_y, profile_);

  // This should never happen.
  DCHECK(!data.empty());
  PopulateDragData(data, &drag_data);

  ::vivaldi::SetTabDragInProgress(false);

  std::unique_ptr<base::ListValue> args =
      vivaldi::tabs_private::OnDragEnd::Create(drag_data, cancelled, outside,
                                               screen_x, screen_y);

  DispatchEvent(extensions::events::VIVALDI_EXTENSION_EVENT,
                vivaldi::tabs_private::OnDragEnd::kEventName, std::move(args));

  return outside ? blink::kWebDragOperationNone : ops;
}

blink::WebDragOperationsMask TabsPrivateEventRouter::OnDragCursorUpdating(
    int screen_x,
    int screen_y,
    blink::WebDragOperationsMask ops) {
  return blink::WebDragOperationsMask::kWebDragOperationMove;
}

TabsPrivateEventRouter::TabsPrivateEventRouter(Profile* profile)
    : profile_(profile) {}

TabsPrivateEventRouter::~TabsPrivateEventRouter() {}

void TabsPrivateEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  event_router->BroadcastEvent(std::move(event));
}

/*static*/
void VivaldiPrivateTabObserver::BroadcastEvent(
    const std::string& eventname,
    std::unique_ptr<base::ListValue> args,
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
}

VivaldiPrivateTabObserver::~VivaldiPrivateTabObserver() {}

void VivaldiPrivateTabObserver::WebContentsDestroyed() {
}

void VivaldiPrivateTabObserver::OnPrefsChanged(const std::string& path) {
  if (path == vivaldiprefs::kWebpagesFocusTrap) {
    UpdateAllowTabCycleIntoUI();
    CommitSettings();
  }
}

void VivaldiPrivateTabObserver::BroadcastTabInfo() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  tabs_private::UpdateTabInfo info;
  info.show_images.reset(new bool(show_images()));
  info.load_from_cache_only.reset(new bool(load_from_cache_only()));
  info.enable_plugins.reset(new bool(enable_plugins()));
  info.mime_type.reset(new std::string(contents_mime_type()));
  int id = SessionTabHelper::IdForTab(web_contents()).id();

  std::unique_ptr<base::ListValue> args =
      tabs_private::OnTabUpdated::Create(id, info);
  BroadcastEvent(tabs_private::OnTabUpdated::kEventName, std::move(args),
                 profile);
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
  BroadcastEvent(tabs_private::OnThemeColorChanged::kEventName, std::move(args),
                 profile);
}

base::Value DictionaryToJSONString(const base::DictionaryValue& dict) {
  std::string json_string;
  base::JSONWriter::WriteWithOptions(dict, 0, &json_string);
  return base::Value(json_string);
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
  UpdateAllowTabCycleIntoUI();
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

  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  std::unique_ptr<base::Value> json = base::JSONReader::Read(ext, options);
  base::DictionaryValue* dict = NULL;
  if (json && json->GetAsDictionary(&dict)) {
    if (dict) {
      dict->SetDouble("vivaldi_tab_zoom", zoom_level);
      base::Value st = DictionaryToJSONString(*dict);
      web_contents()->SetExtData(st.GetString());
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
  CommitSettings();
}

void VivaldiPrivateTabObserver::UpdateAllowTabCycleIntoUI() {
  content::RendererPreferences* render_prefs =
    web_contents()->GetMutableRendererPrefs();
  DCHECK(render_prefs);
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (::vivaldi::IsVivaldiRunning()) {
    render_prefs->allow_tab_cycle_from_webpage_into_ui =
        !profile->GetPrefs()->GetBoolean(vivaldiprefs::kWebpagesFocusTrap);
  } else {
    // Avoid breaking tests.
    render_prefs->allow_tab_cycle_from_webpage_into_ui = true;
  }
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
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  renderer_preferences_util::UpdateFromSystemSettings(render_prefs, profile);

  if (load_from_cache_only_ && enable_plugins_) {
    render_prefs->should_ask_plugin_content = true;
  } else {
    render_prefs->should_ask_plugin_content = false;
  }
  web_contents()->GetRenderViewHost()->SyncRendererPrefs();
}

void VivaldiPrivateTabObserver::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  content::WebContents* web_contents = data.web_contents;
  content::StoragePartition* current_partition =
      content::BrowserContext::GetStoragePartition(
          web_contents->GetBrowserContext(), web_contents->GetSiteInstance(),
          false);
  if (current_partition &&
      current_partition == content::BrowserContext::GetDefaultStoragePartition(
                               web_contents->GetBrowserContext()))
    SetZoomLevelForTab(data.new_zoom_level);
}

void VivaldiPrivateTabObserver::SetZoomLevelForTab(double level) {
  if (level != tab_zoom_level_) {
    tab_zoom_level_ = level;
    SaveZoomLevelToExtData(level);
  }
}

bool VivaldiPrivateTabObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(VivaldiPrivateTabObserver, message)
  IPC_MESSAGE_HANDLER(VivaldiViewHostMsg_RequestThumbnailForFrame_ACK,
                      OnRequestThumbnailForFrameResponse)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VivaldiPrivateTabObserver::DocumentAvailableInMainFrame() {
  VivaldiPrivateTabObserver* tab_api =
      VivaldiPrivateTabObserver::FromWebContents(web_contents());
  DCHECK(tab_api);
  if (tab_api) {
    tab_api->SetContentsMimeType(web_contents()->GetContentsMimeType());
    tab_api->BroadcastTabInfo();
  }
}

void VivaldiPrivateTabObserver::CaptureTab(
    gfx::Size size,
    bool full_page,
    const CaptureTabDoneCallback& callback) {
  VivaldiViewMsg_RequestThumbnailForFrame_Params param;
  gfx::Rect rect = web_contents()->GetContainerBounds();

  param.callback_id = SessionTabHelper::IdForTab(web_contents()).id();
  if (full_page) {
    param.size = size;
  } else {
    param.size = rect.size();
  }
  capture_callback_ = callback;
  param.full_page = full_page;

  web_contents()->GetRenderViewHost()->Send(
      new VivaldiViewMsg_RequestThumbnailForFrame(
          web_contents()->GetRenderViewHost()->GetRoutingID(), param));
}

bool VivaldiPrivateTabObserver::IsCapturing() {
  return capture_callback_.is_null() ? false : true;
}

void VivaldiPrivateTabObserver::OnRequestThumbnailForFrameResponse(
    base::SharedMemoryHandle handle,
    const gfx::Size image_size,
    int callback_id,
    bool success) {
  if (!capture_callback_.is_null()) {
    capture_callback_.Run(handle, image_size, callback_id, success);
    capture_callback_.Reset();
  }
}

void VivaldiPrivateTabObserver::OnPermissionAccessed(
    ContentSettingsType content_settings_type, std::string origin,
    ContentSetting content_setting) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents());

  std::string type_name = base::ToLowerASCII(
      PermissionUtil::GetPermissionString(content_settings_type));

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

  std::unique_ptr<base::ListValue> args =
      vivaldi::tabs_private::OnPermissionAccessed::Create(tab_id, type_name,
                                                          origin, setting);

  Profile *profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  BroadcastEvent(tabs_private::OnPermissionAccessed::kEventName,
                 std::move(args), profile);
}

////////////////////////////////////////////////////////////////////////////////

TabsPrivateUpdateFunction::TabsPrivateUpdateFunction() {}

TabsPrivateUpdateFunction::~TabsPrivateUpdateFunction() {}

bool TabsPrivateUpdateFunction::RunAsync() {
  std::unique_ptr<tabs_private::Update::Params> params(
      tabs_private::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tabs_private::UpdateTabInfo* info = &params->tab_info;
  int tab_id = params->tab_id;

  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());
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
      tab_api->BroadcastTabInfo();
    }
  }
  SendResponse(true);
  return true;
}

TabsPrivateGetFunction::TabsPrivateGetFunction() {}

TabsPrivateGetFunction::~TabsPrivateGetFunction() {}

bool TabsPrivateGetFunction::RunAsync() {
  std::unique_ptr<tabs_private::Get::Params> params(
      tabs_private::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  tabs_private::UpdateTabInfo info;

  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());
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
  SendResponse(false);
  return false;
}

TabsPrivateInsertTextFunction::TabsPrivateInsertTextFunction() {}

TabsPrivateInsertTextFunction::~TabsPrivateInsertTextFunction() {}

bool TabsPrivateInsertTextFunction::RunAsync() {
  std::unique_ptr<tabs_private::InsertText::Params> params(
      tabs_private::InsertText::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = params->tab_id;
  bool success = false;

  base::string16 text = base::UTF8ToUTF16(params->text);

  content::WebContents* tabstrip_contents =
      ::vivaldi::ui_tools::GetWebContentsFromTabStrip(tab_id, GetProfile());

  content::RenderFrameHost* focused_frame =
      tabstrip_contents->GetFocusedFrame();

  if (focused_frame) {
    success = true;
    tabstrip_contents->GetRenderViewHost()->Send(new VivaldiMsg_InsertText(
        tabstrip_contents->GetRenderViewHost()->GetRoutingID(), text));
  }

  SendResponse(success);
  return success;
}

TabsPrivateStartDragFunction::TabsPrivateStartDragFunction() {}

TabsPrivateStartDragFunction::~TabsPrivateStartDragFunction() {}

bool TabsPrivateStartDragFunction::RunAsync() {
  std::unique_ptr<tabs_private::StartDrag::Params> params(
      tabs_private::StartDrag::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  SkBitmap bitmap;
  gfx::Vector2d image_offset;
  if (params->drag_image.get()) {
    std::string string_data;
    if (base::Base64Decode(params->drag_image->image, &string_data)) {
      const unsigned char* data =
          reinterpret_cast<const unsigned char*>(string_data.c_str());
      // Try PNG first.
      if (!gfx::PNGCodec::Decode(data, string_data.length(), &bitmap)) {
        // Try JPEG.
        std::unique_ptr<SkBitmap> decoded_jpeg(
            gfx::JPEGCodec::Decode(data, string_data.length()));
        if (decoded_jpeg) {
          bitmap = *decoded_jpeg;
        } else {
          LOG(WARNING) << "Error decoding png or jpg image data";
        }
      }
    } else {
      LOG(WARNING) << "Error decoding base64 image data";
    }
    image_offset.set_x(params->drag_image->cursor_x);
    image_offset.set_y(params->drag_image->cursor_y);
  }
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  DCHECK(browser);
  VivaldiBrowserWindow* window =
      static_cast<VivaldiBrowserWindow*>(browser->window());
  DCHECK(window);
  content::RenderViewHostImpl* rvh = static_cast<content::RenderViewHostImpl*>(
      window->web_contents()->GetRenderViewHost());
  DCHECK(rvh);
  content::RenderViewHostDelegateView* view =
      rvh->GetDelegate()->GetDelegateView();

  content::DropData drop_data;
  const base::string16 identifier(
      base::UTF8ToUTF16(params->drag_data.mime_type));
  const base::string16 custom_data(
      base::UTF8ToUTF16(params->drag_data.custom_data));

  drop_data.custom_data.insert(std::make_pair(identifier, custom_data));

  drop_data.url = GURL(params->drag_data.url);
  drop_data.url_title = base::UTF8ToUTF16(params->drag_data.title);

  blink::WebDragOperationsMask allowed_ops =
      static_cast<blink::WebDragOperationsMask>(
          blink::WebDragOperation::kWebDragOperationMove);

  gfx::ImageSkia image(gfx::ImageSkiaRep(bitmap, 1));
  content::DragEventSourceInfo event_info;

  event_info.event_source =
      params->is_from_touch.get() && *params->is_from_touch.get()
          ? ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH
          : ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  event_info.event_location =
      display::Screen::GetScreen()->GetCursorScreenPoint();

  ::vivaldi::SetTabDragInProgress(true);
  view->StartDragging(drop_data, allowed_ops, image, image_offset, event_info,
                      rvh->GetWidget());
  SendResponse(true);
  return true;
}

}  // namespace extensions
