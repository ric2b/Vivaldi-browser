// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "ui/devtools/devtools_connector.h"

#include <string>
#include <vector>
#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/renderer_context_menu/context_menu_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/devtools_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

DevtoolsConnectorAPI::DevtoolsConnectorAPI(content::BrowserContext* context)
    : browser_context_(context) {}

DevtoolsConnectorAPI::~DevtoolsConnectorAPI() {}

void DevtoolsConnectorAPI::Shutdown() {}

static base::LazyInstance<::extensions::BrowserContextKeyedAPIFactory<
    DevtoolsConnectorAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
::extensions::BrowserContextKeyedAPIFactory<DevtoolsConnectorAPI>*
DevtoolsConnectorAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

DevtoolsConnectorItem* DevtoolsConnectorAPI::GetOrCreateDevtoolsConnectorItem(
    int tab_id) {
  for (DevtoolsConnectorItem* item : connector_items_) {
    if (item->tab_id() == tab_id) {
      return item;
    }
  }
  DevtoolsConnectorItem* new_item =
      new DevtoolsConnectorItem(tab_id, browser_context_);
  connector_items_.push_back(new_item);

  return new_item;
}

void DevtoolsConnectorAPI::RemoveDevtoolsConnectorItem(int tab_id) {
  DevtoolsConnectorAPI::SendClosed(browser_context_, tab_id);

  for (std::vector<DevtoolsConnectorItem*>::iterator it =
           connector_items_.begin();
       it != connector_items_.end(); it++) {
    if ((*it)->tab_id() == tab_id) {
      connector_items_.erase(it);
      return;
    }
  }
  // Falltrough as both main and toolbox content refers to the same inspected
  // tab-id.
}

// static
void DevtoolsConnectorAPI::CloseAllDevtools() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (Profile* profile : profile_manager->GetLoadedProfiles()) {
    DCHECK(profile);
    CloseDevtoolsForBrowser(profile, nullptr);
  }
}

// static
void DevtoolsConnectorAPI::CloseDevtoolsForBrowser(
    content::BrowserContext* browser_context,
    Browser* closing_browser) {
  DevtoolsConnectorAPI* api = GetFactoryInstance()->Get(browser_context);
  if (!api) {
    // This can happen when closing windows.
    return;
  }
  content::WebContents* tabstrip_contents = nullptr;
  WindowController* browser;
  int tab_index;

  std::vector<DevtoolsConnectorItem*>::iterator it =
      api->connector_items_.begin();
  while (it != api->connector_items_.end()) {
    if (extensions::ExtensionTabUtil::GetTabById(
            (*it)->tab_id(), browser_context, true, &browser,
            &tabstrip_contents, &tab_index)) {
      if (closing_browser == nullptr || closing_browser == browser->GetBrowser()) {
        DevToolsWindow* window =
            DevToolsWindow::GetInstanceForInspectedWebContents(
                tabstrip_contents);
        if (window) {
          // This call removes the element from connector_items_.
          window->ForceCloseWindow();
          it = api->connector_items_.begin();
        } else return;
      } else {
        it++;
      }
    } else {
      it++;
    }
  }
}

// static
void DevtoolsConnectorAPI::SendOnUndockedEvent(
    content::BrowserContext* browser_context,
    int tab_id,
    bool show_window) {
  extensions::vivaldi::devtools_private::DevtoolsWindowParams params;
  bool need_defaults = true;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();
  const base::Value::Dict& pref_dict =
      prefs->GetDict(prefs::kAppWindowPlacement);
  if (const base::Value::Dict* state =
          pref_dict.FindDict(DevToolsWindow::kDevToolsApp)) {
    params.left = state->FindInt("left").value_or(0);
    params.top = state->FindInt("top").value_or(0);
    params.right = state->FindInt("right").value_or(0);
    params.bottom = state->FindInt("bottom").value_or(0);
    params.maximized = state->FindBool("maximized").value_or(false);
    params.always_on_top = state->FindBool("always_on_top").value_or(false);
    need_defaults = false;
  }
  if (need_defaults) {
    // Set defaults in prefs, based on DevToolsWindow::CreateDevToolsBrowser
    ScopedDictPrefUpdate update(prefs, prefs::kAppWindowPlacement);
    base::Value::Dict& wp_prefs = update.Get();
    base::Value::Dict dev_tools_defaults;
    dev_tools_defaults.Set("left", 100);
    dev_tools_defaults.Set("top", 100);
    dev_tools_defaults.Set("right", 740);
    dev_tools_defaults.Set("bottom", 740);
    dev_tools_defaults.Set("maximized", false);
    dev_tools_defaults.Set("always_on_top", false);

    wp_prefs.Set(DevToolsWindow::kDevToolsApp,
                            std::move(dev_tools_defaults));
  }

  ::vivaldi::BroadcastEvent(
      vivaldi::devtools_private::OnDevtoolsUndocked::kEventName,
      vivaldi::devtools_private::OnDevtoolsUndocked::Create(tab_id, show_window,
                                                            params),
      browser_context);
}

