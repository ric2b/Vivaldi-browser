// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_browser_window.h"

#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/jumplist_win.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"

// VivaldiBrowserWindow --------------------------------------------------------

VivaldiBrowserWindow::VivaldiBrowserWindow() :
  is_active_(false),
  bounds_(gfx::Rect()){
#if defined(OS_WIN)
  jumplist_ = NULL;
#endif
}

VivaldiBrowserWindow::~VivaldiBrowserWindow() {
#if defined(OS_WIN)
  // Terminate the jumplist (must be called before browser_->profile() is
  // destroyed.
  if (jumplist_) {
    jumplist_->Terminate();
  }
#endif

  // Explicitly set browser_ to NULL.
  browser_.reset();
}

void VivaldiBrowserWindow::Init(Browser* browser) {
  browser_.reset(browser);
  SetBounds(browser->override_bounds());
#if defined(OS_WIN)
  DCHECK(!jumplist_);
  jumplist_ = new JumpList(browser_->profile());
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

void VivaldiBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  return;
}

void VivaldiBrowserWindow::Close() {
  //gisli@vivaldi.com: Code based on BrowserView::CanClose();

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
#if defined(OS_LINUX)
  // Never activate an active window. It triggers problems in focus follows
  // mouse window manager mode. See VB-11947
  if (is_active_)
    return;
#endif

  extensions::AppWindow* app_window = GetAppWindow();
  if (app_window) {
    app_window->GetBaseWindow()->Activate();
  }

  is_active_ = true;

  BrowserList::SetLastActive(browser_.get());
}

void VivaldiBrowserWindow::Deactivate() {
  is_active_ = false;
}

bool VivaldiBrowserWindow::IsActive() const {
  return is_active_;
}

bool VivaldiBrowserWindow::IsAlwaysOnTop() const {
  return false;
}

extensions::AppWindow* VivaldiBrowserWindow::GetAppWindow() const {
  extensions::AppWindow* app_window = NULL;

  extensions::AppWindowRegistry* appwinreg =
    extensions::AppWindowRegistry::Get(browser_->profile());
  content::WebContentsImpl* web_contents =
    static_cast<content::WebContentsImpl*>(browser_->tab_strip_model()
    ->GetActiveWebContents());
  if (!!web_contents) {
    content::BrowserPluginGuest* guestplugin =
      web_contents->GetBrowserPluginGuest();
    if (!!guestplugin) {
      content::WebContents* embedder_web_contents
        = guestplugin->embedder_web_contents();
      if (embedder_web_contents) {
        app_window =
          appwinreg->GetAppWindowForWebContents(
          embedder_web_contents);
      }
    }
  }
  if(!app_window){

    scoped_ptr<base::Value> value(base::JSONReader::Read(browser_->ext_data()));
    base::DictionaryValue* dictionary;
    if (value && value->GetAsDictionary(&dictionary)) {
      std::string windowid;
      if (dictionary->GetString("ext_id", &windowid)) {
        windowid.insert(0, "vivaldi_window_");  // This is added in the client.
        app_window =
            appwinreg->GetAppWindowForAppAndKey(
                vivaldi::kVivaldiAppId, windowid);
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

bool VivaldiBrowserWindow::SupportsFullscreenWithToolbar() const {
  return false;
}

bool VivaldiBrowserWindow::IsFullscreenWithToolbar() const {
  return false;
}

#if defined(OS_WIN)
bool VivaldiBrowserWindow::IsInMetroSnapMode() const {
  return false;
}
#endif

bool VivaldiBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

LocationBar* VivaldiBrowserWindow::GetLocationBar() const {
  return NULL;;
}

ToolbarActionsBar* VivaldiBrowserWindow::GetToolbarActionsBar() {
  return NULL;
}

bool VivaldiBrowserWindow::PreHandleKeyboardEvent(
  const content::NativeWebKeyboardEvent& event,
  bool* is_keyboard_shortcut) {
  return false;
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

gfx::Rect VivaldiBrowserWindow::GetRootWindowResizerRect() const {
  return gfx::Rect();
}

bool VivaldiBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* VivaldiBrowserWindow::GetDownloadShelf() {
  if (!download_shelf_.get()) {
    download_shelf_.reset(new DownloadShelfView(browser_.get(), NULL));
    download_shelf_->set_owned_by_client();
  }
  return download_shelf_.get();
}

void VivaldiBrowserWindow::ShowWebsiteSettings(Profile* profile,
        content::WebContents* web_contents,
        const GURL& url,
        const security_state::SecurityStateModel::SecurityInfo& security_info) {

  // For Vivaldi we reroute this back to javascript site, for either
  // display a javascript siteinfo or call back to us (via webview) using
  // VivaldiShowWebsiteSettingsAt.
  content::WebContentsImpl* web_contents_impl =
    static_cast<content::WebContentsImpl*>(web_contents);

  static_cast<extensions::WebViewGuest*>(
    web_contents_impl->GetDelegate())->RequestPageInfo(url);
}

// See comments on: BrowserWindow.VivaldiShowWebSiteSettingsAt.
void VivaldiBrowserWindow::VivaldiShowWebsiteSettingsAt(Profile* profile,
        content::WebContents* web_contents,
        const GURL& url,
        const security_state::SecurityStateModel::SecurityInfo& security_info,
        gfx::Point pos) {
#if defined(USE_AURA)
  // This is only for AURA.  Mac is done in VivaldiBrowserCocoa.
  WebsiteSettingsPopupView::ShowPopupAtPos(
    pos,
    profile,
    web_contents,
    url,
    security_info,
    browser_.get(),
    GetAppWindow()->GetNativeWindow());
#endif

}

WindowOpenDisposition VivaldiBrowserWindow::GetDispositionForPopupBounds(
  const gfx::Rect& bounds) {
  return NEW_POPUP;
}

FindBar* VivaldiBrowserWindow::CreateFindBar() {
  return NULL;
}

web_modal::WebContentsModalDialogHost*
VivaldiBrowserWindow::GetWebContentsModalDialogHost() {
  return NULL;
}

int
VivaldiBrowserWindow::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  return 0;
}

void VivaldiBrowserWindow::ExecuteExtensionCommand(
  const extensions::Extension* extension,
  const extensions::Command& command) {}

ExclusiveAccessContext* VivaldiBrowserWindow::GetExclusiveAccessContext() {
  return NULL;
}

autofill::SaveCardBubbleView* VivaldiBrowserWindow::ShowSaveCreditCardBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture){
  return NULL;
}

void VivaldiBrowserWindow::DestroyBrowser() {
  delete this;
}

bool VivaldiBrowserWindow::ShouldHideFullscreenToolbar() const {
  return false;
}
