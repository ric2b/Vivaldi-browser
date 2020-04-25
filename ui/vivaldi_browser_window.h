// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_BROWSER_WINDOW_H_
#define UI_VIVALDI_BROWSER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
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
#include "ui/infobar_container_web_proxy.h"
#include "ui/vivaldi_location_bar.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

class Browser;

#if defined(OS_WIN)
class JumpList;
#endif

class VivaldiBrowserWindow;
class VivaldiNativeAppWindow;

namespace autofill {
class AutofillBubbleHandler;
}

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
}

namespace autofill {
class SaveCardBubbleView;
class LocalCardMigrationBubble;
class SaveCardBubbleController;
class LocalCardMigrationBubbleController;
}

class VivaldiAutofillBubbleHandler : public autofill::AutofillBubbleHandler {
 public:
  VivaldiAutofillBubbleHandler();
  ~VivaldiAutofillBubbleHandler() override;

  autofill::SaveCardBubbleView* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) override;

  // Shows the sign in promo bubble from the avatar button.
  autofill::SaveCardBubbleView* ShowSaveCardSignInPromoBubble(
      content::WebContents* contents,
      autofill::SaveCardBubbleController* controller) override;

  autofill::LocalCardMigrationBubble* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      autofill::LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;

  void OnPasswordSaved() override {}

  void HideSignInPromo() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiAutofillBubbleHandler);
};

class VivaldiNativeAppWindowViews;

// AppWindowContents class specific to vivaldi app windows. It maintains a
// WebContents instance and observes it for the purpose of passing
// messages to the extensions system.
class VivaldiAppWindowContentsImpl : public content::WebContentsDelegate,
                                     public content::WebContentsObserver {
 public:
  VivaldiAppWindowContentsImpl(VivaldiBrowserWindow* host);
  ~VivaldiAppWindowContentsImpl() override;

  content::WebContents* web_contents() const { return web_contents_.get(); }

  // AppWindowContents
  void Initialize(std::unique_ptr<content::WebContents> web_contents);
  void NativeWindowClosed();

  // Overridden from WebContentsDelegate
  bool HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  content::ColorChooser* OpenColorChooser(
    content::WebContents* web_contents,
    SkColor color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      std::unique_ptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents,
      const viz::SurfaceId& surface_id,
      const gfx::Size& natural_size) override;
  void ExitPictureInPicture() override;
  void PrintCrossProcessSubframe(
      content::WebContents* web_contents,
      const gfx::Rect& rect,
      int document_cookie,
      content::RenderFrameHost* subframe_host) const override;
  void ActivateContents(content::WebContents* contents) override;

 private:
  // content::WebContentsObserver
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* sender) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  void UpdateDraggableRegions(
      content::RenderFrameHost* sender,
      const std::vector<extensions::DraggableRegion>& regions);

  extensions::AppDelegate* GetAppDelegate();

  VivaldiBrowserWindow* host_;  // This class is owned by |host_|
  std::unique_ptr<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppWindowContentsImpl);
};