// static
void DevtoolsConnectorAPI::SendDockingStateChanged(
    content::BrowserContext* browser_context,
    int tab_id,
    const std::string& docking_state) {
  ::vivaldi::BroadcastEvent(
      vivaldi::devtools_private::OnDockingStateChanged::kEventName,
      vivaldi::devtools_private::OnDockingStateChanged::Create(tab_id,
                                                               docking_state),
      browser_context);
}

// static
void DevtoolsConnectorAPI::SendClosed(content::BrowserContext* browser_context,
                                      int tab_id) {
  ::vivaldi::BroadcastEvent(vivaldi::devtools_private::OnClosed::kEventName,
                            vivaldi::devtools_private::OnClosed::Create(tab_id),
                            browser_context);
}

DevtoolsConnectorItem::DevtoolsConnectorItem(int tab_id,
                                             content::BrowserContext* context)
    : tab_id_(tab_id), browser_context_(context) {}

DevtoolsConnectorItem::~DevtoolsConnectorItem() {
  extensions::DevtoolsConnectorAPI* api =
      extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
          Profile::FromBrowserContext(browser_context_));
  DCHECK(api);

  api->RemoveDevtoolsConnectorItem(tab_id());
}

void DevtoolsConnectorItem::ResetDockingState() {
  devtools_docking_state_ = "off";
}

// content::WebContentsDelegate:
void DevtoolsConnectorItem::ActivateContents(content::WebContents* contents) {
  if (devtools_delegate_) {
    devtools_delegate_->ActivateContents(contents);
  }
  // guest view does not need notification, WebContents gets it above.
}

content::WebContents* DevtoolsConnectorItem::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (devtools_delegate_) {
    // The webview is called in devtools_delegate_
    return devtools_delegate_->AddNewContents(source, std::move(new_contents),
                                       target_url, disposition, window_features,
                                       user_gesture, was_blocked);
  }

  return nullptr;
}

void DevtoolsConnectorItem::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  if (devtools_delegate_) {
    devtools_delegate_->WebContentsCreated(
        source_contents, opener_render_process_id, opener_render_frame_id,
        frame_name, target_url, new_contents);
  }
}

void DevtoolsConnectorItem::CloseContents(content::WebContents* source) {
  if (devtools_delegate_) {
    devtools_delegate_->CloseContents(source);
    // at this point, |this| is not longer valid.
  }
}

void DevtoolsConnectorItem::ContentsZoomChange(bool zoom_in) {
  if (devtools_delegate_) {
    devtools_delegate_->ContentsZoomChange(zoom_in);
  }
}

void DevtoolsConnectorItem::BeforeUnloadFired(content::WebContents* tab,
                                              bool proceed,
                                              bool* proceed_to_fire_unload) {
  if (devtools_delegate_) {
    devtools_delegate_->BeforeUnloadFired(tab, proceed, proceed_to_fire_unload);
  }
}

content::KeyboardEventProcessingResult
DevtoolsConnectorItem::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  content::KeyboardEventProcessingResult handled =
      content::KeyboardEventProcessingResult::NOT_HANDLED;

  if (devtools_delegate_) {
    handled = devtools_delegate_->PreHandleKeyboardEvent(source, event);
  }
  return handled;
}

bool DevtoolsConnectorItem::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (devtools_delegate_) {
    return devtools_delegate_->HandleContextMenu(render_frame_host, params);
  }
  return false;
}

#if BUILDFLAG(IS_MAC)
const std::vector<std::string> commands_to_fwd = {
  "COMMAND_CLOSE_TAB",
  "COMMAND_CLOSE_WINDOW",
  "COMMAND_DEVELOPER_TOOLS",
  "COMMAND_DEVTOOLS_CONSOLE",
  "COMMAND_DEVTOOLS_INSPECTOR",
  "COMMAND_NEW_TAB",
  "COMMAND_NEW_BACKGROUND_TAB",
  "COMMAND_NEW_PRIVATE_WINDOW",
  "COMMAND_NEW_WINDOW",
  "COMMAND_QUIT_MAC_MAYBE_WARN",
  "COMMAND_CLIPBOARD_COPY",
  "COMMAND_CLIPBOARD_CUT",
  "COMMAND_CLIPBOARD_PASTE",
  "COMMAND_CLIPBOARD_PASTE_AS_PLAIN_TEXT"
};

