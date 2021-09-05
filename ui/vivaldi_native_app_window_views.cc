// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "ui/vivaldi_native_app_window_views.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/draggable_region.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_family.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"
#include "ui/vivaldi_quit_confirmation_dialog.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if defined(OS_WIN)
#include "browser/win/vivaldi_utils.h"
#endif  // defined(OS_WIN)

namespace {

// Make sure we answer correctly for ClientView::CanClose to make sure the exit
// sequence is started when closing a BrowserWindow. See comment in
// fast_unload_controller.h.
class VivaldiWindowClientView : public views::ClientView {
 public:
  VivaldiWindowClientView(views::Widget* widget,
                          views::View* contents_view,
                          VivaldiBrowserWindow* window)
      : views::ClientView(widget, contents_view), window_(window) {}

  // views::ClientView:
  views::CloseRequestResult OnWindowCloseRequested() override {
    return (window_->ConfirmWindowClose()
                ? views::CloseRequestResult::kCanClose
                : views::CloseRequestResult::kCannotClose);
  }

 private:
  VivaldiBrowserWindow* const window_;
  DISALLOW_COPY_AND_ASSIGN(VivaldiWindowClientView);
};

const int kLargeIconSizeViv = 256;
const int kSmallIconSizeViv = 16;

// This is based on GetInitialWindowBounds() from
// chromium/extensions/browser/app_window/app_window.cc
gfx::Rect GetInitialWindowBounds(
    const VivaldiBrowserWindowParams& params,
    const gfx::Insets& frame_insets) {
  // Combine into a single window bounds.
  gfx::Rect combined_bounds(VivaldiBrowserWindowParams::kUnspecifiedPosition,
                            VivaldiBrowserWindowParams::kUnspecifiedPosition, 0,
                            0);
  if (params.content_bounds.x() !=
      VivaldiBrowserWindowParams::kUnspecifiedPosition)
    combined_bounds.set_x(params.content_bounds.x() - frame_insets.left());
  if (params.content_bounds.y() !=
      VivaldiBrowserWindowParams::kUnspecifiedPosition)
    combined_bounds.set_y(params.content_bounds.y() - frame_insets.top());
  if (params.content_bounds.width() > 0) {
    combined_bounds.set_width(params.content_bounds.width() +
                              frame_insets.width());
  }
  if (params.content_bounds.height() > 0) {
    combined_bounds.set_height(
        params.content_bounds.height() + frame_insets.height());
  }

  // Constrain the bounds.
  gfx::Size size = combined_bounds.size();
  size.SetToMax(params.minimum_size);
  combined_bounds.set_size(size);

  return combined_bounds;
}

}  // namespace

VivaldiNativeAppWindowViews::VivaldiNativeAppWindowViews()
    : window_(nullptr), web_view_(nullptr), widget_(nullptr) {}

VivaldiNativeAppWindowViews::~VivaldiNativeAppWindowViews() = default;

bool VivaldiNativeAppWindowViews::IsOnCurrentWorkspace() const {
  return true;
}

void VivaldiNativeAppWindowViews::InitializeDefaultWindow(
  const VivaldiBrowserWindowParams& create_params) {
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);

  init_params.delegate = this;
  init_params.remove_standard_frame = frameless_;
  init_params.use_system_default_icon = false;
  if (create_params.alpha_enabled) {
    init_params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

    // The given window is most likely not rectangular since it uses
    // transparency and has no standard frame, don't show a shadow for it.
    // TODO(skuhne): If we run into an application which should have a shadow
    // but does not have, a new attribute has to be added.
    if (frameless_)
      init_params.shadow_type =
          views::Widget::InitParams::ShadowType::kNone;
  }
  init_params.visible_on_all_workspaces =
      create_params.visible_on_all_workspaces;

  OnBeforeWidgetInit(init_params);
  widget()->Init(std::move(init_params));

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  widget()->SetNativeWindowProperty(Profile::kProfileKey,
                                    window_->browser()->profile());

  // The frame insets are required to resolve the bounds specifications
  // correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();

  widget_->OnSizeConstraintsChanged();

  gfx::Rect window_bounds = GetInitialWindowBounds(create_params, frame_insets);
  if (!window_bounds.IsEmpty()) {
    bool position_specified =
      window_bounds.x() != VivaldiBrowserWindowParams::kUnspecifiedPosition &&
      window_bounds.y() != VivaldiBrowserWindowParams::kUnspecifiedPosition;
    if (!position_specified)
      widget()->CenterWindow(window_bounds.size());
    else
      widget()->SetBounds(window_bounds);
  }
}