// An implementation of BrowserWindow used for Vivaldi.
class VivaldiBrowserWindow
    : public BrowserWindow,
      public web_modal::WebContentsModalDialogManagerDelegate,
      public ExclusiveAccessContext,
      public ui::AcceleratorProvider,
      public infobars::InfoBarContainer::Delegate,
      public extensions::ExtensionFunctionDispatcher::Delegate,
      public extensions::ExtensionRegistryObserver {
 public:
  VivaldiBrowserWindow();
  ~VivaldiBrowserWindow() override;

  enum WindowType {
    NORMAL,
    SETTINGS,
  };

  // Returns the BrowserView used for the specified Browser.
  static VivaldiBrowserWindow* GetBrowserWindowForBrowser(
      const Browser* browser);

  // Create a new VivaldiBrowserWindow;
  static VivaldiBrowserWindow* CreateVivaldiBrowserWindow(
      std::unique_ptr<Browser> browser);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }
  content::WebContents* web_contents() const {
    return app_window_contents_.web_contents();
  }

  // Takes ownership of |browser|.
  void SetBrowser(std::unique_ptr<Browser> browser);

  void CreateWebContents(const extensions::AppWindow::CreateParams& params,
                         content::RenderFrameHost* creator_frame);

  void LoadContents(const std::string& resource_relative_url);

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

  // infobars::InfoBarContainer::Delegate
  void InfoBarContainerStateChanged(bool is_animating) override;

  //
  // BrowserWindow overrides
  //
  void Show() override;
  void ShowInactive() override {}
  void Hide() override;
  bool IsVisible() const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void FlashFrame(bool flash) override {}
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
  void ResetToolbarTabState(content::WebContents* contents) override {}
  bool IsFullscreenBubbleVisible() const override;
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override {}
  void UpdateReloadStopState(bool is_loading, bool force) override {}
  void UpdateToolbar(content::WebContents* contents) override;
  void FocusToolbar() override {}
  ToolbarActionsBar* GetToolbarActionsBar() override;
  void ToolbarSizeChanged(bool is_animating) override {}
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInactivePopupForAccessibility() override {}
  void RotatePaneFocus(bool forwards) override {}
  void ShowAppMenu() override {}
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  gfx::Size GetContentsSize() const override;
  void SetContentsSize(const gfx::Size& size) override {}
  bool UpdatePageActionIcon(PageActionIconType type) override;
  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() override;
  void ExecutePageActionIconForTesting(PageActionIconType type) override {}
  void ShowEmojiPanel() override;
  bool IsBookmarkBarVisible() const override;
  bool IsBookmarkBarAnimating() const override;
  bool IsTabStripEditable() const override;
  bool IsToolbarVisible() const override;
  void ShowUpdateChromeDialog() override {}
  void ShowBookmarkBubble(const GURL& url, bool already_bookmarked) override {}
  qrcode_generator::QRCodeGeneratorBubbleView* ShowQRCodeGeneratorBubble(
      content::WebContents* contents,
      qrcode_generator::QRCodeGeneratorBubbleController* controller,
      const GURL& url) override;
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors::Type error_type,
      bool is_user_gesture) override;
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      bool app_modal,
      const base::Callback<void(bool)>& callback) override;
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override {}
  void VivaldiShowWebsiteSettingsAt(
      Profile* profile,
      content::WebContents* web_contents,
      const GURL& url,
      security_state::SecurityLevel security_level,
      const security_state::VisibleSecurityState& visible_security_state,
      gfx::Point pos) override;
  void CutCopyPaste(int command_id) override {}
  bool IsOnCurrentWorkspace() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override {}
  bool DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override {}
  bool CanUserExitFullscreen() const override;
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ExecuteExtensionCommand(const extensions::Extension* extension,
                               const extensions::Command& command) override;
  void ShowOneClickSigninConfirmation(
      const base::string16& email,
      base::OnceCallback<void(bool)> start_sync_callback) override {}
  void OnTabRestored(int command_id) override {}
  void OnTabDetached(content::WebContents* contents, bool was_active) override {
  }
  void TabDraggingStatusChanged(bool is_dragging) override {}
  // Shows in-product help for the given feature.
  void ShowInProductHelpPromo(InProductHelpFeature iph_feature) override {}
  void UpdateFrameColor() override {}
#if !defined(OS_ANDROID)
  void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      PageActionIconType icon_type,
      const base::Optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override {}
