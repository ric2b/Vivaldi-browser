// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_BROWSER_WINDOW_H_
#define UI_VIVALDI_BROWSER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "extensions/browser/app_window/app_web_contents_helper.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/accelerators/accelerator.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

class Browser;

#if defined(OS_WIN)
class JumpList;
#endif

class VivaldiBrowserWindow;

namespace ui {
class Accelerator;
}

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
class NativeAppWindow;
struct DraggableRegion;

class VivaldiWindowEventDelegate {
 public:
  virtual void OnMinimized() = 0;
  virtual void OnMaximized() = 0;
  virtual void OnRestored() = 0;
  virtual void OnFullscreenChanged(bool fullscreen) = 0;
  virtual void OnDocumentLoaded() = 0;
};

// AppWindowContents class specific to vivaldi app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class VivaldiAppWindowContentsImpl : public AppWindowContents,
                                     public content::WebContentsDelegate,
                                     public content::WebContentsObserver {
 public:
  VivaldiAppWindowContentsImpl(VivaldiBrowserWindow* host,
                               VivaldiWindowEventDelegate* delegate,
                               extensions::AppDelegate* app_delegate);
  ~VivaldiAppWindowContentsImpl() override;

  // AppWindowContents
  void Initialize(content::BrowserContext* context,
                  content::RenderFrameHost* creator_frame,
                  const GURL& url) override;
  void LoadContents(int32_t creator_process_id) override;
  void NativeWindowChanged(NativeAppWindow* native_app_window) override;
  void NativeWindowClosed() override;
  void OnWindowReady() override;
  content::WebContents* GetWebContents() const override;
  WindowController* GetWindowController() const override;

  struct StateData {
    // State kept to dispatch events on changes.
    bool is_fullscreen_ = false;
    bool is_maximized_ = false;
    bool is_minimized_ = false;
  };

  // Overridden from WebContentsDelegate
  void HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  content::ColorChooser* OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<content::ColorSuggestion>& suggestions) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      const content::FileChooserParams& params) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) override;
  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;

 private:
  // content::WebContentsObserver
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* sender) override;
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  void UpdateDraggableRegions(content::RenderFrameHost* sender,
                              const std::vector<DraggableRegion>& regions);
  void SuspendRenderFrameHost(content::RenderFrameHost* rfh);

  // Force showing of the window after a delay, even if the document has not
  // loaded. This avoids situations where the document fails somehow and no
  // window is shown.
  void ForceShowWindow();

  VivaldiBrowserWindow* host_;  // This class is owned by |host_|
  GURL url_;
  std::unique_ptr<content::WebContents> web_contents_;
  bool is_blocking_requests_;
  bool is_window_ready_;
  VivaldiWindowEventDelegate* delegate_ = nullptr;

  std::unique_ptr<extensions::AppWebContentsHelper> helper_;

  StateData state_;

  // Used to timeout the main document loading and force show the window.
  base::Timer load_timeout_;

  // Owned by VivaldiBrowserWindow
  extensions::AppDelegate* app_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowContentsImpl);
};

}  // namespace extensions

