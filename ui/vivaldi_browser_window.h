// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_BROWSER_WINDOW_H_
#define UI_VIVALDI_BROWSER_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/web_modal/modal_dialog_host.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "ui/gfx/image/image_family.h"
#include "ui/infobar_container_web_proxy.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"

#include "ui/vivaldi_ui_web_contents_delegate.h"

class ScopedKeepAlive;
class SkRegion;

class ToastSpecification;

class VivaldiLocationBar;
class VivaldiWindowWidgetDelegate;

namespace ui {
class Accelerator;
}

namespace views {
class View;
class Widget;
}  // namespace views

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
class VivaldiRootDocumentHandler;
}  // namespace extensions

namespace vivaldi {
class InfoBarContainerWebProxy;
}

class VivaldiUIRelay: public send_tab_to_self::ReceivingUiHandler {
 public:
  VivaldiUIRelay(Profile* profile);
  virtual ~VivaldiUIRelay() {}

  // ReceivingUiHandler
  void DisplayNewEntries(const std::vector<const send_tab_to_self::SendTabToSelfEntry*>& new_entries) override;
  void DismissEntries(const std::vector<std::string>& guids) override;
  const Profile* profile() const override;

 private:
  Profile* profile_;
};

class VivaldiToolbarButtonProvider : public ToolbarButtonProvider {
 public:
  VivaldiToolbarButtonProvider(VivaldiBrowserWindow* window);
  //  VivaldiToolbarButtonProvider(const ToolbarView&) = delete;
  //  VivaldiToolbarButtonProvider& operator=(const ToolbarView&) = delete;

  ~VivaldiToolbarButtonProvider() override;

 private:
  // ToolbarButtonProvider:
  ExtensionsToolbarContainer* GetExtensionsToolbarContainer() override;
  gfx::Size GetToolbarButtonSize() const override;
  views::View* GetDefaultExtensionDialogAnchorView() override;
  PageActionIconView* GetPageActionIconView(PageActionIconType type) override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_bottom) override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;
  views::View* GetAnchorView(
      std::optional<PageActionIconType> type) override;  // the one
  void ZoomChangedForActiveTab(bool can_show_bubble) override;
  AvatarToolbarButton* GetAvatarToolbarButton() override;
  ToolbarButton* GetBackButton() override;
  ReloadButton* GetReloadButton() override;
  IntentChipButton* GetIntentChipButton() override;
  DownloadToolbarButtonView* GetDownloadButton() override;
  //SidePanelToolbarButton* GetSidePanelButton() override;

  raw_ptr<VivaldiBrowserWindow> window_ = nullptr;
};

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
  std::string workspace;

  // Singleton key used when set.
  std::string window_key;

  gfx::Size minimum_size;
  gfx::Rect content_bounds{kUnspecifiedPosition, kUnspecifiedPosition, 0, 0};

  // Initial state of the window.
  ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kDefault;

  // The initial URL to show in the window.
  std::string resource_relative_url;

  // The frame that created this frame if any.
  raw_ptr<content::RenderFrameHost> creator_frame = nullptr;
};