#endif
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfBubble(
    content::WebContents* contents,
    send_tab_to_self::SendTabToSelfBubbleController* controller,
    bool is_user_gesture) override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override {}
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowHatsBubble(const std::string& site_id) override {}
  // BrowserWindow overrides end

  // BaseWindow overrides
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override {}

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
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetActiveWebContents() override;
  void UnhideDownloadShelf() override;
  void HideDownloadShelf() override;

  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {}

  void ShowImeWarningBubble(
      const extensions::Extension* extension,
      const base::Callback<void(ImeWarningBubblePermissionStatus status)>&
          callback) override {}

  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;

  void ResetDockingState(int tab_id);

  bool IsToolbarShowing() const override;

  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions);

  // If moved is true, the change is caused by a move
  void OnNativeWindowChanged(bool moved = false);
  void OnNativeClose();
  void OnNativeWindowActivationChanged(bool active);

  void NavigationStateChanged(content::WebContents* source,
    content::InvalidateTypes changed_flags);

  // Enable or disable fullscreen mode.
  void SetFullscreen(bool enable);

  base::string16 GetTitle();
  extensions::NativeAppWindow* GetBaseWindow() const;

  content::WebContents* GetActiveWebContents() const;

  // TODO(pettern): fix
  bool requested_alpha_enabled() { return false; }

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

  // Move pinned tabs to remaining window if we have 2 open windows and
  // close the one with pinned tabs.
  void MovePinnedTabsToOtherWindowIfNeeded();

  void UpdateActivation(bool is_active);

  // The Browser object we are associated with.
  std::unique_ptr<Browser> browser_;

  class VivaldiManagePasswordsIconView : public ManagePasswordsIconView {
   public:
    VivaldiManagePasswordsIconView(Browser* browser);

    virtual ~VivaldiManagePasswordsIconView() = default;

    void SetState(password_manager::ui::State state) override;
    bool Update();

   private:
    Browser* browser_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(VivaldiManagePasswordsIconView);
  };

 private:
  struct WindowStateData {
    // State kept to dispatch events on changes.
    bool is_fullscreen = false;
    bool is_maximized = false;
    bool is_minimized = false;
    gfx::Rect bounds;
  };

  friend class VivaldiAppWindowContentsImpl;

  void OnMinimizedChanged(bool minimized);
  void OnMaximizedChanged(bool maximized);
  void OnFullscreenChanged(bool fullscreen);
  void OnPositionChanged();
  void OnActivationChanged(bool activated);

  // Force showing of the window, even if the document has not loaded. This
  // avoids situations where the document fails somehow and no window is shown.
  void ForceShow();

  // From AppWindow:
  // TODO: rename VivaldiNativeAppWindowViews to VivaldiNativeAppWindow.
  std::unique_ptr<VivaldiNativeAppWindowViews> native_app_window_;
  VivaldiAppWindowContentsImpl app_window_contents_;
  std::unique_ptr<extensions::AppDelegate> app_delegate_;

  // Whether the window has been shown or not.
  bool has_been_shown_ = false;

  // Whether the window is hidden or not. Hidden in this context means actively
  // by the chrome.app.window API, not in an operating system context. For
  // example windows which are minimized are not hidden, and windows which are
  // part of a hidden app on OS X are not hidden. Windows which were created
  // with the |hidden| flag set to true, or which have been programmatically
  // hidden, are considered hidden.
  bool is_hidden_ = false;

  // Whether the window is active (focused) or not.
  bool is_active_ = false;

  // Return the old title if the new title is not usable.
  base::string16 old_title_;

  // The window type for this window.
  WindowType window_type_ = WindowType::NORMAL;

  // Reference to the Vivaldi extension.
  extensions::Extension* extension_ = nullptr;

  // The InfoBarContainerWebProxy that contains InfoBars for the current tab.
  vivaldi::InfoBarContainerWebProxy* infobar_container_ = nullptr;

  std::unique_ptr<VivaldiLocationBar> location_bar_;

#if !defined(OS_MACOSX)
  // Last key code received in HandleKeyboardEvent(). For auto repeat detection.
  int last_key_code_ = -1;
#endif  // !defined(OS_MACOSX)

  WindowStateData window_state_data_;

  // Used to timeout the main document loading and force show the window.
  std::unique_ptr<base::OneShotTimer> show_delay_timeout_;

  std::unique_ptr<VivaldiManagePasswordsIconView> icon_view_;
  std::unique_ptr<autofill::AutofillBubbleHandler> autofill_bubble_handler_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserWindow);
};

#endif  // UI_VIVALDI_BROWSER_WINDOW_H_
