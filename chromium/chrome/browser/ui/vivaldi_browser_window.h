// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef CHROME_BROWSER_UI_VIVALID_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_VIVALID_BROWSER_WINDOW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "extensions/browser/app_window/app_window.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/jumplist_win.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

class Browser;

#if defined(OS_WIN)
class JumpList;
#endif

namespace extensions {
  class Extension;
}

// An implementation of BrowserWindow used for Vivaldi.
// This is a dummy window, i.e. this window is never displayed directly, instead
// we use chrome.app.window to display the actual window.  Therefore we
// implement as little as possible for of the BrowerWindow interface, in
// fact we only implment what is needed to pass data from the app window to the
// cpp code.
class VivaldiBrowserWindow : public BrowserWindow {
 public:
  VivaldiBrowserWindow();
  ~VivaldiBrowserWindow() override;

#if defined(OS_WIN)
  JumpList* GetJumpList() const {
    return jumplist_.get();
  }
#endif

  // Takes ownership of |browser|.
  void Init(Browser* browser);

  // Returns the BrowserView used for the specified Browser.
  static VivaldiBrowserWindow* GetBrowserWindowForBrowser(
      const Browser* browser);

  // Create a new VivaldiBrowserWindow;
  static VivaldiBrowserWindow* CreateVivaldiBrowserWindow(Browser* browser);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }

  // LoadCompleteListener::Delegate implementation. Creates and initializes the
  // |jumplist_| after the first page load.
  //virtual void OnLoadCompleted() override;

  // BrowserWindow:
  void Show() override{}
  void ShowInactive() override{}
  void Hide() override{}
  void SetBounds(const gfx::Rect& bounds) override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void FlashFrame(bool flash) override{}
  bool IsAlwaysOnTop() const override;
  void SetAlwaysOnTop(bool always_on_top) override{}
  gfx::NativeWindow GetNativeWindow() const override;
  StatusBubble* GetStatusBubble() override;
  void UpdateTitleBar() override{}
  void BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) override{}
  void UpdateDevTools() override{}
  void UpdateLoadingAnimations(bool should_animate) override{}
  void SetStarredState(bool is_starred) override{}
  void SetTranslateIconToggled(bool is_lit) override{}
  void OnActiveTabChanged(content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) override{}
  void ZoomChangedForActiveTab(bool can_show_bubble) override{}
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override{}
  void Minimize() override{}
  void Restore() override{}
  void EnterFullscreen(
    const GURL& url, ExclusiveAccessBubbleType bubble_type,
                               bool with_toolbar) override{}
  void ExitFullscreen() override{}
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type) override {};
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  bool SupportsFullscreenWithToolbar() const override;
  void UpdateFullscreenWithToolbar(bool with_toolbar) override {};
  bool IsFullscreenWithToolbar() const override;
  void ResetToolbarTabState(content::WebContents* contents) override {};
  bool ShowSessionCrashedBubble() override;
  bool IsProfileResetBubbleSupported() const override;
  GlobalErrorBubbleViewBase* ShowProfileResetBubble(
      const base::WeakPtr<ProfileResetGlobalError>& global_error) override;
#if defined(OS_WIN)
  void SetMetroSnapMode(bool enable) override{}
  bool IsInMetroSnapMode() const;
#endif
  bool IsFullscreenBubbleVisible() const override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override{}
  void UpdateReloadStopState(bool is_loading, bool force) override{}
  void UpdateToolbar(content::WebContents* contents) override{}
  void FocusToolbar() override{}
  void ToolbarSizeChanged(bool is_animating) override{};
  void FocusAppMenu() override{}
  void FocusBookmarksToolbar() override{}
  void FocusInfobars() override{}
  void RotatePaneFocus(bool forwards) override{}
  void ShowAppMenu() override{}
  bool PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) override;
  void HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) override{}

  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  gfx::Rect GetRootWindowResizerRect() const override;
  void ConfirmAddSearchProvider(TemplateURL* template_url,
    Profile* profile) override{}
  void ShowUpdateChromeDialog() override{}
  void ShowBookmarkBubble(const GURL& url,
    bool already_bookmarked) override{}
  void ShowBookmarkAppBubble(
      const WebApplicationInfo& web_app_info,
      const ShowBookmarkAppBubbleCallback& callback) override {};
  void ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) override{}
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
    const base::Callback<void(bool)>& callback) override{}
  void UserChangedTheme() override{}
  void ShowWebsiteSettings(Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const content::SSLStatus& ssl) override;
  void VivaldiShowWebsiteSettingsAt(Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const content::SSLStatus& ssl,
    gfx::Point pos) override;
  void CutCopyPaste(int command_id) override{}

  WindowOpenDisposition GetDispositionForPopupBounds(
    const gfx::Rect& bounds) override;
  FindBar* CreateFindBar() override;
  web_modal::WebContentsModalDialogHost*
    GetWebContentsModalDialogHost() override;
  void ShowAvatarBubbleFromAvatarButton(AvatarBubbleMode mode,
    const signin::ManageAccountsParams& manage_accounts_params) override{}
  int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  void ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) override;

  //extensions::AppWindow* GetAppWindow(content::WebContents* web_contents);
  ExclusiveAccessContext* GetExclusiveAccessContext() override;

 protected:
  void DestroyBrowser() override;

 private:
  // The Browser object we are associated with.
  scoped_ptr<Browser> browser_;

  // Is the window active.
  bool is_active_;

  // The window bounds.
  gfx::Rect bounds_;

#if defined(OS_WIN)
  // The custom JumpList for Windows 7.
  scoped_refptr<JumpList> jumplist_;
#endif

  extensions::AppWindow* GetAppWindow() const;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserWindow);
};

namespace chrome {

// Helper that handle the lifetime of VivaldiBrowserWindow instances.
Browser* CreateBrowserWithVivaldiWindowForParams(Browser::CreateParams* params);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIVALID_BROWSER_WINDOW_H_