// The class that binds BrowserWindow with native UI. Initially it started as a
// fork of deprecated NativeAppWindow from
// chromium/extensions/components/native_app_window/native_app_window.h, but
// these days it follows mostly views::BrowserViews. Compared with the latter
// this class owns single WebContents where out UI runs which in turn uses
// <webview> tags to show web pages.
//
// Another key difference is that while BrowserView subclasses
// views::ClientView, this class owns views::Widget (widget_ field) that
// represents the whole OS window. The picture below adapted from
// chromium/ui/views/window/non_client_view.h shows the hierarhy of Chromium UI
// classes that we use.
//
//  +- views::Widget (widget_ field for OS window)-----+ |
//  | +- views::RootView ------------------------------+ |
//  | | +- views::NonClientView----------------------+ | |
//  | | | +- views::NonClientFrameView subclass ---+ | | |
//  | | | |                                        | | | |
//  | | | | << all painting and event receiving >> | | | |
//  | | | | << of the non-client areas of a     >> | | | |
//  | | | | << views::Widget.                   >> | | | |
//  | | | |                                        | | | |
//  | | | | +- views::ClientView subclass -------+ | | | |
//  | | | | | +- views::WebView ---------------+ | | | | |
//  | | | | | | +- views::NativeViewHost ----+ | | | | | |
//  | | | | +------------------------------------+ | | | |
//  | | | | +------------------------------------+ | | | |
//  | | | +----------------------------------------+ | | |
//  | | +--------------------------------------------+ | |
//  | +------------------------------------------------+ |
//  +----------------------------------------------------+
//
// In addition, we also subclass views::WidgetDelegate and views::NativeWidget
// and related classes.
//
// TODO(igor@vivaldi.com) As we draw everything in JS consider simplifying this
// like replacing NonClientView with WebView or NativeViewHost or even
// eliminating the whole views::* hierarhy and just coupling gfx::NativeView
// with WebContext on each platform. But that could have resulted into
// forking even more Chromium code compared with what we have now.
//
// views::BrowserView directly extends various helper interfaces in addition to
// BrowserWindow. To keep the public interface of this class small we implement
// those using separated classes like InterfaceHelper below.
//
class VivaldiBrowserWindow final : public BrowserWindow {
 public:

  VivaldiBrowserWindow();
  ~VivaldiBrowserWindow() override;
  VivaldiBrowserWindow(const VivaldiBrowserWindow&) = delete;
  VivaldiBrowserWindow& operator=(const VivaldiBrowserWindow&) = delete;

  enum WindowType {
    NORMAL,
    POPUP,
    SETTINGS,
  };

  static base::TimeTicks GetFirstWindowCreationTime();

  // Return the Vivaldi Window that shows the given Browser. Return null when
  // browser is null or is not held by a Vivaldi Window.
  static VivaldiBrowserWindow* FromBrowser(const Browser* browser);

  // Get the Vivaldi Window with the given id or null if such windows does not
  // exist or was closed.
  static VivaldiBrowserWindow* FromId(SessionID::id_type window_id);

  // Create a new VivaldiBrowserWindow;
  static VivaldiBrowserWindow* CreateVivaldiBrowserWindow(
      std::unique_ptr<Browser> browser);

  // Returns a Browser instance of this view.
  Browser* browser() { return browser_.get(); }
  const Browser* browser() const { return browser_.get(); }
  content::WebContents* web_contents() const { return web_contents_.get(); }

  // Use the id together with FromId() to store long-term references to the
  // window.
  SessionID::id_type id() const { return browser()->session_id().id(); }

  bool is_hidden() { return is_hidden_; }

  WindowType type() { return window_type_; }

  const extensions::Extension* extension() { return extension_; }

  bool with_native_frame() const { return with_native_frame_; }

  int resize_inside_bounds_size() const { return resize_inside_bounds_size_; }

  int resize_area_corner_size() const { return resize_area_corner_size_; }

  gfx::Size minimum_size() const { return minimum_size_; }

  const SkRegion* draggable_region() const { return draggable_region_.get(); }

  views::Widget* GetWidget() const { return widget_; }

  views::View* GetWebView() const;

  // TODO(pettern): fix
  bool requested_alpha_enabled() { return false; }

  void SetWindowState(ui::mojom::WindowShowState show_state) {
    window_state_data_.state = show_state;
  }
  ui::mojom::WindowShowState GetWindowState() {
    return window_state_data_.state;
  }

  // Takes ownership of |browser|.
  void CreateWebContents(std::unique_ptr<Browser> browser,
                         const VivaldiBrowserWindowParams& params);