bool ShouldForwardKeyCombo(std::string shortcut_text,
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  PrefService* prefs = profile->GetPrefs();
  auto& vivaldi_actions = prefs->GetList(vivaldiprefs::kActions);
  const base::Value::Dict* dict = vivaldi_actions[0].GetIfDict();

  for (int i = 0; i < (int)commands_to_fwd.size(); i++) {
    const base::Value::Dict* shortcut = dict->FindDict(commands_to_fwd.at(i));
    if (!shortcut) {
      continue;
    }
    for (const auto [name, keycombo] : *shortcut) {
      if (name == "shortcuts") {
        for (const base::Value& kc : keycombo.GetList()) {
          if (shortcut_text == *kc.GetIfString()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}
#endif

bool DevtoolsConnectorItem::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // NOTE(david@vivaldi.com): With SHIFT+CTRL+I we are now able to debug dev
  // tools in undocked state.
  const int modifier_mask =
      blink::WebInputEvent::kShiftKey | blink::WebInputEvent::kControlKey;
  if (devtools_docking_state_ == "undocked" &&
      (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) &&
      ((event.GetModifiers() & modifier_mask) == modifier_mask)) {
    if (event.windows_key_code == ui::VKEY_I) {
      DevToolsWindow::OpenDevToolsWindow(
          source, DevToolsOpenedByAction::kMainMenuOrMainShortcut);
    }
  }

#if BUILDFLAG(IS_MAC)
  if ((event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) &&
      ((event.windows_key_code != ui::VKEY_CONTROL &&
      event.windows_key_code != ui::VKEY_MENU &&
      event.windows_key_code != ui::VKEY_SHIFT &&
      event.windows_key_code != ui::VKEY_COMMAND &&
      event.GetModifiers() > 0) ||
      (ui::VKEY_F1 <= event.windows_key_code &&
      event.windows_key_code <= ui::VKEY_F12))) {
    std::string shortcut_text =
        base::ToLowerASCII(::vivaldi::ShortcutTextFromEvent(event));
    if (ShouldForwardKeyCombo(shortcut_text, browser_context_)) {
      input::NativeWebKeyboardEvent new_event(event);
      new_event.from_devtools = true;
      return devtools_delegate_->HandleKeyboardEvent(source, new_event);
    }
  }
#endif

  // Do not pass on keyboard events to the delegate, aka. our BrowserWindow,
  // so we no longer need special handling of shortcuts when devtools is running
  // docked as shortcuts entered in devtools are no longer sent to our shortcut
  // handling code.
  return false;
}

content::JavaScriptDialogManager*
DevtoolsConnectorItem::GetJavaScriptDialogManager(
    content::WebContents* source) {
  if (devtools_delegate_) {
    return devtools_delegate_->GetJavaScriptDialogManager(source);
  }
  NOTREACHED();
  //return nullptr;
}

void DevtoolsConnectorItem::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  if (devtools_delegate_) {
    return devtools_delegate_->RunFileChooser(render_frame_host, std::move(listener),
                                       params);
  }
  NOTREACHED();
}

bool DevtoolsConnectorItem::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  if (devtools_delegate_) {
    return devtools_delegate_->PreHandleGestureEvent(source, event);
  }
  NOTREACHED();
  //return true;
}

content::WebContents* DevtoolsConnectorItem::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (devtools_delegate_) {
    return devtools_delegate_->OpenURLFromTab(
        source, params, std::move(navigation_handle_callback));
  }
  NOTREACHED();
  //return nullptr;
}

std::unique_ptr<content::EyeDropper> DevtoolsConnectorItem::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (devtools_delegate_) {
    return devtools_delegate_->OpenEyeDropper(frame, listener);
  }
  NOTREACHED();
  //return nullptr;
}

// DevToolsUIBindings::Delegate implementation
UIBindingsDelegate::UIBindingsDelegate(content::BrowserContext* browser_context,
                                       int tab_id,
                                       DevToolsUIBindings::Delegate* delegate)
    : tab_id_(tab_id), browser_context_(browser_context) {
  ui_bindings_delegate_.reset(delegate);
}

UIBindingsDelegate::~UIBindingsDelegate() {}

