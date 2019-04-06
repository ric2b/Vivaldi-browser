// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "ui/vivaldi_native_app_window_views.h"

#include "base/command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/common/chrome_switches.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/draggable_region.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/path.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_ui_utils.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

#if defined(OS_WIN)
#include "browser/win/vivaldi_utils.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#endif  // defined(OS_WIN)

using extensions::AppWindow;

namespace {

const int kMinPanelWidth = 100;
const int kMinPanelHeight = 100;
const int kDefaultPanelWidth = 200;
const int kDefaultPanelHeight = 300;

const int kLargeIconSize = 256;
const int kSmallIconSize = 16;

struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};

// NOTE(daniel@vivaldi): Vivaldi handles ctrl+w and ctrl+shift+w by itself
const AcceleratorMapping kAppWindowAcceleratorMap[] = {
#if !defined(VIVALDI_BUILD)
  { ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
  { ui::VKEY_W, ui::EF_CONTROL_DOWN, IDC_CLOSE_WINDOW },
#endif
  { ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN, IDC_TASK_MANAGER },
  { ui::VKEY_F4, ui::EF_ALT_DOWN, IDC_CLOSE_WINDOW},
};

// These accelerators will only be available in kiosk mode. These allow the
// user to manually zoom app windows. This is only necessary in kiosk mode
// (in normal mode, the user can zoom via the screen magnifier).
// TODO(xiyuan): Write a test for kiosk accelerators.
const AcceleratorMapping kAppWindowKioskAppModeAcceleratorMap[] = {
  { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN,
    IDC_ZOOM_MINUS },
  { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, IDC_ZOOM_MINUS },
  { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, IDC_ZOOM_PLUS },
  { ui::VKEY_0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
  { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, IDC_ZOOM_NORMAL },
};

void AddAcceleratorsFromMapping(const AcceleratorMapping mapping[],
                                size_t mapping_length,
                                std::map<ui::Accelerator, int>* accelerators) {
  for (size_t i = 0; i < mapping_length; ++i) {
    ui::Accelerator accelerator(mapping[i].keycode, mapping[i].modifiers);
    (*accelerators)[accelerator] = mapping[i].command_id;
  }
}

const std::map<ui::Accelerator, int>& GetAcceleratorTable() {
  typedef std::map<ui::Accelerator, int> AcceleratorMap;
  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, accelerators, ());
  if (!chrome::IsRunningInForcedAppMode()) {
    if (accelerators.empty()) {
      AddAcceleratorsFromMapping(
          kAppWindowAcceleratorMap,
          arraysize(kAppWindowAcceleratorMap),
          &accelerators);
    }
    return accelerators;
  }

  CR_DEFINE_STATIC_LOCAL(AcceleratorMap, app_mode_accelerators, ());
  if (app_mode_accelerators.empty()) {
    AddAcceleratorsFromMapping(
        kAppWindowAcceleratorMap,
        arraysize(kAppWindowAcceleratorMap),
        &app_mode_accelerators);
    AddAcceleratorsFromMapping(
        kAppWindowKioskAppModeAcceleratorMap,
        arraysize(kAppWindowKioskAppModeAcceleratorMap),
        &app_mode_accelerators);
  }
  return app_mode_accelerators;
}

}  // namespace

VivaldiNativeAppWindowViews::VivaldiNativeAppWindowViews()
    : has_frame_color_(false),
      active_frame_color_(SK_ColorBLACK),
      inactive_frame_color_(SK_ColorBLACK),
      window_(NULL),
      web_view_(NULL),
      widget_(NULL),
      frameless_(false),
      resizable_(false),
      weak_ptr_factory_(this) {
  }

VivaldiNativeAppWindowViews::~VivaldiNativeAppWindowViews() {
  web_view_->SetWebContents(NULL);
}

void VivaldiNativeAppWindowViews::OnBeforeWidgetInit(
  const AppWindow::CreateParams& create_params,
  views::Widget::InitParams* init_params,
  views::Widget* widget) {
}