  // DidStartNavigation for the window contents.
  void ContentsDidStartNavigation();
  // DocumentOnLoadCompletedInPrimaryMainFrame for the window webcontents.
  void ContentsLoadCompletedInMainFrame();

  bool ConfirmWindowClose();

  void HandleMouseChange(bool motion);

  // If moved is true, the change is caused by a move
  void OnNativeWindowChanged(bool moved = false);
  void OnNativeClose();
  void OnNativeWindowActivationChanged(bool active);

  void NavigationStateChanged(content::WebContents* source,
                              content::InvalidateTypes changed_flags);
  void OnUIReady();

  // Enable or disable fullscreen mode.
  void SetFullscreen(bool enable);

  void ShowForReal();

  Profile* GetProfile() const;

  std::u16string GetTitle();

  views::View* GetContentsView() const;

  gfx::NativeView GetNativeView();

  ui::AcceleratorProvider* GetAcceleratorProvider();

  // View to pass to BubbleDialogDelegateView and its sublasses.
  views::View* GetBubbleDialogAnchor() const;

  void ResetDockingState(int tab_id);

  void SetWindowKey(std::string key) { window_key_ = key; }

  // window for the callback is null on errors or if the user closed the
  // window before the initial content was loaded.
  using DidFinishNavigationCallback =
      base::OnceCallback<void(VivaldiBrowserWindow* window)>;
  void SetDidFinishNavigationCallback(DidFinishNavigationCallback callback);

  void SetWindowURL(std::string s) { resource_relative_url_ = std::move(s); }

  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents);

  const gfx::Rect& GetMaximizeButtonBounds() const {
    return maximize_button_bounds_;
  }

  void UpdateMaximizeButtonPosition(const gfx::Rect& rect);

  // Getter for the `window.setResizable(bool)` state.
  std::optional<bool> GetCanResizeFromWebAPI() const;

  //
  // BrowserWindow overrides
  //
  void DestroyBrowser() override;
  ExclusiveAccessContext* GetExclusiveAccessContext() override;
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
  void TemporarilyShowBookmarkBar(base::TimeDelta duration) override {}
  void UpdateDevTools() override;
  void UpdateLoadingAnimations(bool should_animate) override {}
  void SetStarredState(bool is_starred) override {}
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents,
                          int index,
                          int reason) override;
  void ZoomChangedForActiveTab(bool can_show_bubble) override {}
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
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
  bool IsForceFullscreen() const override;
  void SetForceFullscreen(bool force_fullscreen) override {}
  LocationBar* GetLocationBar() const override;
  void SetFocusToLocationBar(bool select_all) override {}
  void UpdateReloadStopState(bool is_loading, bool force) override {}
  void UpdateToolbar(content::WebContents* contents) override;
  bool UpdateToolbarSecurityState() override;
  bool IsToolbarShowing() const override;
  void MaybeShowProfileSwitchIPH() override {}
  void MaybeShowSupervisedUserProfileSignInIPH() override {}
  void FocusToolbar() override {}
  void ToolbarSizeChanged(bool is_animating) override {}
  void FocusAppMenu() override {}
  void FocusBookmarksToolbar() override {}
  void FocusInactivePopupForAccessibility() override {}
  void RotatePaneFocus(bool forwards) override {}
  void FocusWebContentsPane() override {}
  void ShowAppMenu() override {}
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) override;
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
  sharing_hub::ScreenshotCapturedBubble* ShowScreenshotCapturedBubble(
      content::WebContents* contents,
      const gfx::Image& image) override;
  qrcode_generator::QRCodeGeneratorBubbleView* ShowQRCodeGeneratorBubble(
      content::WebContents* contents,
      const GURL& url,
      bool show_back_button) override;
  send_tab_to_self::SendTabToSelfBubbleView*
  ShowSendTabToSelfDevicePickerBubble(content::WebContents* contents) override;
  send_tab_to_self::SendTabToSelfBubbleView* ShowSendTabToSelfPromoBubble(
      content::WebContents* contents,
      bool show_signin_button) override;
  ShowTranslateBubbleResult ShowTranslateBubble(
      content::WebContents* contents,
      translate::TranslateStep step,
      const std::string& source_language,
      const std::string& target_language,
      translate::TranslateErrors error_type,
      bool is_user_gesture) override;
  void StartPartialTranslate(const std::string& source_language,
                             const std::string& target_language,
                             const std::u16string& text_selection) override {}
  void ShowAvatarBubbleFromAvatarButton(bool is_source_accelerator) override {}
  bool IsDownloadShelfVisible() const override;
  DownloadShelf* GetDownloadShelf() override;
  views::View* GetTopContainer() override;
  DownloadBubbleUIController* GetDownloadBubbleUIController() override;
  void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      Browser::DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) override;
  void UserChangedTheme(BrowserThemeChangeType theme_change_type) override {}
  void VivaldiShowWebsiteSettingsAt(Profile* profile,
                                    content::WebContents* web_contents,
                                    const GURL& url,
                                    gfx::Point pos) override;
  bool IsOnCurrentWorkspace() const override;
  void SetTopControlsShownRatio(content::WebContents* web_contents,
                                float ratio) override {}
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  ui::NativeTheme* GetNativeTheme() override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  const ui::ColorProvider* GetColorProvider() const override;
  ui::ElementContext GetElementContext() override;
  int GetTopControlsHeight() const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override {}
  std::unique_ptr<FindBar> CreateFindBar() override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;
  void ShowOneClickSigninConfirmation(
      const std::u16string& email,
      base::OnceCallback<void(bool)> start_sync_callback) override {}
  void OnTabDetached(content::WebContents* contents, bool was_active) override;
  void TabDraggingStatusChanged(bool is_dragging) override {}
  void LinkOpeningFromGesture(WindowOpenDisposition disposition) override {}
  // Shows in-product help for the given feature.
