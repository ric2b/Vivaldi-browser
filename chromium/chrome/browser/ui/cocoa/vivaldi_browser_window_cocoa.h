// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_VIVALDI_BROWSER_WINDOW_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_keybinding_registry.h"
#include "chrome/browser/signin/signin_header_helper.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/search/search_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "extensions/browser/app_window/app_window.h"
#include "ui/base/ui_base_types.h"


class Browser;
@class BrowserWindowController;
@class FindBarCocoaController;
@class NSEvent;
@class NSMenu;
@class NSWindow;

namespace extensions {
  class ActiveTabPermissionGranter;
  class Command;
  class Extension;
}

// An implementation of BrowserWindow for Cocoa. Bridges between C++ and
// the Cocoa NSWindow. Cross-platform code will interact with this object when
// it needs to manipulate the window.

// This Vivaldi version takes from:
//  BrowserWindowCocoa
//  VivaldiBrowserWindow

class VivaldiBrowserWindowCocoa : public BrowserWindow
{
public:
  VivaldiBrowserWindowCocoa(Browser* browser,
                     BrowserWindowController* controller);
  ~VivaldiBrowserWindowCocoa() override;
  
  // Overridden from BrowserWindow
  void Show() override {}
  void ShowInactive() override {}
  void Hide() override {}
  void SetBounds(const gfx::Rect& bounds) override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void FlashFrame(bool flash) override {}
  bool IsAlwaysOnTop() const override;
  void SetAlwaysOnTop(bool always_on_top) override {}
  gfx::NativeWindow GetNativeWindow() const override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override {}
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override {}
  void UpdateDevTools() override {}
  void UpdateLoadingAnimations(bool should_animate) override {}
  void SetStarredState(bool is_starred) override {}
  void SetTranslateIconToggled(bool is_lit) override {}
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override {}
  void ZoomChangedForActiveTab(bool can_show_bubble) override {}
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override {}
  void Minimize() override {}
  void Restore() override {}
  void EnterFullscreen(const GURL& url, ExclusiveAccessBubbleType type,
                               bool with_toolbar) override {}
  void ExitFullscreen() override {}
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) override {};
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool IsFullscreenBubbleVisible() const override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override {}
  void UpdateReloadStopState(bool is_loading, bool force) override {}
  void UpdateToolbar(content::WebContents* contents) override {}
  void ResetToolbarTabState(content::WebContents* contents) override {}
  void FocusToolbar() override {}
  void ToolbarSizeChanged(bool is_animating) override{}
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInfobars() override {}
  void RotatePaneFocus(bool forwards) override {}
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  gfx::Rect GetRootWindowResizerRect() const override;
  void ConfirmAddSearchProvider(TemplateURL* template_url,
                                Profile* profile) override {}
  void ShowUpdateChromeDialog() override {}
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override {}
  void ShowBookmarkAppBubble(const WebApplicationInfo& web_app_info,
                             const ShowBookmarkAppBubbleCallback& callback) override {}
  void ShowTranslateBubble(content::WebContents* contents,
                           translate::TranslateStep step,
                           translate::TranslateErrors::Type error_type,
                           bool is_user_gesture) override {}
  bool ShowSessionCrashedBubble() override;
  bool IsProfileResetBubbleSupported() const override;
  GlobalErrorBubbleViewBase* ShowProfileResetBubble(
      const base::WeakPtr<ProfileResetGlobalError>& global_error) override;

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  void ShowOneClickSigninBubble(
                                OneClickSigninBubbleType type,
                                const base::string16& email,
                                const base::string16& error_message,
                                const StartSyncCallback& start_sync_callback) override{}
#endif
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  void ConfirmBrowserCloseWithPendingDownloads(
                                               int download_count,
                                               Browser::DownloadClosePreventionType dialog_type,
                                               bool app_modal,
                                               const base::Callback<void(bool)>& callback) override;
  void UserChangedTheme() override {}
  void ShowWebsiteSettings(Profile* profile,
                           content::WebContents* web_contents,
                           const GURL& url,
                           const content::SSLStatus& ssl) override;
  void VivaldiShowWebsiteSettingsAt(Profile* profile,
                           content::WebContents* web_contents,
                           const GURL& url,
                           const content::SSLStatus& ssl,
                             gfx::Point anchor) override;
  void ShowAppMenu() override {}
  bool PreHandleKeyboardEvent(const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) override;
  void HandleKeyboardEvent(
                           const content::NativeWebKeyboardEvent& event) override;
  void CutCopyPaste(int command_id) override{}
  bool SupportsFullscreenWithToolbar() const override;
  void UpdateFullscreenWithToolbar(bool with_toolbar) override {};
  bool IsFullscreenWithToolbar() const  override;
  WindowOpenDisposition GetDispositionForPopupBounds(
                                                     const gfx::Rect& bounds) override;
  FindBar* CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
  override;
  void ShowAvatarBubbleFromAvatarButton(
                                        AvatarBubbleMode mode,
                                        const signin::ManageAccountsParams& manage_accounts_params) override {}
  int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command) override;

  ExclusiveAccessContext* GetExclusiveAccessContext() override;

protected:
  void DestroyBrowser() override;
  
private:
  NSWindow* window() const;  // Accessor for the (current) |NSWindow|.
  
  scoped_ptr<Browser> browser_;
  // Is the window active.
  bool is_active_;
  
  // The window bounds.
  gfx::Rect bounds_;
  
  extensions::AppWindow* GetAppWindow() const;
};

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_COCOA_H_