void VivaldiNativeAppWindowViews::OnBeforePanelWidgetInit(
  views::Widget::InitParams* init_params,
  views::Widget* widget) {
}

void VivaldiNativeAppWindowViews::InitializeDefaultWindow(
  const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);

  init_params.delegate = this;
  init_params.remove_standard_frame = IsFrameless() || has_frame_color_;
  init_params.use_system_default_icon = false;
  if (create_params.alpha_enabled) {
    init_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;

    // The given window is most likely not rectangular since it uses
    // transparency and has no standard frame, don't show a shadow for it.
    // TODO(skuhne): If we run into an application which should have a shadow
    // but does not have, a new attribute has to be added.
    if (IsFrameless())
      init_params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  }
  init_params.keep_on_top = create_params.always_on_top;
  init_params.visible_on_all_workspaces =
    create_params.visible_on_all_workspaces;

  init_params.thumbnail_window = create_params.thumbnail_window;

  OnBeforeWidgetInit(create_params, &init_params, widget());
  widget()->Init(init_params);

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  widget()->SetNativeWindowProperty(Profile::kProfileKey,
                                    window_->browser()->profile());

  // Vivaldi: This will be used as window state for the first Show()
  widget()->SetSavedShowState(create_params.state);

  // The frame insets are required to resolve the bounds specifications
  // correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();
  gfx::Rect window_bounds = create_params.GetInitialWindowBounds(frame_insets);
  SetContentSizeConstraints(create_params.GetContentMinimumSize(frame_insets),
                            create_params.GetContentMaximumSize(frame_insets));
  if (!window_bounds.IsEmpty()) {
    using BoundsSpecification = AppWindow::BoundsSpecification;
    bool position_specified =
      window_bounds.x() != BoundsSpecification::kUnspecifiedPosition &&
      window_bounds.y() != BoundsSpecification::kUnspecifiedPosition;
    if (!position_specified)
      widget()->CenterWindow(window_bounds.size());
    else
      widget()->SetBounds(window_bounds);
  }

  // Register accelerators supported by app windows.
  // TODO(jeremya/stevenjb): should these be registered for panels too?
  views::FocusManager* focus_manager = GetFocusManager();
  const std::map<ui::Accelerator, int>& accelerator_table =
    GetAcceleratorTable();
  const bool is_kiosk_app_mode = chrome::IsRunningInForcedAppMode();

  // Ensures that kiosk mode accelerators are enabled when in kiosk mode (to be
  // future proof). This is needed because GetAcceleratorTable() uses a static
  // to store data and only checks kiosk mode once. If a platform app is
  // launched before kiosk mode starts, the kiosk accelerators will not be
  // registered. This CHECK catches the case.
  CHECK(!is_kiosk_app_mode ||
        accelerator_table.size() ==
        arraysize(kAppWindowAcceleratorMap) +
        arraysize(kAppWindowKioskAppModeAcceleratorMap));

  // Ensure there is a ZoomController in kiosk mode, otherwise the processing
  // of the accelerators will cause a crash. Note CHECK here because DCHECK
  // will not be noticed, as this could only be relevant on real hardware.
  CHECK(!is_kiosk_app_mode ||
        zoom::ZoomController::FromWebContents(web_view()->GetWebContents()));

  for (std::map<ui::Accelerator, int>::const_iterator iter =
       accelerator_table.begin();
       iter != accelerator_table.end(); ++iter) {
    if (is_kiosk_app_mode && !chrome::IsCommandAllowedInAppMode(iter->second))
      continue;

    focus_manager->RegisterAccelerator(
      iter->first, ui::AcceleratorManager::kNormalPriority, this);
  }
  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  const extensions::Extension *extension = window_->extension();
  const ExtensionIconSet& icon_set = extensions::IconsInfo::GetIcons(extension);

  for (const auto& iter : icon_set.map()) {
    extensions::ExtensionResource resource =
        extension->GetResource(iter.second);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
        resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
        gfx::Size(iter.first, iter.first), ui::SCALE_FACTOR_100P));
    }
  }
  extensions::ImageLoader* loader =
      extensions::ImageLoader::Get(window_->GetProfile());
  loader->LoadImageFamilyAsync(
      extension, info_list,
      base::Bind(&VivaldiNativeAppWindowViews::OnImagesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void VivaldiNativeAppWindowViews::InitializePanelWindow(
  const AppWindow::CreateParams& create_params) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_PANEL);
  params.delegate = this;

  gfx::Rect initial_window_bounds =
    create_params.GetInitialWindowBounds(gfx::Insets());
  gfx::Size preferred_size =
    gfx::Size(initial_window_bounds.width(), initial_window_bounds.height());
  if (preferred_size.width() == 0)
    preferred_size.set_width(kDefaultPanelWidth);
  else if (preferred_size.width() < kMinPanelWidth)
    preferred_size.set_width(kMinPanelWidth);

  if (preferred_size.height() == 0)
    preferred_size.set_height(kDefaultPanelHeight);
  else if (preferred_size.height() < kMinPanelHeight)
    preferred_size.set_height(kMinPanelHeight);
  SetPreferredSize(preferred_size);

  params.bounds = gfx::Rect(preferred_size);
  OnBeforePanelWidgetInit(&params, widget());
  widget()->Init(params);
  widget()->set_focus_on_creation(create_params.focused);
}