#if !BUILDFLAG(IS_ANDROID)
  void ShowIntentPickerBubble(
      std::vector<apps::IntentPickerAppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      apps::IntentPickerBubbleType bubble_type,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse callback) override {}
#endif
  sharing_hub::SharingHubBubbleView* ShowSharingHubBubble(
      share::ShareAttempt attempt) override;
  ExtensionsContainer* GetExtensionsContainer() override;
  void UpdateCustomTabBarVisibility(bool visible, bool animate) override {}
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data) override;
  void ShowHatsDialog(
      const std::string& site_id,
      const std::optional<std::string>& histogram_name,
      const std::optional<uint64_t> hats_survey_ukm_id,
      base::OnceClosure success_callback,
      base::OnceClosure failure_callback,
      const SurveyBitsData& product_specific_bits_data,
      const SurveyStringData& product_specific_string_data) override {}
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void ShowCaretBrowsingDialog() override {}
  void CreateTabSearchBubble(
      tab_search::mojom::TabSearchSection section,
      tab_search::mojom::TabOrganizationFeature organization_feature) override {
  }
  void CloseTabSearchBubble() override {}
  bool IsFeaturePromoActive(
      const base::Feature& iph_feature) const override;
  user_education::FeaturePromoResult CanShowFeaturePromo(
      const base::Feature& iph_feature) const override;
  void MaybeShowFeaturePromo(
      user_education::FeaturePromoParams params) override {}
  void MaybeShowStartupFeaturePromo(
      user_education::FeaturePromoParams params) override {}
  bool AbortFeaturePromo(const base::Feature& iph_feature) override;
  user_education::FeaturePromoHandle CloseFeaturePromoAndContinue(
      const base::Feature& iph_feature) override;
  bool NotifyFeaturePromoFeatureUsed(
      const base::Feature& feature,
      FeaturePromoFeatureUsedAction action) override;
  void NotifyAdditionalConditionEvent(const char* event_name) override {}
  user_education::DisplayNewBadge MaybeShowNewBadgeFor(
      const base::Feature& feature) override;
  void NotifyNewBadgeFeatureUsed(const base::Feature& feature) override {}
  void ShowIncognitoClearBrowsingDataDialog() override {}
  void ShowIncognitoHistoryDisclaimerDialog() override {}
  std::string GetWorkspace() const override;
  bool IsVisibleOnAllWorkspaces() const override;
  bool IsLocationBarVisible() const override;
  bool IsBorderlessModeEnabled() const override;
  void ShowChromeLabs() override {}

  // Notifies `BrowserView` about the resizable boolean having been set vith
  // `window.setResizable(bool)` API.
  void OnCanResizeFromWebAPIChanged() override;
  bool GetCanResize() override;
  ui::mojom::WindowShowState GetWindowShowState() const override;
  views::WebView* GetContentsWebView() override;
  BrowserView* AsBrowserView() override;


  // BrowserWindow overrides end

  // BaseWindow overrides
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel order) override {}

  void BeforeUnloadFired(content::WebContents* source);

  // Returns true if this window contains pinned or workspace tabs. To be used
  // for Mac when last window closes.
  bool HasPersistentTabs();

  // Returns a list of pinned and workspace tabs in the window.
  std::vector<int> GetPersistentTabIds();

  // Move pinned or workspace tabs to remaining window if we have 2 open
  // windows and close the one with pinned/workspace tabs.
  void MovePersistentTabsToOtherWindowIfNeeded();

  // Called from unload handler during the close sequence if the sequence is
  // aborted.
  static void CancelWindowClose();

  ToolbarButtonProvider* toolbar_button_provider() {
    return toolbar_button_provider_.get();
  }

  void ShowToast(const ToastSpecification* spec, std::vector<std::u16string> body_string_replacement_params);

 protected:
  user_education::FeaturePromoController* GetFeaturePromoControllerImpl()
      override;

 private:
  enum QuitAction { ShowDialogOnQuit = 0, SaveSessionOnQuit,  DoNothingOnQuit };
  enum class CloseDialogMode { CloseWindow = 0, QuitApplication };

  // Ensures only one window can display a quit dialog on exit.
  bool AcquireQuitDialog();
  // Signals all windows that the quit dialog confirms the action.
  void AcceptQuitForAllWindows();
  // Sets the owner of the quit dialog (the window that shows the dialog).
  void SetQuitDialogOwner(VivaldiBrowserWindow* owner);
  // Determines what to do on exit.
  QuitAction GetQuitAction();
  // Determines if a window close confirmation dialog should be shown.
  bool ShouldShowDialogOnCloseWindow();
  // Determines if a session of persistent tabs should be saved.
  bool ShouldSavePersistentTabsOnCloseWindow();

  // Implementation of various interface-like Chromium classes is in this inner
  // class not to pollute with extra details the main class.
  class InterfaceHelper;

  friend InterfaceHelper;
  friend VivaldiUIWebContentsDelegate;
  friend VivaldiWindowWidgetDelegate;

  struct WindowStateData {
    // State kept to dispatch events on changes.
    ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kDefault;
    gfx::Rect bounds;
  };

  void InitWidget(const VivaldiBrowserWindowParams& create_params);

  void DeleteThis();


  void UpdateActivation(bool is_active);
  void OnIconImagesLoaded(gfx::ImageFamily image_family);

  void OnStateChanged(ui::mojom::WindowShowState state);
  void OnPositionChanged();
  void OnActivationChanged(bool activated);

  void OnViewWasResized();

  // VivaldiQuitConfirmationDialog::CloseCallback
  void ContinueClose(CloseDialogMode mode, bool close, bool stop_asking);

  void OnDidFinishNavigation(bool success);

  void InjectVivaldiWindowIdComplete(base::Value result);

  void ReportWebsiteSettingsState(bool visible);
  void OnWebsiteSettingsStatClosed(views::Widget::ClosedReason closed_reason,
                                   bool reload_prompt);

  content::WebContents* GetActiveWebContents() const;

  gfx::Insets GetFrameInsets() const;

  // Helper methods implemented only on Windows

  void SetupShellIntegration(const VivaldiBrowserWindowParams& create_params);
  int GetCommandIDForAppCommandID(int app_command_id) const;


  void CloseCleanup();

  // Saved session history. To be called when last window closes before content
  // is removed.
  void AutoSaveSession();
  // This must be the first field for the class to ensure that it outlives any
  // other field that use the class as a delegate and embeds a raw pointer to
  // it.
  const std::unique_ptr<InterfaceHelper> interface_helper_;

  // The Browser object for this window. This must be the first field in the
  // class so it is destructed after any field below that may refer to the
  // browser.
  std::unique_ptr<Browser> browser_;

  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<views::Widget> widget_ = nullptr;
  std::unique_ptr<VivaldiWindowWidgetDelegate> widget_delegate_;
  VivaldiUIWebContentsDelegate web_contents_delegate_{this};
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

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
  // Only the owner can show a dialog.
  raw_ptr<VivaldiBrowserWindow> quit_dialog_owner_ = nullptr;

  // When true, use the system frame with OS-rendered titlebar and window
  // control buttons.
  bool with_native_frame_ = false;

  // Allow resize for clicks this many pixels inside the bounds.
  int resize_inside_bounds_size_ = 5;

  // Size in pixels of the lower-right corner resize handle.
  int resize_area_corner_size_ = 16;

  gfx::Size minimum_size_;

  // The region in the window to drag it around OS desktop.
  std::unique_ptr<SkRegion> draggable_region_;

  // The window type for this window.
  WindowType window_type_ = NORMAL;

  // Caching the last value of `PageData::can_resize_` that has been notified to
  // the WidgetObservers to avoid notifying them when nothing has changed.
  std::optional<bool> cached_can_resize_from_web_api_;

  // Reference to the Vivaldi extension.
  raw_ptr<extensions::Extension> extension_ = nullptr;

  // The InfoBarContainerWebProxy that contains InfoBars for the current tab.
  std::unique_ptr<vivaldi::InfoBarContainerWebProxy> infobar_container_;

  std::unique_ptr<VivaldiLocationBar> location_bar_;

  // Used when loading the url in the webcontents lazily.
  std::string resource_relative_url_;
  // |rootdochandler_| is BrowserContext bound and outlives this.
  raw_ptr<extensions::VivaldiRootDocumentHandler> root_doc_handler_ = nullptr;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // The handler responsible for showing autofill bubbles.
  std::unique_ptr<VivaldiToolbarButtonProvider> toolbar_button_provider_;

  std::unique_ptr<autofill::AutofillBubbleHandler> autofill_bubble_handler_;


#if !BUILDFLAG(IS_MAC)
  // Last key code received in HandleKeyboardEvent(). For auto repeat detection.
  int last_key_code_ = -1;
#endif  // !BUILDFLAG(IS_MAC)
  bool last_motion_ = false;
  WindowStateData window_state_data_;

  std::string window_key_;

  // Coordinates of the bounds of the maximize button as rendered by our UI.
  // This is used to implement the Snap Layout maximized button menu on Windows.
  //
  // TODO(igor@vivaldi.com): Figure out how to use this on Mac to show the
  // maximized menu there.
  gfx::Rect maximize_button_bounds_;

  bool is_moving_persistent_tabs_ = false;

  // The icon family for the task bar and elsewhere.
  gfx::ImageFamily icon_family_;

  DidFinishNavigationCallback did_finish_navigation_callback_;

  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      modal_dialog_observers_;

  base::WeakPtrFactory<VivaldiBrowserWindow> weak_ptr_factory_{this};
};

#endif  // UI_VIVALDI_BROWSER_WINDOW_H_
