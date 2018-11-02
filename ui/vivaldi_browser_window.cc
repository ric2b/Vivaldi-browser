// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_browser_window.h"

#include <memory>
#include <string>

#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/schema/devtools_private.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/jumplist_factory.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

// VivaldiBrowserWindow --------------------------------------------------------

VivaldiBrowserWindow::VivaldiBrowserWindow() : bounds_(gfx::Rect()) {}

VivaldiBrowserWindow::~VivaldiBrowserWindow() {
  // Explicitly set browser_ to NULL.
  browser_.reset();
}

void VivaldiBrowserWindow::Init(Browser* browser) {
  browser_.reset(browser);
  SetBounds(browser->override_bounds());
#if defined(OS_WIN)
  JumpListFactory::GetForProfile(browser_->profile());
#endif
}

// static
VivaldiBrowserWindow* VivaldiBrowserWindow::GetBrowserWindowForBrowser(
    const Browser* browser) {
  return static_cast<VivaldiBrowserWindow*>(browser->window());
}

// static
VivaldiBrowserWindow* VivaldiBrowserWindow::CreateVivaldiBrowserWindow(
    Browser* browser) {
  // Create the view and the frame. The frame will attach itself via the view
  // so we don't need to do anything with the pointer.
  VivaldiBrowserWindow* vview = new VivaldiBrowserWindow();
  vview->Init(browser);
  return vview;
}

void VivaldiBrowserWindow::Show() {
#if !defined(OS_WIN)
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behavior under
  // Windows and Ash, but other platforms will not trigger
  // OnWidgetActivationChanged() until we return to the runloop. Therefore any
  // calls to Browser::GetLastActive() will return the wrong result if we do not
  // explicitly set it here.
  // A similar block also appears in BrowserWindowCocoa::Show().
  BrowserList::SetLastActive(browser());
#endif
}

void VivaldiBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  return;
}

void VivaldiBrowserWindow::Close() {
  // gisli@vivaldi.com: Code based on BrowserView::CanClose();

  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return;

  bool fast_tab_closing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFastUnload);

  if (!browser_->tab_strip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    browser_->OnWindowClosing();
    if (fast_tab_closing_enabled)
      browser_->tab_strip_model()->CloseAllTabs();
    return;
  } else if (fast_tab_closing_enabled &&
             !browser_->HasCompletedUnloadProcessing()) {
    // The browser needs to finish running unload handlers.
    // Hide the frame (so it appears to have closed immediately), and
    // the browser will call us back again when it is ready to close.
    return;
  }

  delete this;
}

void VivaldiBrowserWindow::Activate() {
  extensions::AppWindow* app_window = GetAppWindow();
  if (app_window) {
    app_window->GetBaseWindow()->Activate();
  }
  BrowserList::SetLastActive(browser_.get());
}

void VivaldiBrowserWindow::Deactivate() {}

bool VivaldiBrowserWindow::IsActive() const {
  Browser* active = BrowserList::GetInstance()->GetLastActive();
  return (active && active->window() == this);
}

bool VivaldiBrowserWindow::IsAlwaysOnTop() const {
  return false;
}

extensions::AppWindow* VivaldiBrowserWindow::GetAppWindow() const {
  extensions::AppWindow* app_window = NULL;

  extensions::AppWindowRegistry* appwinreg =
      extensions::AppWindowRegistry::Get(browser_->profile());
  content::WebContentsImpl* web_contents =
      static_cast<content::WebContentsImpl*>(
          browser_->tab_strip_model()->GetActiveWebContents());
  if (!!web_contents) {
    content::BrowserPluginGuest* guestplugin =
        web_contents->GetBrowserPluginGuest();
    if (!!guestplugin) {
      content::WebContents* embedder_web_contents =
          guestplugin->embedder_web_contents();
      if (embedder_web_contents) {
        app_window =
            appwinreg->GetAppWindowForWebContents(embedder_web_contents);
      }
    }
  }
  if (!app_window) {
    std::unique_ptr<base::Value> value(
        base::JSONReader::Read(browser_->ext_data()));
    base::DictionaryValue* dictionary;
    if (value && value->GetAsDictionary(&dictionary)) {
      std::string windowid;
      if (dictionary->GetString("ext_id", &windowid)) {
        windowid.insert(0, "vivaldi_window_");  // This is added in the client.
        app_window = appwinreg->GetAppWindowForAppAndKey(vivaldi::kVivaldiAppId,
                                                         windowid);
      }
    }
  }
  return app_window;
}