views::NonClientFrameView*
VivaldiNativeAppWindowViews::CreateStandardDesktopAppFrame() {
  return views::WidgetDelegateView::CreateNonClientFrameView(widget());
}

void VivaldiNativeAppWindowViews::Init(
    VivaldiBrowserWindow* window,
    const AppWindow::CreateParams& create_params) {
  window_ = window;
  frameless_ = create_params.frame == AppWindow::FRAME_NONE;
  resizable_ = create_params.resizable;
  size_constraints_.set_minimum_size(
      create_params.GetContentMinimumSize(gfx::Insets()));
  size_constraints_.set_maximum_size(
      create_params.GetContentMaximumSize(gfx::Insets()));
  Observe(window_->web_contents());

  widget_ = new views::Widget;
  widget_->AddObserver(this);
  InitializeWindow(window, create_params);

  OnViewWasResized();
}

void VivaldiNativeAppWindowViews::UpdateEventTargeterWithInset() {
#if !defined(OS_MACOSX)
  bool is_maximized = IsMaximized();
  aura::Window* window = widget()->GetNativeWindow();
  int resize_inside =
      is_maximized ? 0 : 5;  // frame->resize_inside_bounds_size();
  gfx::Insets inset(resize_inside, resize_inside, resize_inside, resize_inside);
  // Add the EasyResizeWindowTargeter on the window, not its root window. The
  // root window does not have a delegate, which is needed to handle the event
  // in Linux.
  std::unique_ptr<ui::EventTargeter> old_eventtarget =
    window->SetEventTargeter(std::unique_ptr<ui::EventTargeter>(
      new wm::EasyResizeWindowTargeter(window, inset, inset)));
  delete old_eventtarget.release();
#endif
}


void VivaldiNativeAppWindowViews::OnCanHaveAlphaEnabledChanged() {
  window_->OnNativeWindowChanged();
}

void VivaldiNativeAppWindowViews::InitializeWindow(
    VivaldiBrowserWindow* window,
    const AppWindow::CreateParams& create_params) {
  DCHECK(widget());
  has_frame_color_ = create_params.has_frame_color;
  active_frame_color_ = create_params.active_frame_color;
  inactive_frame_color_ = create_params.inactive_frame_color;

  InitializeDefaultWindow(create_params);

  extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
    Profile::FromBrowserContext(window->browser()->profile()),
    widget()->GetFocusManager(),
    extensions::ExtensionKeybindingRegistry::PLATFORM_APPS_ONLY,
    NULL));
}

// ui::BaseWindow implementation.

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
  return widget_->GetNativeWindow();
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
    widget_->SetSavedShowState(current_state);
    return;
  }

  if (shown_) {
    // Shown previously so we can change the saved state.
    widget_->SetSavedShowState(current_state);
  }
  widget_->Show();
  shown_ = true;
}

