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
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_types.h"  // WindowShowState
#include "ui/gfx/image/image_family.h"
#include "ui/infobar_container_web_proxy.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/vivaldi_ui_web_contents_delegate.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/views/win/scoped_fullscreen_visibility.h"
#endif

class ScopedKeepAlive;

#if defined(OS_WIN)
class JumpList;
#endif

class VivaldiBrowserWindow;
class VivaldiLocationBar;
class VivaldiNativeAppWindowViews;

namespace autofill {
class AutofillBubbleHandler;
}

namespace ui {
class Accelerator;
}

namespace views {
class View;
}

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
}

namespace autofill {
class AutofillBubbleBase;
class LocalCardMigrationBubble;
class SaveCardBubbleController;
class LocalCardMigrationBubbleController;
}  // namespace autofill

struct VivaldiBrowserWindowParams {
  static constexpr int kUnspecifiedPosition = INT_MIN;

  VivaldiBrowserWindowParams();
  ~VivaldiBrowserWindowParams();

  VivaldiBrowserWindowParams(const VivaldiBrowserWindowParams&) = delete;
  VivaldiBrowserWindowParams& operator=(const VivaldiBrowserWindowParams&) =
      delete;

  bool settings_window = false;
  bool native_decorations = false;
  bool visible_on_all_workspaces = false;
  bool alpha_enabled = false;
  bool focused = false;

  gfx::Size minimum_size;
  gfx::Rect content_bounds{kUnspecifiedPosition, kUnspecifiedPosition, 0, 0};

  // Initial state of the window.
  ui::WindowShowState state = ui::SHOW_STATE_DEFAULT;

  // The initial URL to show in the window.
  std::string resource_relative_url;

  // The frame that created this frame if any.
  content::RenderFrameHost* creator_frame = nullptr;
};

class VivaldiAutofillBubbleHandler : public autofill::AutofillBubbleHandler {
 public:
  VivaldiAutofillBubbleHandler();
  ~VivaldiAutofillBubbleHandler() override;

  autofill::AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) override;

  autofill::AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      autofill::LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override;

  autofill::AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* web_contents,
      autofill::OfferNotificationBubbleController* controller,
      bool is_user_gesture) override;

  autofill::SaveUPIBubble* ShowSaveUPIBubble(
      content::WebContents* contents,
      autofill::SaveUPIBubbleController* controller) override;

  autofill::AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      autofill::SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) override;

  autofill::AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      autofill::SaveUpdateAddressProfileBubbleController* controller,
      bool is_user_gesture) override;

  autofill::AutofillBubbleBase* ShowEditAddressProfileDialog(
      content::WebContents* web_contents,
      autofill::EditAddressProfileDialogController* controller) override;

  autofill::AutofillBubbleBase* ShowVirtualCardManualFallbackBubble(
      content::WebContents* web_contents,
      autofill::VirtualCardManualFallbackBubbleController* controller,
      bool is_user_gesture) override;

  void OnPasswordSaved() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VivaldiAutofillBubbleHandler);
};

// An implementation of BrowserWindow used for Vivaldi.
class VivaldiBrowserWindow final
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

  static base::TimeTicks GetFirstWindowCreationTime();

  // Return the Vivaldi Window that shows the given Browser. Return null when
  // browser is null or is not held by a Vivaldi Window.
  static VivaldiBrowserWindow* FromBrowser(const Browser* browser);

  // Create a new VivaldiBrowserWindow;
  static VivaldiBrowserWindow* CreateVivaldiBrowserWindow(
      std::unique_ptr<Browser> browser);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }
  content::WebContents* web_contents() const { return web_contents_.get(); }

  SessionID::id_type id() const { return browser()->session_id().id(); }

  VivaldiNativeAppWindowViews* views() const { return views_.get(); }

  // Takes ownership of |browser|.
  void CreateWebContents(std::unique_ptr<Browser> browser,
                         const VivaldiBrowserWindowParams& params);

  void ContentsDidStartNavigation();

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // ExtensionFunctionDispatcher::Delegate implementation.
  extensions::WindowController* GetExtensionWindowController() const override;
  content::WebContents* GetAssociatedWebContents() const override;

  // infobars::InfoBarContainer::Delegate
  void InfoBarContainerStateChanged(bool is_animating) override;

  void HandleMouseChange(bool motion);

  bool ConfirmWindowClose();

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
  void ToolbarSizeChanged(bool is_animating) override {}
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInactivePopupForAccessibility() override {}
  void FocusHelpBubble() override {}
  void RotatePaneFocus(bool forwards) override {}
  void ShowAppMenu() override {}
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  gfx::Size GetContentsSize() const override;
  void SetContentsSize(const gfx::Size& size) override {}
  void UpdatePageActionIcon(PageActionIconType type) override;
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
      base::OnceCallback<void(bool)> callback) override;
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override {}
  void VivaldiShowWebsiteSettingsAt(Profile* profile,
                                    content::WebContents* web_contents,
                                    const GURL& url,
                                    gfx::Point pos) override;
  void CutCopyPaste(int command_id) override {}
  bool IsOnCurrentWorkspace() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override {}
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override {}
  bool CanUserExitFullscreen() const override;
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowOneClickSigninConfirmation(
      const std::u16string& email,
      base::OnceCallback<void(bool)> start_sync_callback) override {}
  void OnTabRestored(int command_id) override {}
  void OnTabDetached(content::WebContents* contents, bool was_active) override {
  }
  void TabDraggingStatusChanged(bool is_dragging) override {}
  void LinkOpeningFromGesture(WindowOpenDisposition disposition) override {}
  // Shows in-product help for the given feature.
  void ShowInProductHelpPromo(InProductHelpFeature iph_feature) override {}
  void UpdateFrameColor() override {}