gfx::NativeWindow VivaldiBrowserWindow::GetNativeWindow() const {
  extensions::AppWindow* app_window = GetAppWindow();
  if (app_window) {
    return app_window->GetNativeWindow();
  }
  return NULL;
}

StatusBubble* VivaldiBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect VivaldiBrowserWindow::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::WindowShowState VivaldiBrowserWindow::GetRestoredState() const {
  return ui::SHOW_STATE_DEFAULT;
}

gfx::Rect VivaldiBrowserWindow::GetBounds() const {
  return bounds_;
}

bool VivaldiBrowserWindow::IsMaximized() const {
  return false;
}

bool VivaldiBrowserWindow::IsMinimized() const {
  return false;
}

bool VivaldiBrowserWindow::ShouldHideUIForFullscreen() const {
  return false;
}

bool VivaldiBrowserWindow::IsFullscreen() const {
  return false;
}

bool VivaldiBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

LocationBar* VivaldiBrowserWindow::GetLocationBar() const {
  return NULL;
}

ToolbarActionsBar* VivaldiBrowserWindow::GetToolbarActionsBar() {
  return NULL;
}

content::KeyboardEventProcessingResult
VivaldiBrowserWindow::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool VivaldiBrowserWindow::IsBookmarkBarVisible() const {
  return false;
}

bool VivaldiBrowserWindow::IsBookmarkBarAnimating() const {
  return false;
}

bool VivaldiBrowserWindow::IsTabStripEditable() const {
  return true;
}

bool VivaldiBrowserWindow::IsToolbarVisible() const {
  return false;
}

bool VivaldiBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* VivaldiBrowserWindow::GetDownloadShelf() {
  return NULL;
}

// See comments on: BrowserWindow.VivaldiShowWebSiteSettingsAt.
void VivaldiBrowserWindow::VivaldiShowWebsiteSettingsAt(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const security_state::SecurityInfo& security_info,
    gfx::Point pos) {
#if defined(USE_AURA)
  // This is only for AURA.  Mac is done in VivaldiBrowserCocoa.
  PageInfoBubbleView::ShowPopupAtPos(pos, profile, web_contents, url,
                                           security_info, browser_.get(),
                                           GetAppWindow()->GetNativeWindow());
#endif
}

WindowOpenDisposition VivaldiBrowserWindow::GetDispositionForPopupBounds(
    const gfx::Rect& bounds) {
  return WindowOpenDisposition::NEW_POPUP;
}

FindBar* VivaldiBrowserWindow::CreateFindBar() {
  return NULL;
}

web_modal::WebContentsModalDialogHost*
VivaldiBrowserWindow::GetWebContentsModalDialogHost() {
  return NULL;
}

int VivaldiBrowserWindow::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  return 0;
}

void VivaldiBrowserWindow::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {}

ExclusiveAccessContext* VivaldiBrowserWindow::GetExclusiveAccessContext() {
  return this;
}

autofill::SaveCardBubbleView* VivaldiBrowserWindow::ShowSaveCreditCardBubble(
    content::WebContents* contents,
    autofill::SaveCardBubbleController* controller,
    bool is_user_gesture) {
  return NULL;
}

void VivaldiBrowserWindow::DestroyBrowser() {
  delete this;
}

gfx::Size VivaldiBrowserWindow::GetContentsSize() const {
  return gfx::Size();
}

std::string VivaldiBrowserWindow::GetWorkspace() const {
  return std::string();
}

bool VivaldiBrowserWindow::IsVisibleOnAllWorkspaces() const {
  return false;
}

Profile* VivaldiBrowserWindow::GetProfile() {
  return browser_->profile();
}

void VivaldiBrowserWindow::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {}

void VivaldiBrowserWindow::ExitFullscreen() {}

void VivaldiBrowserWindow::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type) {}

void VivaldiBrowserWindow::OnExclusiveAccessUserInput() {}

content::WebContents* VivaldiBrowserWindow::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void VivaldiBrowserWindow::UnhideDownloadShelf() {}

void VivaldiBrowserWindow::HideDownloadShelf() {}

ShowTranslateBubbleResult VivaldiBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID;
}