void UIBindingsDelegate::ActivateWindow() {
  ::vivaldi::BroadcastEvent(
      vivaldi::devtools_private::OnActivateWindow::kEventName,
      vivaldi::devtools_private::OnActivateWindow::Create(tab_id()),
      browser_context_);
}

void UIBindingsDelegate::NotifyUpdateBounds() {
  // Notify the js side to update bounds.
  ::vivaldi::BroadcastEvent(
      vivaldi::devtools_private::OnDockingSizesChanged::kEventName,
      vivaldi::devtools_private::OnDockingSizesChanged::Create(tab_id()),
      browser_context_);
}

void UIBindingsDelegate::CloseWindow() {
  DevtoolsConnectorAPI::SendClosed(browser_context_, tab_id());

  // We need to let VivaldiBrowserWindow for this tab know that we've closed
  // devtools now.
  // TODO(pettern): Not very elegant, find a better way?
  content::WebContents* tabstrip_contents = NULL;
  bool include_incognito = true;
  WindowController* browser;
  int tab_index;

  if (extensions::ExtensionTabUtil::GetTabById(
          tab_id(), browser_context_, include_incognito, &browser,
          &tabstrip_contents, &tab_index)) {
    VivaldiBrowserWindow* window =
        static_cast<VivaldiBrowserWindow*>(browser->window());
    window->ResetDockingState(tab_id());
  }
  NotifyUpdateBounds();

  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->CloseWindow();
  }
}

void UIBindingsDelegate::Inspect(
    scoped_refptr<content::DevToolsAgentHost> host) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->Inspect(host);
  }
}

void UIBindingsDelegate::SetInspectedPageBounds(const gfx::Rect& rect) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->SetInspectedPageBounds(rect);
  }
  NotifyUpdateBounds();
}

void UIBindingsDelegate::InspectElementCompleted() {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->InspectElementCompleted();
  }
}

void UIBindingsDelegate::SetIsDocked(bool is_docked) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->SetIsDocked(is_docked);
  }
  NotifyUpdateBounds();
}

void UIBindingsDelegate::OpenInNewTab(const std::string& url) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->OpenInNewTab(url);
  }
}

void UIBindingsDelegate::OpenSearchResultsInNewTab(const std::string& query) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->OpenSearchResultsInNewTab(query);
  }
}

void UIBindingsDelegate::SetWhitelistedShortcuts(const std::string& message) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->SetWhitelistedShortcuts(message);
  }
}

void UIBindingsDelegate::InspectedContentsClosing() {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->InspectedContentsClosing();
  }
}

void UIBindingsDelegate::OnLoadCompleted() {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->OnLoadCompleted();
  }
}

void UIBindingsDelegate::OpenNodeFrontend() {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->OpenNodeFrontend();
  }
}

void UIBindingsDelegate::ReadyForTest() {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->ReadyForTest();
  }
}

infobars::ContentInfoBarManager* UIBindingsDelegate::GetInfoBarManager() {
  if (ui_bindings_delegate_) {
    return ui_bindings_delegate_->GetInfoBarManager();
  }
  return nullptr;
}

void UIBindingsDelegate::RenderProcessGone(bool crashed) {
  if (ui_bindings_delegate_) {
    return ui_bindings_delegate_->RenderProcessGone(crashed);
  }
}

void UIBindingsDelegate::SetEyeDropperActive(bool active) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->SetEyeDropperActive(active);
  }
}

void UIBindingsDelegate::ShowCertificateViewer(const std::string& cert_chain) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->ShowCertificateViewer(cert_chain);
  }
}

void UIBindingsDelegate::ConnectionReady() {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->ConnectionReady();
  }
}

void UIBindingsDelegate::SetOpenNewWindowForPopups(bool value) {
  if (ui_bindings_delegate_) {
    ui_bindings_delegate_->SetOpenNewWindowForPopups(value);
  }
}

int UIBindingsDelegate::GetDockStateForLogging() {
  if (ui_bindings_delegate_) {
    return ui_bindings_delegate_->GetDockStateForLogging();
  }
  return 0; // kUndocked
}
int UIBindingsDelegate::GetOpenedByForLogging() {
  if (ui_bindings_delegate_) {
    return ui_bindings_delegate_->GetDockStateForLogging();
  }
  return 0; // kUndocked
}
int UIBindingsDelegate::GetClosedByForLogging() {
  if (ui_bindings_delegate_) {
    return ui_bindings_delegate_->GetDockStateForLogging();
  }
  return 0; // kUndocked
}


}  // namespace extensions