void VivaldiNativeAppWindowViews::ShowInactive() {
  if (widget_->IsVisible())
    return;

  widget_->ShowInactive();
}

void VivaldiNativeAppWindowViews::Hide() {
  widget_->Hide();
}

bool VivaldiNativeAppWindowViews::IsVisible() const {
  return widget_->IsVisible();
}

void VivaldiNativeAppWindowViews::Close() {
  // NOTE(pettern): This will abort the currently open thumbnail
  // generating windows, but if this is not the last window,
  // the rest will continue.  This is not ideal, but avoids
  // lingering processes until AppWindows are no longer used
  // for thumbnail generation. See VB-38712.
  extensions::VivaldiUtilitiesAPI* utils_api =
    extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(
      window_->GetProfile());
  DCHECK(utils_api);
  utils_api->CloseAllThumbnailWindows();

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

bool VivaldiNativeAppWindowViews::IsAlwaysOnTop() const {
  return widget_->IsAlwaysOnTop();
}

void VivaldiNativeAppWindowViews::SetAlwaysOnTop(bool always_on_top) {
  widget_->SetAlwaysOnTop(always_on_top);
}

gfx::NativeView VivaldiNativeAppWindowViews::GetHostView() const {
  return widget_->GetNativeView();
}

gfx::Point VivaldiNativeAppWindowViews::GetDialogPosition(
    const gfx::Size& size) {
  gfx::Size app_window_size = widget_->GetWindowBoundsInScreen().size();
  return gfx::Point(app_window_size.width() / 2 - size.width() / 2,
                    app_window_size.height() / 2 - size.height() / 2);
}

gfx::Size VivaldiNativeAppWindowViews::GetMaximumDialogSize() {
  return widget_->GetWindowBoundsInScreen().size();
}

void VivaldiNativeAppWindowViews::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.AddObserver(observer);
}

void VivaldiNativeAppWindowViews::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void VivaldiNativeAppWindowViews::OnViewWasResized() {
  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
}

// WidgetDelegate implementation.

gfx::ImageSkia VivaldiNativeAppWindowViews::GetWindowAppIcon() {
  if (icon_family_.empty()) {
    return gfx::ImageSkia();
  }
  const gfx::Image* img =
      icon_family_.GetBest(kLargeIconSize, kLargeIconSize);
  return img ? *img->ToImageSkia() : gfx::ImageSkia();
}

gfx::ImageSkia VivaldiNativeAppWindowViews::GetWindowIcon() {
  if (icon_family_.empty()) {
    return gfx::ImageSkia();
  }
  const gfx::Image* img = icon_family_.GetBest(kSmallIconSize,
                                               kSmallIconSize);
  return img ? *img->ToImageSkia() : gfx::ImageSkia();
}

void VivaldiNativeAppWindowViews::OnImagesLoaded(gfx::ImageFamily images) {
  icon_family_ = std::move(images);
  widget_->UpdateWindowIcon();
}

views::NonClientFrameView*
VivaldiNativeAppWindowViews::CreateNonClientFrameView(views::Widget* widget) {
  return (IsFrameless() || has_frame_color_) ? CreateNonStandardAppFrame()
                                             : CreateStandardDesktopAppFrame();
}

views::ClientView*
VivaldiNativeAppWindowViews::CreateClientView(views::Widget* widget) {
  return new VivaldiAppWindowClientView(widget, GetContentsView(), window());
}

std::string VivaldiNativeAppWindowViews::GetWindowName() const {
  return chrome::GetWindowName(window_->browser());
}

bool VivaldiNativeAppWindowViews::WidgetHasHitTestMask() const {
  return shape_ != NULL;
}

void VivaldiNativeAppWindowViews::GetWidgetHitTestMask(gfx::Path* mask) const {
  shape_->getBoundaryPath(mask);
}

void VivaldiNativeAppWindowViews::OnWindowBeginUserBoundsChange() {
  content::WebContents* web_contents = window_->GetActiveWebContents();
  if (!web_contents)
    return;
  web_contents->GetRenderViewHost()->NotifyMoveOrResizeStarted();
}