void VivaldiNativeAppWindowViews::Init(
    VivaldiBrowserWindow* window,
    const VivaldiBrowserWindowParams& create_params) {
  window_ = window;
  frameless_ = !create_params.native_decorations;
  minimum_size_ = create_params.minimum_size;

  widget_ = new views::Widget;
  widget_->AddObserver(this);

  InitializeDefaultWindow(create_params);

  OnViewWasResized();
}

void VivaldiNativeAppWindowViews::UpdateEventTargeterWithInset() {
#if !defined(OS_MAC)
  bool is_maximized = IsMaximized();
  aura::Window* window = widget()->GetNativeWindow();
  int resize_inside =
      is_maximized ? 0 : 5;  // frame->resize_inside_bounds_size();
  gfx::Insets inset(resize_inside, resize_inside, resize_inside, resize_inside);
  // Add the EasyResizeWindowTargeter on the window, not its root window. The
  // root window does not have a delegate, which is needed to handle the event
  // in Linux.
  std::unique_ptr<ui::EventTargeter> old_eventtarget =
    window->SetEventTargeter(std::unique_ptr<aura::WindowTargeter>(
      new wm::EasyResizeWindowTargeter(inset, inset)));
  delete old_eventtarget.release();
#endif
}


void VivaldiNativeAppWindowViews::OnCanHaveAlphaEnabledChanged() {
  window_->OnNativeWindowChanged();
}

void VivaldiNativeAppWindowViews::OnViewWasResized() {
  for (auto& observer : modal_dialog_host_.observers_)
    observer.OnPositionRequiresUpdate();
}

bool VivaldiNativeAppWindowViews::IsActive() const {
  return widget_ ? widget_->IsActive() : false;
}

bool VivaldiNativeAppWindowViews::IsMaximized() const {
  return widget_ ? widget_->IsMaximized() : false;
}

bool VivaldiNativeAppWindowViews::IsMinimized() const {
  return widget_ ? widget_->IsMinimized() : false;
}

bool VivaldiNativeAppWindowViews::IsFullscreen() const {
  return widget_ ? widget_->IsFullscreen() : false;
}

gfx::NativeWindow VivaldiNativeAppWindowViews::GetNativeWindow() const {
  return widget_ ? widget_->GetNativeWindow() : nullptr;
}

gfx::Rect VivaldiNativeAppWindowViews::GetRestoredBounds() const {
  return widget_ ? widget_->GetRestoredBounds() : gfx::Rect();
}

ui::WindowShowState VivaldiNativeAppWindowViews::GetRestoredState() const {
  if (IsFullscreen())
    return ui::SHOW_STATE_FULLSCREEN;
  if (IsMaximized())
    return ui::SHOW_STATE_MAXIMIZED;

  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect VivaldiNativeAppWindowViews::GetBounds() const {
  return widget_ ? widget_->GetWindowBoundsInScreen() : gfx::Rect();
}

void VivaldiNativeAppWindowViews::Show() {
  // In maximized state IsVisible is true and activate does not show a
  // hidden window.
  ui::WindowShowState current_state = GetRestoredState();
  if (widget_->IsVisible() && current_state != ui::SHOW_STATE_MAXIMIZED) {
    widget_->Activate();
    return;
  }

  widget_->Show();
}

void VivaldiNativeAppWindowViews::Hide() {
  widget_->Hide();
}

bool VivaldiNativeAppWindowViews::IsVisible() const {
  return widget_->IsVisible();
}

void VivaldiNativeAppWindowViews::Activate() {
  widget_->Activate();
}

void VivaldiNativeAppWindowViews::Deactivate() {
  widget_->Deactivate();
}

void VivaldiNativeAppWindowViews::Maximize() {
  widget_->Maximize();
}

void VivaldiNativeAppWindowViews::Minimize() {
  widget_->Minimize();
}

void VivaldiNativeAppWindowViews::Restore() {
  widget_->Restore();
}

void VivaldiNativeAppWindowViews::SetBounds(const gfx::Rect& bounds) {
  widget_->SetBounds(bounds);
}

void VivaldiNativeAppWindowViews::FlashFrame(bool flash) {
  widget_->FlashFrame(flash);
}

gfx::NativeView VivaldiNativeAppWindowViews::GetNativeView() {
  return widget_->GetNativeView();
}

// WidgetDelegate implementation.

gfx::ImageSkia VivaldiNativeAppWindowViews::GetWindowAppIcon() {
  if (window_->browser()->is_type_popup()) {
    content::WebContents* web_contents =
        window_->browser()->tab_strip_model()->GetActiveWebContents();
    if (web_contents) {
      favicon::FaviconDriver* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents);
      gfx::Image app_icon = favicon_driver->GetFavicon();
      if (!app_icon.IsEmpty())
        return *app_icon.ToImageSkia();
    }
  }

  if (icon_family_.empty()) {
    return gfx::ImageSkia();
  }
  const gfx::Image* img =
      icon_family_.GetBest(kLargeIconSizeViv, kLargeIconSizeViv);
  return img ? *img->ToImageSkia() : gfx::ImageSkia();
}