// An implementation of BrowserWindow used for Vivaldi.
class VivaldiBrowserWindow
    : public BrowserWindow,
      public content::WebContentsObserver,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public ExclusiveAccessContext,
      public ui::AcceleratorProvider,
      public extensions::VivaldiWindowEventDelegate,
      public extensions::ExtensionFunctionDispatcher::Delegate,
      public extensions::ExtensionRegistryObserver {
 public:
  VivaldiBrowserWindow();
  ~VivaldiBrowserWindow() override;

  enum WindowType {
    NORMAL,
    SETTINGS,
  };

  // Takes ownership of |browser|.
  void Init(Browser* browser,
            std::string& base_url,
            extensions::AppWindow::CreateParams& params);

  // Returns the BrowserView used for the specified Browser.
  static VivaldiBrowserWindow* GetBrowserWindowForBrowser(
      const Browser* browser);

  // Create a new VivaldiBrowserWindow;
  static VivaldiBrowserWindow* CreateVivaldiBrowserWindow(
      Browser* browser,
      std::string& base_url,
      extensions::AppWindow::CreateParams& params);

  static extensions::AppWindow::CreateParams PrepareWindowParameters(
      Browser* browser,
      std::string& base_url);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }
  content::WebContents* web_contents() const;

  // Takes ownership of |browser|.
  void set_browser(Browser* browser) { browser_.reset(browser); }

  // LoadCompleteListener::Delegate implementation. Creates and initializes the
  // |jumplist_| after the first page load.
  // virtual void OnLoadCompleted() override;

  void Show(extensions::AppWindow::ShowType show_type);

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override;

  // ExtensionFunctionDispatcher::Delegate implementation.
  extensions::WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // BrowserWindow:
  void Show() override;
  void ShowInactive() override {}
  void Hide() override;
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
  void UpdateTitleBar() override;
  void BookmarkBarStateChanged(
      BookmarkBar::AnimateChangeType change_type) override {}
  void UpdateDevTools() override;
  void UpdateLoadingAnimations(bool should_animate) override {}
  void SetStarredState(bool is_starred) override {}
  void SetTranslateIconToggled(bool is_lit) override {}
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override {}
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  bool ShouldHideUIForFullscreen() const override;
  bool IsFullscreen() const override;
  void ResetToolbarTabState(content::WebContents* contents) override{};
  bool IsFullscreenBubbleVisible() const override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override {}
  void UpdateReloadStopState(bool is_loading, bool force) override {}
  void UpdateToolbar(content::WebContents* contents) override {}
  void FocusToolbar() override {}
  ToolbarActionsBar* GetToolbarActionsBar() override;
  void ToolbarSizeChanged(bool is_animating) override{};
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInfobars() override {}
  void RotatePaneFocus(bool forwards) override {}
  void ShowAppMenu() override {}
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  gfx::Size GetContentsSize() const override;

  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  void ShowUpdateChromeDialog() override {}
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override {}
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) override;
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadClosePreventionType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override;
  void UserChangedTheme() override {}
  void VivaldiShowWebsiteSettingsAt(
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      const security_state::SecurityInfo& security_info,
      gfx::Point pos) override;
  void CutCopyPaste(int command_id) override {}

  WindowOpenDisposition GetDispositionForPopupBounds(
      const gfx::Rect& bounds) override;
  FindBar* CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  int GetRenderViewHeightInsetWithDetachedBookmarkBar() override;
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command) override;
  autofill::SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) override;
  void ShowOneClickSigninConfirmation(
      const base::string16& email,
      const StartSyncCallback& start_sync_callback) override{};

  // web_modal::WebContentsModalDialogManagerDelegate implementation.
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;

  // Overridden from ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  ui::AcceleratorProvider* GetAcceleratorProvider();

  // Overridden from ExclusiveAccessContext
  Profile* GetProfile() override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback) override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetActiveWebContents() override;
  void UnhideDownloadShelf() override;
  void HideDownloadShelf() override;

  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      const signin::ManageAccountsParams& manage_accounts_params,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {}

  void ShowImeWarningBubble(
      const extensions::Extension* extension,
      const base::Callback<void(ImeWarningBubblePermissionStatus status)>&
          callback) override {}

  std::string GetWorkspace() const override;
  void MaybeShowNewBackShortcutBubble(bool forward) override {}
  void HideNewBackShortcutBubble() override {}
  bool IsVisibleOnAllWorkspaces() const override;

  void ResetDockingState(int tab_id);

  bool IsToolbarShowing() const override;

  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions);

  void OnNativeWindowChanged();
  void OnNativeClose();
  void OnNativeWindowActivated();

  // VivaldiWindowEventDelegate implementation
  void OnMinimized() override;
  void OnMaximized() override;
  void OnRestored() override;
  void OnFullscreenChanged(bool fullscreen) override;
  void OnDocumentLoaded() override;

  // Enable or disable fullscreen mode.
  void SetFullscreen(bool enable);

  base::string16 GetTitle();
  extensions::NativeAppWindow* GetBaseWindow() const;

  content::WebContents* GetActiveWebContents() const;

  // TODO(pettern): fix
  bool requested_alpha_enabled() { return false; }

  void OnReadyToCommitFirstNavigation();

  // Call to create web contents delayed.
  void CreateWebContents(content::RenderFrameHost* host);

  const extensions::Extension* extension() { return extension_; }

  bool is_hidden() { return is_hidden_; }

  void set_type(WindowType window_type) {
    window_type_ = window_type;
  }
  WindowType type() {
    return window_type_;
  }

 protected:
  void DestroyBrowser() override;

  void DeleteThis();

  // The Browser object we are associated with.
  std::unique_ptr<Browser> browser_;

 private:
  // From AppWindow:
  std::unique_ptr<extensions::NativeAppWindow> native_app_window_;
  std::unique_ptr<extensions::AppWindowContents> app_window_contents_;
  std::unique_ptr<extensions::AppDelegate> app_delegate_;

  // The initial url this AppWindow was navigated to.
  GURL initial_url_;
  std::string base_url_;

  // params passed from the api.
  extensions::AppWindow::CreateParams params_;

  // Whether the window has been shown or not.
  bool has_been_shown_ = false;

  // Whether the window is hidden or not. Hidden in this context means actively
  // by the chrome.app.window API, not in an operating system context. For
  // example windows which are minimized are not hidden, and windows which are
  // part of a hidden app on OS X are not hidden. Windows which were created
  // with the |hidden| flag set to true, or which have been programmatically
  // hidden, are considered hidden.
  bool is_hidden_ = false;

  // Whether the main document (UI) has loaded or not. The window will not be
  // be shown until that happens.
  bool has_loaded_document_ = false;

  // Return the old title if the new title is not usable.
  base::string16 old_title_;

  ui::WindowShowState initial_state_ = ui::SHOW_STATE_DEFAULT;

  // The window type for this window.
  WindowType window_type_ = WindowType::NORMAL;

  // Reference to the Vivaldi extension.
  extensions::Extension* extension_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserWindow);
};

#endif  // UI_VIVALDI_BROWSER_WINDOW_H_
