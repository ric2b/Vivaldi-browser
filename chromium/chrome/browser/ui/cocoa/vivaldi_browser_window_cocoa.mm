// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/vivaldi_browser_window_cocoa.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/fullscreen.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#import "chrome/browser/ui/cocoa/browser/edit_search_engine_cocoa_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/download/download_shelf_controller.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_base_controller.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_menu_bubble_controller.h"
#include "chrome/browser/ui/cocoa/restart_browser.h"
#include "chrome/browser/ui/cocoa/status_bubble_mac.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/web_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/website_settings/website_settings_bubble_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/translate/core/browser/language_state.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/geometry/rect.h"


#include "base/json/json_reader.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"


#if defined(ENABLE_ONE_CLICK_SIGNIN)
#import "chrome/browser/ui/cocoa/one_click_signin_bubble_controller.h"
#import "chrome/browser/ui/cocoa/one_click_signin_dialog_controller.h"
#endif

using content::NativeWebKeyboardEvent;
using content::SSLStatus;
using content::WebContents;

static const char vivaldi_app_id[] = "mpognobbkildjkofajifpdfhcoklimli";

namespace {
  /* gisli@vivaldi.com:  This is not used for now.  Leave it in 
     here as we might want to use this to map from win to mac coord.
  NSPoint GetPointForBubble(content::WebContents* web_contents,
                            int x_offset,
                            int y_offset) {
    NSView* view = web_contents->GetNativeView();
    NSRect bounds = [view bounds];
    NSPoint point;
    point.x = NSMinX(bounds) + x_offset;
    // The view's origin is at the bottom but |rect|'s origin is at the top.
    point.y = NSMaxY(bounds) - y_offset;
    point = [view convertPoint:point toView:nil];
    point = [[view window] convertBaseToScreen:point];
    return point;
  }
  */
}  // namespace

VivaldiBrowserWindowCocoa::VivaldiBrowserWindowCocoa(Browser* browser,
                                       BrowserWindowController* controller)
:
//BrowserWindowCocoa(browser, controller)
//,
//browser_(browser),
//,
//controller_(controller),
//initial_show_state_(ui::SHOW_STATE_DEFAULT),
//attention_request_id_(0)
bounds_(browser->override_bounds())
{
  browser_.reset(browser);
  //
  //gfx::Rect bounds;
  //chrome::GetSavedWindowBoundsAndShowState(browser_,
  //                                         &bounds,
  //                                         &initial_show_state_);
  
  //browser_->search_model()->AddObserver(this);
}

VivaldiBrowserWindowCocoa::~VivaldiBrowserWindowCocoa() {
  //browser_->search_model()->RemoveObserver(this);
  browser_.reset();
}


void VivaldiBrowserWindowCocoa::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
}

// Callers assume that this doesn't immediately delete the Browser object.
// The controller implementing the window delegate methods called from
// |-performClose:| must take precautions to ensure that.
void VivaldiBrowserWindowCocoa::Close() {
  //gisli@vivaldi.com: Code based on BrowserView::CanClose();
  // Cant compare directly with BrowserWindowCocoa.Close() as some of the
  // functionality on Mac is in BrowserWindowController.mm
  
  // Give beforeunload handlers the chance to cancel the close before we hide
  // the window below.
  if (!browser_->ShouldCloseWindow())
    return;
  
  bool fast_tab_closing_enabled =
  base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableFastUnload);
  
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
  
  // Empty TabStripModel, it's now safe to allow the Window to be closed.
  content::NotificationService::current()->Notify(
    chrome::NOTIFICATION_WINDOW_CLOSED,
    content::Source<gfx::NativeWindow>(NULL),
    content::NotificationService::NoDetails());
  
  delete this;
  
  /* Vivaldi:
  // If there is an overlay window, we contain a tab being dragged between
  // windows. Don't hide the window as it makes the UI extra confused. We can
  // still close the window, as that will happen when the drag completes.
  if ([controller_ overlayWindow]) {
    [controller_ deferPerformClose];
  } else {
    // Using |-performClose:| can prevent the window from actually closing if
    // a JavaScript beforeunload handler opens an alert during shutdown, as
    // documented at <http://crbug.com/118424>. Re-implement
    // -[NSWindow performClose:] as closely as possible to how Apple documents
    // it.
    //
    // Before calling |-close|, hide the window immediately. |-performClose:|
    // would do something similar, and this ensures that the window is removed
    // from AppKit's display list. Not doing so can lead to crashes like
    // <http://crbug.com/156101>.
    id<NSWindowDelegate> delegate = [window() delegate];
    SEL window_should_close = @selector(windowShouldClose:);
    if ([delegate respondsToSelector:window_should_close]) {
      if ([delegate windowShouldClose:window()]) {
        [window() orderOut:nil];
        [window() close];
      }
    } else if ([window() respondsToSelector:window_should_close]) {
      if ([window() performSelector:window_should_close withObject:window()]) {
        [window() orderOut:nil];
        [window() close];
      }
    } else {
      [window() orderOut:nil];
      [window() close];
    }
  }
   */
}