// Similar to |BrowserView::CanClose|
bool VivaldiAppWindowClientView::CanClose() {
  Browser *browser = browser_window_->browser();

  // This adds a quick hide code path to avoid VB-33480, but
  // must be removed when beforeunload dialogs are implemented.
  int count;
  if (browser->OkToCloseWithInProgressDownloads(&count) ==
      Browser::DOWNLOAD_CLOSE_OK) {
    browser_window_->Hide();
  }
  if (!browser->ShouldCloseWindow()) {
    return false;
  }
  bool fast_tab_closing_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFastUnload);

  if (!browser->tab_strip_model()->empty()) {
    browser_window_->Hide();
    browser->OnWindowClosing();
    if (fast_tab_closing_enabled)
      browser->tab_strip_model()->CloseAllTabs();
    return false;
  } else if (fast_tab_closing_enabled &&
             !browser->HasCompletedUnloadProcessing()) {
    browser_window_->Hide();
    return false;
  }
  return true;
}

void VivaldiNativeAppWindowViews::OnWidgetMove() {
  window_->OnNativeWindowChanged();
}

views::View* VivaldiNativeAppWindowViews::GetInitiallyFocusedView() {
  return web_view_;
}

bool VivaldiNativeAppWindowViews::CanResize() const {
  return resizable_ && !size_constraints_.HasFixedSize() &&
         !WidgetHasHitTestMask();
}

bool VivaldiNativeAppWindowViews::CanMaximize() const {
  return resizable_ && !size_constraints_.HasMaximumSize() &&
         !WidgetHasHitTestMask();
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

bool VivaldiNativeAppWindowViews::ShouldShowWindowIcon() const {
  return false;
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
      // VB-35145: Don't save placement after browser_window_->Hide() in
      // VivaldiAppWindowClientView::CanClose() unmaximizes.
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

views::Widget* VivaldiNativeAppWindowViews::GetWidget() {
  return widget_;
}

const views::Widget* VivaldiNativeAppWindowViews::GetWidget() const {
  return widget_;
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
  extensions::WebViewGuest *current_webviewguest =
      vivaldi::ui_tools::GetActiveWebViewGuest(this);
  if (current_webviewguest) {
    content::NativeWebKeyboardEvent synth_event(
        blink::WebInputEvent::kRawKeyDown, blink::WebInputEvent::kNoModifiers,
        ui::EventTimeForNow());
    synth_event.windows_key_code = code;
    current_webviewguest->web_contents()->GetDelegate()
        ->HandleKeyboardEvent(web_view_->GetWebContents(), synth_event);
  }
}

// WidgetObserver implementation.

void VivaldiNativeAppWindowViews::OnWidgetDestroying(views::Widget* widget) {
  for (auto& observer : observer_list_)
    observer.OnHostDestroying();

  extension_keybinding_registry_.reset(nullptr);
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
  if (!active) {
    BrowserList::NotifyBrowserNoLongerActive(browser);
  }
}

void VivaldiNativeAppWindowViews::OnWidgetDestroyed(views::Widget* widget) {
  if (widget == widget_) {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }
}

// WebContentsObserver implementation.
void VivaldiNativeAppWindowViews::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  content::RenderWidgetHostView* view =
    render_view_host->GetWidget()->GetView();
  DCHECK(view);
  if (window_->requested_alpha_enabled() && CanHaveAlphaEnabled()) {
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

void VivaldiNativeAppWindowViews::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  OnViewWasResized();
}

// views::View implementation.

bool VivaldiNativeAppWindowViews::AcceleratorPressed(
  const ui::Accelerator& accelerator) {
  const std::map<ui::Accelerator, int>& accelerator_table =
    GetAcceleratorTable();
  std::map<ui::Accelerator, int>::const_iterator iter =
    accelerator_table.find(accelerator);
  DCHECK(iter != accelerator_table.end());
  int command_id = iter->second;
  switch (command_id) {
  case IDC_CLOSE_WINDOW:
    window_->Close();
    return true;
  case IDC_TASK_MANAGER:
    chrome::OpenTaskManager(NULL);
    return true;

  case IDC_ZOOM_MINUS:
    zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                         content::PAGE_ZOOM_OUT);
    return true;
  case IDC_ZOOM_NORMAL:
    zoom::PageZoom::Zoom(web_view()->GetWebContents(),
                         content::PAGE_ZOOM_RESET);
    return true;
  case IDC_ZOOM_PLUS:
    zoom::PageZoom::Zoom(web_view()->GetWebContents(), content::PAGE_ZOOM_IN);
    return true;
  default:
    NOTREACHED() << "Unknown accelerator sent to app window.";
  }
  return true;
}

void VivaldiNativeAppWindowViews::Layout() {
  DCHECK(web_view_);
  web_view_->SetBounds(0, 0, width(), height());
  OnViewWasResized();
}

void VivaldiNativeAppWindowViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    web_view_ = new views::WebView(NULL);
    AddChildView(web_view_);
    web_view_->SetWebContents(window_->web_contents());
  }
}

