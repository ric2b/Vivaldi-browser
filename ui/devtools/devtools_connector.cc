// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "ui/devtools/devtools_connector.h"

#include <string>
#include <vector>
#include "base/lazy_instance.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/devtools_private.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"

namespace extensions {

DevtoolsConnectorAPI::DevtoolsConnectorAPI(content::BrowserContext* context)
  : browser_context_(context) {
}

DevtoolsConnectorAPI::~DevtoolsConnectorAPI() {
}

void DevtoolsConnectorAPI::Shutdown() {
}

static base::LazyInstance<
    ::extensions::BrowserContextKeyedAPIFactory<DevtoolsConnectorAPI>>::
      DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

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
  for (std::vector<DevtoolsConnectorItem*>::iterator it =
           connector_items_.begin();
       it != connector_items_.end(); it++) {
    if ((*it)->tab_id() == tab_id) {
      connector_items_.erase(it);
      return;
    }
  }
}

void DevtoolsConnectorAPI::CloseAllDevtools() {
  CloseDevtoolsForBrowser(nullptr);
}

void DevtoolsConnectorAPI::CloseDevtoolsForBrowser(Browser* closing_browser) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  content::WebContents* tabstrip_contents = nullptr;
  Browser* browser;
  int tab_index;

  std::vector<DevtoolsConnectorItem*>::iterator it = connector_items_.begin();
  while (it != connector_items_.end()) {
    if (extensions::ExtensionTabUtil::GetTabById(
            (*it)->tab_id(), profile, true, &browser, nullptr,
            &tabstrip_contents, &tab_index)) {
      if (closing_browser == nullptr || closing_browser == browser) {
        DevToolsWindow* window =
            DevToolsWindow::GetInstanceForInspectedWebContents(
                tabstrip_contents);
        if (window) {
          // This call removes the element from connector_items_.
          window->ForceCloseWindow();
          it = connector_items_.begin();
        }
      } else {
        it++;
      }
    } else {
      it++;
    }
  }
}

void DevtoolsConnectorAPI::SendOnUndockedEvent(
    content::BrowserContext* context,
    int tab_id, bool show_window) {
  extensions::vivaldi::devtools_private::DevtoolsWindowParams params;
  bool need_defaults = true;
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();
  if (prefs->GetDictionary(prefs::kAppWindowPlacement)->
      HasKey(DevToolsWindow::kDevToolsApp)) {
    const base::DictionaryValue* dict =
        prefs->GetDictionary(prefs::kAppWindowPlacement);
    const base::DictionaryValue* state = nullptr;
    if (dict && dict->GetDictionary(DevToolsWindow::kDevToolsApp, &state)) {
        state->GetInteger("left", &params.left);
        state->GetInteger("top", &params.top);
        state->GetInteger("right", &params.right);
        state->GetInteger("bottom", &params.bottom);
        state->GetBoolean("maximized", &params.maximized);
        state->GetBoolean("always_on_top", &params.always_on_top);
        need_defaults = false;
    }
  }
  if (need_defaults) {
    // Set defaults in prefs, based on DevToolsWindow::CreateDevToolsBrowser
    DictionaryPrefUpdate update(prefs, prefs::kAppWindowPlacement);
    base::DictionaryValue* wp_prefs = update.Get();
    std::unique_ptr<base::DictionaryValue>
          dev_tools_defaults(new base::DictionaryValue);
    dev_tools_defaults->SetInteger("left", 100);
    dev_tools_defaults->SetInteger("top", 100);
    dev_tools_defaults->SetInteger("right", 740);
    dev_tools_defaults->SetInteger("bottom", 740);
    dev_tools_defaults->SetBoolean("maximized", false);
    dev_tools_defaults->SetBoolean("always_on_top", false);

    wp_prefs->Set(DevToolsWindow::kDevToolsApp, std::move(dev_tools_defaults));
  }

  std::unique_ptr<base::ListValue> args =
    vivaldi::devtools_private::
      OnDevtoolsUndocked::Create(tab_id, show_window, params);

  extensions::DevtoolsConnectorAPI::BroadcastEvent(
    vivaldi::devtools_private::OnDevtoolsUndocked::kEventName,
    std::move(args), context);
}

/* static */
void DevtoolsConnectorAPI::BroadcastEvent(const std::string& eventname,
                                          std::unique_ptr<base::ListValue> args,
                                          content::BrowserContext* context) {
  std::unique_ptr<Event> event(
      new Event(events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args),
                context));
  EventRouter* event_router = EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}

DevtoolsConnectorItem::DevtoolsConnectorItem(int tab_id,
                                             content::BrowserContext* context)
    : tab_id_(tab_id), browser_context_(context) {
}

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

void DevtoolsConnectorItem::AddNewContents(content::WebContents* source,
                                           std::unique_ptr<content::WebContents> new_contents,
                                           WindowOpenDisposition disposition,
                                           const gfx::Rect& initial_rect,
                                           bool user_gesture,
                                           bool* was_blocked) {
  if (devtools_delegate_) {
    devtools_delegate_->AddNewContents(source, std::move(new_contents), disposition,
                                       initial_rect, user_gesture, was_blocked);
  }
  if (guest_delegate_) {
    guest_delegate_->AddNewContents(source, std::move(new_contents), disposition,
                                    initial_rect, user_gesture, was_blocked);
  }
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
  if (guest_delegate_) {
    guest_delegate_->WebContentsCreated(
      source_contents, opener_render_process_id, opener_render_frame_id,
      frame_name, target_url, new_contents);
  }
}