void VivaldiBrowserWindow::UpdateDevTools() {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();

  // Get the docking state.
  const base::DictionaryValue* prefs =
      browser_->profile()->GetPrefs()->GetDictionary(
          prefs::kDevToolsPreferences);

  std::string docking_state;
  std::string device_mode;

  // DevToolsWindow code has already activated the tab.
  content::WebContents* contents = tab_strip_model->GetActiveWebContents();
  int tab_id = SessionTabHelper::IdForTab(contents);
  extensions::DevtoolsConnectorAPI* api =
    extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
      browser_->profile());
  DCHECK(api);
  scoped_refptr<extensions::DevtoolsConnectorItem> item =
      make_scoped_refptr(api->GetOrCreateDevtoolsConnectorItem(tab_id));

  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);

  if (window) {
    if (window->IsClosing()) {
      std::unique_ptr<base::ListValue> args =
          extensions::vivaldi::devtools_private::OnClosed::Create(tab_id);

      extensions::DevtoolsConnectorAPI::BroadcastEvent(
          extensions::vivaldi::devtools_private::OnClosed::kEventName,
          std::move(args), browser_->profile());

      ResetDockingState(tab_id);
    } else {
      if (prefs->GetString("currentDockState", &docking_state)) {
        // Strip quotation marks from the state.
        base::ReplaceChars(docking_state, "\"", "", &docking_state);
        if (item->docking_state() != docking_state) {
          item->set_docking_state(docking_state);

          std::unique_ptr<base::ListValue> args =
            extensions::vivaldi::devtools_private::OnDockingStateChanged::Create(
              tab_id, docking_state);

          extensions::DevtoolsConnectorAPI::BroadcastEvent(
            extensions::vivaldi::devtools_private::OnDockingStateChanged::
            kEventName,
            std::move(args), browser_->profile());
        }
      }
      if (prefs->GetString("showDeviceMode", &device_mode)) {
        base::ReplaceChars(device_mode, "\"", "", &device_mode);
        bool device_mode_enabled = device_mode == "true";
        if (item->device_mode_enabled() == device_mode_enabled) {
          item->set_device_mode_enabled(device_mode_enabled);
        }
      }
    }
  }
}

void VivaldiBrowserWindow::ResetDockingState(int tab_id) {
  extensions::DevtoolsConnectorAPI* api =
    extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
      browser_->profile());
  DCHECK(api);
  scoped_refptr<extensions::DevtoolsConnectorItem> item =
      make_scoped_refptr(api->GetOrCreateDevtoolsConnectorItem(tab_id));

  item->ResetDockingState();

  std::unique_ptr<base::ListValue> args =
    extensions::vivaldi::devtools_private::OnDockingStateChanged::Create(
      tab_id, item->docking_state());

  extensions::DevtoolsConnectorAPI::BroadcastEvent(
    extensions::vivaldi::devtools_private::OnDockingStateChanged::
    kEventName,
    std::move(args),
    browser_->profile());
}

bool VivaldiBrowserWindow::IsToolbarShowing() const {
  return false;
}

void VivaldiBrowserWindow::OnActiveTabChanged(
    content::WebContents *old_contents, content::WebContents *new_contents,
    int index, int reason) {
#if !defined(OS_MACOSX)

  // Make sure we do a re-layout to keep all webviews in size-sync. Scenario; if
  // the user switch tab in fullscreen mode, only
  // BrowserPlugin::UpdateVisibility is called. We need to make sure
  // BrowserPlugin::UpdateGeometry is called. Note this was not visible prior to
  // the change we have in BrowserPlugin::UpdateGeometry where a lot of extra
  // resizing on hidden WebViews was done, and we want to be lazy. See VB-29032.

  content::WebContentsImpl *web_contents =
      static_cast<content::WebContentsImpl *>(new_contents);
  content::BrowserPluginGuest *guestplugin =
      web_contents->GetBrowserPluginGuest();
  extensions::AppWindow* app_window = GetAppWindow();
  // Only do this in fullscreen as switching tabs in windowed mode will cause a
  // relayout through RenderWidgetCompositor::UpdateLayerTreeHost() (This is due
  // to painting optimization.)
  bool is_fullscreen_without_chrome = app_window && app_window->IsFullscreen();
  if (guestplugin && is_fullscreen_without_chrome) {
    gfx::Rect guest_rect = guestplugin->guest_window_rect();
    gfx::Rect bounds = app_window->GetClientBounds();
    gfx::Size window_size = gfx::Size(bounds.width(), bounds.height());
    if (window_size != gfx::Size(guest_rect.x() + guest_rect.width(),
                                 guest_rect.y() + guest_rect.height())) {
      guestplugin->SizeContents(window_size);
    }
  }
#endif // !OS_MACOSX
}