gfx::Size VivaldiNativeAppWindowViews::GetMinimumSize() const {
  return size_constraints_.GetMinimumSize();
}

gfx::Size VivaldiNativeAppWindowViews::GetMaximumSize() const {
  return size_constraints_.GetMaximumSize();
}

void VivaldiNativeAppWindowViews::OnFocus() {
  web_view_->RequestFocus();
}

// NativeAppWindow implementation.

void VivaldiNativeAppWindowViews::SetFullscreen(int fullscreen_types) {
  // Stub implementation. See also ChromeNativeAppWindowViews.
  widget()->SetFullscreen(fullscreen_types != AppWindow::FULLSCREEN_TYPE_NONE);
}

bool VivaldiNativeAppWindowViews::IsFullscreenOrPending() const {
  return widget()->IsFullscreen();
}

void VivaldiNativeAppWindowViews::UpdateShape(
    std::unique_ptr<ShapeRects> rects) {
  shape_rects_ = std::move(rects);

  // Build a region from the list of rects when it is supplied.
  std::unique_ptr<SkRegion> region;
  if (shape_rects_) {
    region = std::make_unique<SkRegion>();
    for (const gfx::Rect& input_rect : *shape_rects_.get())
      region->op(gfx::RectToSkIRect(input_rect), SkRegion::kUnion_Op);
  }
  widget()->SetShape(shape() ? std::make_unique<ShapeRects>(*shape_rects_)
                             : nullptr);
  widget()->OnSizeConstraintsChanged();
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

  draggable_region_.reset(AppWindow::RawDraggableRegionsToSkRegion(regions));
  OnViewWasResized();
}

SkRegion* VivaldiNativeAppWindowViews::GetDraggableRegion() {
  return draggable_region_.get();
}

void VivaldiNativeAppWindowViews::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                        GetFocusManager());
}

bool VivaldiNativeAppWindowViews::IsFrameless() const {
  return frameless_;
}

bool VivaldiNativeAppWindowViews::HasFrameColor() const {
  return has_frame_color_;
}

SkColor VivaldiNativeAppWindowViews::ActiveFrameColor() const {
  return active_frame_color_;
}

SkColor VivaldiNativeAppWindowViews::InactiveFrameColor() const {
  return inactive_frame_color_;
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

void VivaldiNativeAppWindowViews::HideWithApp() {
}

void VivaldiNativeAppWindowViews::ShowWithApp() {
}

gfx::Size VivaldiNativeAppWindowViews::GetContentMinimumSize() const {
  return size_constraints_.GetMinimumSize();
}

gfx::Size VivaldiNativeAppWindowViews::GetContentMaximumSize() const {
  return size_constraints_.GetMaximumSize();
}

void VivaldiNativeAppWindowViews::SetContentSizeConstraints(
    const gfx::Size& min_size,
    const gfx::Size& max_size) {
  size_constraints_.set_minimum_size(min_size);
  size_constraints_.set_maximum_size(max_size);
  widget_->OnSizeConstraintsChanged();
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