gfx::ImageSkia VivaldiNativeAppWindowViews::GetWindowIcon() {
  if (icon_family_.empty()) {
    return gfx::ImageSkia();
  }
  const gfx::Image* img = icon_family_.GetBest(kSmallIconSizeViv,
                                               kSmallIconSizeViv);
  return img ? *img->ToImageSkia() : gfx::ImageSkia();
}

void VivaldiNativeAppWindowViews::SetIconFamily(gfx::ImageFamily images) {
  icon_family_ = std::move(images);
  widget_->UpdateWindowIcon();
}

views::ClientView*
VivaldiNativeAppWindowViews::CreateClientView(views::Widget* widget) {
  return new VivaldiWindowClientView(widget, GetContentsView(), window());
}

std::string VivaldiNativeAppWindowViews::GetWindowName() const {
  return chrome::GetWindowName(window_->browser());
}

bool VivaldiNativeAppWindowViews::WidgetHasHitTestMask() const {
  return false;
}

void VivaldiNativeAppWindowViews::GetWidgetHitTestMask(SkPath* mask) const {
  NOTREACHED();
}

void VivaldiNativeAppWindowViews::OnWidgetMove() {
  window_->OnNativeWindowChanged(true);
}

views::View* VivaldiNativeAppWindowViews::GetInitiallyFocusedView() {
  return web_view_;
}

bool VivaldiNativeAppWindowViews::CanResize() const {
  return true;
}

bool VivaldiNativeAppWindowViews::CanMaximize() const {
  return true;
}

bool VivaldiNativeAppWindowViews::CanMinimize() const {
  return true;
}

base::string16 VivaldiNativeAppWindowViews::GetWindowTitle() const {
  return window_->GetTitle();
}

bool VivaldiNativeAppWindowViews::ShouldShowWindowTitle() const {
  return true;
}

void VivaldiNativeAppWindowViews::SaveWindowPlacement(
    const gfx::Rect& bounds,
    ui::WindowShowState show_state) {
  if (window_->browser() &&
      chrome::ShouldSaveWindowPlacement(window_->browser()) &&
      // If IsFullscreen() is true, we've just changed into fullscreen mode,
      // and we're catching the going-into-fullscreen sizing and positioning
      // calls, which we want to ignore.
      !IsFullscreen() &&
      // VB-35145: Don't save placement after Hide() in
      // VivaldiBrowserWindow::ConfirmWindowClose() unmaximizes.
      !window_->is_hidden()) {
    WidgetDelegate::SaveWindowPlacement(bounds, show_state);
    chrome::SaveWindowPlacement(window_->browser(), bounds, show_state);
  }
  window_->OnNativeWindowChanged();
}

bool VivaldiNativeAppWindowViews::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  chrome::GetSavedWindowBoundsAndShowState(window_->browser(), bounds, show_state);

  if (chrome::SavedBoundsAreContentBounds(window_->browser())) {
    // This is normal non-app popup window. The value passed in |bounds|
    // represents two pieces of information:
    // - the position of the window, in screen coordinates (outer position).
    // - the size of the content area (inner size).
    // We need to use these values to determine the appropriate size and
    // position of the resulting window.
    gfx::Rect window_rect = GetWidget()->non_client_view()->
      GetWindowBoundsForClientBounds(*bounds);
    window_rect.set_origin(bounds->origin());

    // When we are given x/y coordinates of 0 on a created popup window,
    // assume none were given by the window.open() command.
    if (window_rect.x() == 0 && window_rect.y() == 0) {
      gfx::Size size = window_rect.size();
      window_rect.set_origin(WindowSizer::GetDefaultPopupOrigin(size));
    }
    *bounds = window_rect;
    *show_state = ui::SHOW_STATE_NORMAL;
  }
  // We return true because we can _always_ locate reasonable bounds using the
  // WindowSizer, and we don't want to trigger the Window's built-in "size to
  // default" handling because the browser window has no default preferred
  // size.
  return true;
}