void DevtoolsConnectorItem::CloseContents(content::WebContents* source) {
  if (devtools_delegate_) {
    devtools_delegate_->CloseContents(source);
  }
  if (guest_delegate_) {
    guest_delegate_->CloseContents(source);
  }
}

void DevtoolsConnectorItem::ContentsZoomChange(bool zoom_in) {
  if (devtools_delegate_) {
    devtools_delegate_->ContentsZoomChange(zoom_in);
  }
  if (guest_delegate_) {
    guest_delegate_->ContentsZoomChange(zoom_in);
  }
}

void DevtoolsConnectorItem::BeforeUnloadFired(content::WebContents* tab,
                                              bool proceed,
                                              bool* proceed_to_fire_unload) {
  if (devtools_delegate_) {
    devtools_delegate_->BeforeUnloadFired(tab, proceed, proceed_to_fire_unload);
  }
  if (guest_delegate_) {
    guest_delegate_->BeforeUnloadFired(tab, proceed, proceed_to_fire_unload);
  }
}

content::KeyboardEventProcessingResult
DevtoolsConnectorItem::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  content::KeyboardEventProcessingResult handled =
      content::KeyboardEventProcessingResult::NOT_HANDLED;

  if (devtools_delegate_) {
    handled = devtools_delegate_->PreHandleKeyboardEvent(source, event);
  }
  if (guest_delegate_ &&
      handled != content::KeyboardEventProcessingResult::NOT_HANDLED) {
    handled = guest_delegate_->PreHandleKeyboardEvent(source, event);
  }
  return handled;
}

bool DevtoolsConnectorItem::HasOwnerShipOfContents() {
  return false;
}

void DevtoolsConnectorItem::HandleKeyboardEvent(
  content::WebContents* source,
  const content::NativeWebKeyboardEvent& event) {
  if (devtools_delegate_) {
    devtools_delegate_->HandleKeyboardEvent(source, event);
  }
}

content::JavaScriptDialogManager*
DevtoolsConnectorItem::GetJavaScriptDialogManager(
    content::WebContents* source) {
  if (guest_delegate_) {
    return guest_delegate_->GetJavaScriptDialogManager(source);
  }
  if (devtools_delegate_) {
    return devtools_delegate_->GetJavaScriptDialogManager(source);
  }
  NOTREACHED();
  return nullptr;
}

content::ColorChooser* DevtoolsConnectorItem::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) {
  if (devtools_delegate_) {
    return devtools_delegate_->OpenColorChooser(web_contents, color,
                                                suggestions);
  } else if (guest_delegate_) {
    return guest_delegate_->OpenColorChooser(web_contents, color, suggestions);
  }
  NOTREACHED();
  return nullptr;
}

void DevtoolsConnectorItem::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    const content::FileChooserParams& params) {
  if (devtools_delegate_) {
    devtools_delegate_->RunFileChooser(render_frame_host, params);
  } else if (guest_delegate_) {
    guest_delegate_->RunFileChooser(render_frame_host, params);
  }
  NOTREACHED();
}

bool DevtoolsConnectorItem::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  if (devtools_delegate_) {
    return devtools_delegate_->PreHandleGestureEvent(source, event);
  } else if (guest_delegate_) {
    return guest_delegate_->PreHandleGestureEvent(source, event);
  }
  NOTREACHED();
  return true;
}

// DevToolsUIBindings::Delegate implementation
UIBindingsDelegate::UIBindingsDelegate(content::BrowserContext* browser_context,
                                       int tab_id,
                                       DevToolsUIBindings::Delegate* delegate)
    : tab_id_(tab_id), browser_context_(browser_context) {
  ui_bindings_delegate_.reset(delegate);
}

UIBindingsDelegate::~UIBindingsDelegate() {
}

void UIBindingsDelegate::ActivateWindow() {
}

void UIBindingsDelegate::NotifyUpdateBounds() {
  // Notify the js side to update bounds.
  std::unique_ptr<base::ListValue> args =
    vivaldi::devtools_private::OnDockingSizesChanged::Create(tab_id());

  DevtoolsConnectorAPI::BroadcastEvent(
    vivaldi::devtools_private::OnDockingSizesChanged::kEventName,
    std::move(args), Profile::FromBrowserContext(browser_context_));
}

void UIBindingsDelegate::CloseWindow() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  std::unique_ptr<base::ListValue> args =
      vivaldi::devtools_private::OnClosed::Create(tab_id());

  DevtoolsConnectorAPI::BroadcastEvent(
      vivaldi::devtools_private::OnClosed::kEventName, std::move(args),
      profile);

  // We need to let VivaldiBrowserWindow for this tab know that we've closed
  // devtools now.
  // TODO(pettern): Not very elegant, find a better way?
  content::WebContents* tabstrip_contents = NULL;
  bool include_incognito = true;
  Browser* browser;
  int tab_index;

  if (extensions::ExtensionTabUtil::GetTabById(
      tab_id(), profile, include_incognito, &browser, NULL,
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

void UIBindingsDelegate::SetInspectedPageBounds(
    const gfx::Rect& rect) {
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

InfoBarService* UIBindingsDelegate::GetInfoBarService() {
  if (ui_bindings_delegate_) {
    return ui_bindings_delegate_->GetInfoBarService();
  }
  return nullptr;
}

void UIBindingsDelegate::RenderProcessGone(
    bool crashed) {
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

}  // namespace extensions