void VivaldiBrowserWindowCocoa::Activate() {
  extensions::AppWindow* app_window = GetAppWindow();
  if (app_window) {
    app_window->GetBaseWindow()->Activate();
  }
  
  is_active_ = true;
  
  BrowserList::SetLastActive(browser_.get());
}

void VivaldiBrowserWindowCocoa::Deactivate() {
  is_active_ = false;
}

bool VivaldiBrowserWindowCocoa::IsAlwaysOnTop() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::IsActive() const {
  return is_active_;
}

gfx::NativeWindow VivaldiBrowserWindowCocoa::GetNativeWindow() const {
  return window();
}

StatusBubble* VivaldiBrowserWindowCocoa::GetStatusBubble() {
  return NULL;
}

gfx::Rect VivaldiBrowserWindowCocoa::GetRestoredBounds() const {
  return gfx::Rect();
}

ui::WindowShowState VivaldiBrowserWindowCocoa::GetRestoredState() const {
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;
  if (IsMinimized())
    return ui::SHOW_STATE_MINIMIZED;
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect VivaldiBrowserWindowCocoa::GetBounds() const {
  return bounds_;
}

bool VivaldiBrowserWindowCocoa::IsMaximized() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::IsMinimized() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::ShouldHideUIForFullscreen() const {
  // On Mac, fullscreen mode has most normal things (in a slide-down panel).
  return false;
}

bool VivaldiBrowserWindowCocoa::IsFullscreen() const {
  return false; // Vivaldi: [controller_ isInAnyFullscreenMode];
}

bool VivaldiBrowserWindowCocoa::IsFullscreenBubbleVisible() const {
  return false;
}

LocationBar* VivaldiBrowserWindowCocoa::GetLocationBar() const {
  return NULL;
}

bool VivaldiBrowserWindowCocoa::IsBookmarkBarVisible() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::IsBookmarkBarAnimating() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::IsTabStripEditable() const {
  return true;
}

bool VivaldiBrowserWindowCocoa::IsToolbarVisible() const {
  return false;
}

gfx::Rect VivaldiBrowserWindowCocoa::GetRootWindowResizerRect() const {
  return gfx::Rect();
   
}

bool VivaldiBrowserWindowCocoa::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* VivaldiBrowserWindowCocoa::GetDownloadShelf() {
  return NULL;
}

// We allow closing the window here since the real quit decision on Mac is made
// in [AppController quit:].
void VivaldiBrowserWindowCocoa::ConfirmBrowserCloseWithPendingDownloads(
                                                                 int download_count,
                                                                 Browser::DownloadClosePreventionType dialog_type,
                                                                 bool app_modal,
                                                                 const base::Callback<void(bool)>& callback) {
  // Vivaldi:
  callback.Run(true);
}



void VivaldiBrowserWindowCocoa::ShowWebsiteSettings(Profile* profile,
                                               content::WebContents* web_contents,
                                               const GURL& url,
                                               const content::SSLStatus& ssl) {
  
  // For Vivaldi we reroute this back to javascript site, for either
  // display a javascript siteinfo or call back to us (via webview) using
  // ShowWebsiteSettingsAt.
  content::WebContentsImpl* web_contents_impl =
  static_cast<content::WebContentsImpl*>(web_contents);
  
  static_cast<extensions::WebViewGuest*>(
    web_contents_impl->GetDelegate())->RequestPageInfo(url);
}

void VivaldiBrowserWindowCocoa::VivaldiShowWebsiteSettingsAt(Profile* profile,
                                                 content::WebContents* web_contents,
                                                 const GURL& url,
                                                 const content::SSLStatus& ssl,
                                                 gfx::Point anchor) {
  WebsiteSettingsUIBridge::ShowAt(window(), profile, web_contents, url, ssl, anchor);
}

bool VivaldiBrowserWindowCocoa::PreHandleKeyboardEvent(
                                                const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  return false;
}

void VivaldiBrowserWindowCocoa::HandleKeyboardEvent(
                                             const NativeWebKeyboardEvent& event) {
  if ([BrowserWindowUtils shouldHandleKeyboardEvent:event])
    [BrowserWindowUtils handleKeyboardEvent:event.os_event inWindow:window()];
}

bool VivaldiBrowserWindowCocoa::SupportsFullscreenWithToolbar() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::IsFullscreenWithToolbar() const {
  return false;
}

bool VivaldiBrowserWindowCocoa::ShowSessionCrashedBubble() {
  return false;
}

bool VivaldiBrowserWindowCocoa::IsProfileResetBubbleSupported() const {
  return false;
}

GlobalErrorBubbleViewBase* VivaldiBrowserWindowCocoa::ShowProfileResetBubble(
      const base::WeakPtr<ProfileResetGlobalError>& global_error) {
  NOTREACHED();
  return nullptr;
}


WindowOpenDisposition VivaldiBrowserWindowCocoa::GetDispositionForPopupBounds(
                                                                       const gfx::Rect& bounds) {
  /* Vivaldi:
  // When using Cocoa's System Fullscreen mode, convert popups into tabs.
  if ([controller_ isInAppKitFullscreen])
    return NEW_FOREGROUND_TAB;
  return NEW_POPUP;
   */
  return NEW_POPUP;
}

FindBar* VivaldiBrowserWindowCocoa::CreateFindBar() {
  return NULL;
}

web_modal::WebContentsModalDialogHost*
VivaldiBrowserWindowCocoa::GetWebContentsModalDialogHost() {
  return NULL;
}

void VivaldiBrowserWindowCocoa::DestroyBrowser() {
  delete this;
}

NSWindow* VivaldiBrowserWindowCocoa::window() const {
  // Vivaldi: return [controller_ window];
  if (GetAppWindow()) {
    return GetAppWindow()->GetNativeWindow();
  }
  return nullptr;
}

int
VivaldiBrowserWindowCocoa::GetRenderViewHeightInsetWithDetachedBookmarkBar() {
  /*
  if (browser_->bookmark_bar_state() != BookmarkBar::DETACHED)
    return 0;
   */
  return 40;
}

void VivaldiBrowserWindowCocoa::ExecuteExtensionCommand(
                                                 const extensions::Extension* extension,
                                                 const extensions::Command& command) {
  //Vivaldi: [cocoa_controller() executeExtensionCommand:extension->id() command:command];
}

extensions::AppWindow* VivaldiBrowserWindowCocoa::GetAppWindow() const {
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
        appwinreg->GetAppWindowForWebContents(embedder_web_contents);
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
        app_window = appwinreg->GetAppWindowForAppAndKey(vivaldi_app_id, windowid);
      }
    }
    
  }
  return app_window;
}


ExclusiveAccessContext* VivaldiBrowserWindowCocoa::GetExclusiveAccessContext() {
  return NULL;
}