void VivaldiNativeAppWindowViews::DeleteDelegate() {
  if (widget_) {
    widget_->RemoveObserver(this);
  }
  window_->OnNativeClose();
}

bool VivaldiNativeAppWindowViews::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
#if defined(USE_AURA)
  if (child->Contains(web_view_->web_contents()->GetNativeView())) {
    // App window should claim mouse events that fall within the draggable
    // region.
    return !draggable_region_.get() ||
           !draggable_region_->contains(location.x(), location.y());
  }
#endif

  return true;
}

bool VivaldiNativeAppWindowViews::ExecuteWindowsCommand(int command_id) {
#if defined(OS_WIN)
  // Note: All these commands are context relative to active webview.
  switch (command_id) {
    case APPCOMMAND_BROWSER_BACKWARD:
      HandleKeyboardCode(ui::VKEY_BROWSER_BACK);
      return true;
    case APPCOMMAND_BROWSER_FORWARD:
      HandleKeyboardCode(ui::VKEY_BROWSER_FORWARD);
      return true;
    default:
      break;
  }
#endif  // OS_WIN
  return false;
}

void VivaldiNativeAppWindowViews::HandleKeyboardCode(ui::KeyboardCode code) {
  Browser* browser = window()->browser();
  if (!browser)
    return;
  extensions::WebViewGuest *current_webviewguest =
      vivaldi::ui_tools::GetActiveWebGuestFromBrowser(browser);
  if (current_webviewguest) {
    content::NativeWebKeyboardEvent synth_event(
        blink::WebInputEvent::Type::kRawKeyDown,
        blink::WebInputEvent::kNoModifiers,
        ui::EventTimeForNow());
    synth_event.windows_key_code = code;
    current_webviewguest->web_contents()->GetDelegate()
        ->HandleKeyboardEvent(web_view_->GetWebContents(), synth_event);
  }
}

// WidgetObserver implementation.

void VivaldiNativeAppWindowViews::OnWidgetDestroying(views::Widget* widget) {
  for (auto& observer : modal_dialog_host_.observers_) {
    observer.OnHostDestroying();
  }
}

void VivaldiNativeAppWindowViews::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  window_->OnNativeWindowChanged();
}

void VivaldiNativeAppWindowViews::OnWidgetActivationChanged(
    views::Widget* widget, bool active) {
  window_->OnNativeWindowChanged();
  window_->OnNativeWindowActivationChanged(active);
  Browser* browser = window_->browser();
  if (!active && browser) {
    BrowserList::NotifyBrowserNoLongerActive(browser);
  }
}

void VivaldiNativeAppWindowViews::OnWidgetDestroyed(views::Widget* widget) {
  if (widget == widget_) {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }
}

// views::View implementation.

void VivaldiNativeAppWindowViews::Layout() {
  DCHECK(web_view_);
  web_view_->SetBounds(0, 0, width(), height());
  OnViewWasResized();
}

void VivaldiNativeAppWindowViews::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    web_view_ = new views::WebView(NULL);
    AddChildView(web_view_);
    web_view_->SetWebContents(window_->web_contents());
  }
}

gfx::Size VivaldiNativeAppWindowViews::GetMinimumSize() const {
  return minimum_size_;
}

gfx::Size VivaldiNativeAppWindowViews::GetMaximumSize() const {
  return gfx::Size();
}

void VivaldiNativeAppWindowViews::OnFocus() {
  web_view_->RequestFocus();
}

// NativeAppWindow implementation.

void VivaldiNativeAppWindowViews::SetFullscreen(bool is_fullscreen) {
  widget()->SetFullscreen(is_fullscreen);
}

bool VivaldiNativeAppWindowViews::IsFullscreenOrPending() const {
  return widget()->IsFullscreen();
}

void VivaldiNativeAppWindowViews::UpdateWindowIcon() {
  widget_->UpdateWindowIcon();
}

void VivaldiNativeAppWindowViews::UpdateWindowTitle() {
  widget_->UpdateWindowTitle();
}