#if !defined(OS_ANDROID)
  void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      PageActionIconType icon_type,
      const absl::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override {}
#endif
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfBubble(
      content::WebContents* contents,
      send_tab_to_self::SendTabToSelfBubbleController* controller,
      bool is_user_gesture) override;
  sharing_hub::SharingHubBubbleView* ShowSharingHubBubble(
      content::WebContents* contents,
      sharing_hub::SharingHubBubbleController* controller,
      bool is_user_gesture) override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override {}
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowHatsDialog(
      const std::string& site_id,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const std::map<std::string, bool>& product_specific_data) override {}
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void ShowCaretBrowsingDialog() override {}
  void CreateTabSearchBubble() override {}
  void CloseTabSearchBubble() override {}
  FeaturePromoController* GetFeaturePromoController() override;
  void ShowIncognitoClearBrowsingDataDialog() override {}
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
                       ExclusiveAccessBubbleType bubble_type,
                       int64_t display_id) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetActiveWebContents() override;

  ExclusiveAccessContext* GetExclusiveAccessContext() override;
  void ShowAvatarBubbleFromAvatarButton(
      AvatarBubbleMode mode,
      signin_metrics::AccessPoint access_point,
      bool is_source_keyboard) override {}

  void MaybeShowProfileSwitchIPH() override {}

  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;

  void ResetDockingState(int tab_id);

  bool IsToolbarShowing() const override;

  // If moved is true, the change is caused by a move
  void OnNativeWindowChanged(bool moved = false);
  void OnNativeClose();
  void OnNativeWindowActivationChanged(bool active);

  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags);

  // Enable or disable fullscreen mode.
  void SetFullscreen(bool enable);

  std::u16string GetTitle();
  views::View* GetContentsView() const;
  gfx::NativeView GetNativeView();

  // View to pass to BubbleDialogDelegateView and its sublasses.
  views::View* GetBubbleDialogAnchor() const;

  content::WebContents* GetActiveWebContents() const;

  // TODO(pettern): fix
  bool requested_alpha_enabled() { return false; }

  const extensions::Extension* extension() { return extension_; }

  bool is_hidden() { return is_hidden_; }

  WindowType type() { return window_type_; }

  void SetWindowState(ui::WindowShowState show_state) {
    window_state_data_.state = show_state;
  }

  // window for the callback is null on errors or if the user closed the
  // window before the initial content was loaded.
  using DidFinishNavigationCallback =
      base::OnceCallback<void(VivaldiBrowserWindow* window)>;
  void SetDidFinishNavigationCallback(DidFinishNavigationCallback callback);

 private:
  class VivaldiManagePasswordsIconView : public ManagePasswordsIconView {
   public:
    VivaldiManagePasswordsIconView(VivaldiBrowserWindow& window);

    virtual ~VivaldiManagePasswordsIconView() = default;

    void SetState(password_manager::ui::State state) override;
    bool Update();

   private:
    VivaldiBrowserWindow& window_;

    DISALLOW_COPY_AND_ASSIGN(VivaldiManagePasswordsIconView);
  };

  struct WindowStateData {
    // State kept to dispatch events on changes.
    ui::WindowShowState state = ui::SHOW_STATE_DEFAULT;
    gfx::Rect bounds;
  };

  friend class VivaldiUIWebContentsDelegate;
  friend class VivaldiNativeAppWindowViews;

  void DestroyBrowser() override;

  void DeleteThis();

  // Move pinned tabs to remaining window if we have 2 open windows and
  // close the one with pinned tabs.
  void MovePinnedTabsToOtherWindowIfNeeded();

  void UpdateActivation(bool is_active);
  void OnIconImagesLoaded(gfx::ImageFamily image_family);

  void OnStateChanged(ui::WindowShowState state);
  void OnPositionChanged();
  void OnActivationChanged(bool activated);

  // Force showing of the window, even if the document has not loaded. This
  // avoids situations where the document fails somehow and no window is shown.
  void ForceShow();

  // VivaldiQuitConfirmationDialog::CloseCallback
  void ContinueClose(bool quiting, bool close, bool stop_asking);

  void OnDidFinishNavigation(bool success);

  // The Browser object for this window. This must be the first field in the
  // class so it is destructed after any field below that may refer to the
  // browser.
  std::unique_ptr<Browser> browser_;

  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<VivaldiNativeAppWindowViews> views_;
  VivaldiUIWebContentsDelegate web_contents_delegate_{this};
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

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

  bool quit_dialog_shown_ = false;
  bool close_dialog_shown_ = false;

  // The window type for this window.
  WindowType window_type_ = NORMAL;

  // Reference to the Vivaldi extension.
  extensions::Extension* extension_ = nullptr;

  // The InfoBarContainerWebProxy that contains InfoBars for the current tab.
  vivaldi::InfoBarContainerWebProxy* infobar_container_ = nullptr;

  std::unique_ptr<VivaldiLocationBar> location_bar_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

#if !defined(OS_MAC)
  // Last key code received in HandleKeyboardEvent(). For auto repeat detection.
  int last_key_code_ = -1;
#endif  // !defined(OS_MAC)
  bool last_motion_ = false;
  WindowStateData window_state_data_;

  // Used to timeout the main document loading and force show the window.
  std::unique_ptr<base::OneShotTimer> show_delay_timeout_;

  VivaldiManagePasswordsIconView icon_view_{*this};
  std::unique_ptr<autofill::AutofillBubbleHandler> autofill_bubble_handler_;

  DidFinishNavigationCallback did_finish_navigation_callback_;

  base::WeakPtrFactory<VivaldiBrowserWindow> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(VivaldiBrowserWindow);
};

#endif  // UI_VIVALDI_BROWSER_WINDOW_H_