void VivaldiNativeAppWindowViews::UpdateDraggableRegions(
    const std::vector<extensions::DraggableRegion>& regions) {
  // Draggable region is not supported for non-frameless window.
  if (!frameless_)
    return;

  // This is based on RawDraggableRegionsToSkRegion from
  // chromium/extensions/browser/app_window/app_window.cc
  draggable_region_ = std::make_unique<SkRegion>();
  for (auto iter = regions.cbegin(); iter != regions.cend(); ++iter) {
    const extensions::DraggableRegion& region = *iter;
    draggable_region_->op(
        SkIRect::MakeLTRB(region.bounds.x(), region.bounds.y(),
                          region.bounds.right(), region.bounds.bottom()),
        region.draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }

  OnViewWasResized();
}

gfx::Insets VivaldiNativeAppWindowViews::GetFrameInsets() const {
  if (frameless_ || !widget_ )
    return gfx::Insets();

  // The pretend client_bounds passed in need to be large enough to ensure that
  // GetWindowBoundsForClientBounds() doesn't decide that it needs more than
  // the specified amount of space to fit the window controls in, and return a
  // number larger than the real frame insets. Most window controls are smaller
  // than 1000x1000px, so this should be big enough.
  gfx::Rect client_bounds = gfx::Rect(1000, 1000);
  gfx::Rect window_bounds =
      widget_->non_client_view()->GetWindowBoundsForClientBounds(client_bounds);
  return window_bounds.InsetsFrom(client_bounds);
}

bool VivaldiNativeAppWindowViews::CanHaveAlphaEnabled() const {
  return widget_->IsTranslucentWindowOpacitySupported();
}

void VivaldiNativeAppWindowViews::SetVisibleOnAllWorkspaces(
    bool always_visible) {
  widget_->SetVisibleOnAllWorkspaces(always_visible);
}

void VivaldiNativeAppWindowViews::SetActivateOnPointer(
    bool activate_on_pointer) {}

void VivaldiNativeAppWindowViews::Close() {
  extensions::DevtoolsConnectorAPI::CloseDevtoolsForBrowser(
      window_->GetProfile(), window_->browser());

#if defined(OS_WIN)
  // This must be as early as possible.
  bool should_quit_if_last_browser =
    browser_shutdown::IsTryingToQuit() ||
    KeepAliveRegistry::GetInstance()->IsKeepingAliveOnlyByBrowserOrigin();
  if (should_quit_if_last_browser) {
    vivaldi::OnShutdownStarted();
  }
#endif  // defined(OS_WIN)
  if (widget_) {
    widget_->Close();
  }
}

void VivaldiNativeAppWindowViews::ShowEmojiPanel() {
  GetWidget()->ShowEmojiPanel();
}

ui::ZOrderLevel VivaldiNativeAppWindowViews::GetZOrderLevel() const {
  return GetWidget()->GetZOrderLevel();
}
void VivaldiNativeAppWindowViews::SetZOrderLevel(ui::ZOrderLevel order) {
  GetWidget()->SetZOrderLevel(order);
}

// ModalDialogHost methods

VivaldiNativeAppWindowViews::ModalDialogHost::ModalDialogHost(
    VivaldiNativeAppWindowViews* views)
    : views_(views) {}

VivaldiNativeAppWindowViews::ModalDialogHost::~ModalDialogHost() = default;

gfx::NativeView VivaldiNativeAppWindowViews::ModalDialogHost::GetHostView()
    const {
  if (!views_->widget())
    return nullptr;
  return views_->widget()->GetNativeView();
}

gfx::Point VivaldiNativeAppWindowViews::ModalDialogHost::GetDialogPosition(
    const gfx::Size& size) {
  if (!views_->widget())
    return gfx::Point();
  gfx::Size app_window_size =
      views_->widget()->GetWindowBoundsInScreen().size();
  return gfx::Point(app_window_size.width() / 2 - size.width() / 2,
                    app_window_size.height() / 2 - size.height() / 2);
}

gfx::Size VivaldiNativeAppWindowViews::ModalDialogHost::GetMaximumDialogSize() {
  if (!views_->widget())
    return gfx::Size();
  return views_->widget()->GetWindowBoundsInScreen().size();
}

void VivaldiNativeAppWindowViews::ModalDialogHost::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.AddObserver(observer);
}

void VivaldiNativeAppWindowViews::ModalDialogHost::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observers_.RemoveObserver(observer);
}