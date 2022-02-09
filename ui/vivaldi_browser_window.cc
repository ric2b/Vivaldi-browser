// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_browser_window.h"

#include <memory>
#include <string>
#include <utility>

#include "app/vivaldi_constants.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "browser/menus/vivaldi_menus.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/printing_init.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/sharing/sharing_dialog_data.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/color_chooser.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/site_instance.h"
#include "extensions/api/events/vivaldi_ui_events.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/mojom/app_window.mojom.h"
#include "extensions/helper/vivaldi_app_helper.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/schema/vivaldi_utilities.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "renderer/vivaldi_render_messages.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "ui/devtools/devtools_connector.h"
//#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/vivaldi_location_bar.h"
#include "ui/vivaldi_native_app_window_views.h"
#include "ui/vivaldi_quit_confirmation_dialog.h"
#include "ui/vivaldi_ui_utils.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#if defined(OS_WIN)
#include "chrome/browser/win/jumplist_factory.h"
#endif

VivaldiAutofillBubbleHandler::VivaldiAutofillBubbleHandler() {}
VivaldiAutofillBubbleHandler::~VivaldiAutofillBubbleHandler() {}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    autofill::SaveCardBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    autofill::LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    autofill::OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

autofill::SaveUPIBubble* VivaldiAutofillBubbleHandler::ShowSaveUPIBubble(
    content::WebContents* contents,
    autofill::SaveUPIBubbleController* controller) {
  return nullptr;
}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowSaveAddressProfileBubble(
    content::WebContents* web_contents,
    autofill::SaveUpdateAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowUpdateAddressProfileBubble(
    content::WebContents* web_contents,
    autofill::SaveUpdateAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowEditAddressProfileDialog(
    content::WebContents* web_contents,
    autofill::EditAddressProfileDialogController* controller) {
  return nullptr;
}

autofill::AutofillBubbleBase*
VivaldiAutofillBubbleHandler::ShowVirtualCardManualFallbackBubble(
    content::WebContents* web_contents,
    autofill::VirtualCardManualFallbackBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

namespace {
static base::TimeTicks g_first_window_creation_time;

std::unique_ptr<content::WebContents> CreateBrowserWebContents(
    Browser* browser,
    content::RenderFrameHost* creator_frame,
    const GURL& app_url) {
  Profile* profile = browser->profile();
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(profile, app_url);

  content::WebContents::CreateParams create_params(profile,
                                                   site_instance.get());
  int extension_process_id = site_instance->GetProcess()->GetID();
  if (creator_frame) {
    create_params.opener_render_process_id =
        creator_frame->GetProcess()->GetID();
    create_params.opener_render_frame_id = creator_frame->GetRoutingID();

    // All windows for the same profile should share the same process.
    DCHECK(create_params.opener_render_process_id == extension_process_id);
    if (create_params.opener_render_process_id != extension_process_id) {
      LOG(ERROR) << "VivaldiWindow WebContents will be created in the process ("
                 << extension_process_id << ") != creator ("
                 << create_params.opener_render_process_id
                 << "). Routing disabled.";
    }
  }
  LOG(INFO) << "VivaldiWindow WebContents will be created in the process "
            << extension_process_id
            << ", window_id=" << browser->session_id().id();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  blink::RendererPreferences* render_prefs =
      web_contents->GetMutableRendererPrefs();
  DCHECK(render_prefs);

  // We must update from system settings otherwise many settings would fallback
  // to default values when syncing below.  Guest views use these values from
  // the owner as default values in BrowserPluginGuest::InitInternal().
  renderer_preferences_util::UpdateFromSystemSettings(render_prefs, profile);

  web_contents->GetMutableRendererPrefs()
      ->browser_handles_all_top_level_requests = true;
  web_contents->SyncRendererPrefs();

  return web_contents;
}

extensions::vivaldi::window_private::WindowState ConvertFromWindowShowState(
    ui::WindowShowState state) {
  using extensions::vivaldi::window_private::WindowState;
  switch (state) {
    case ui::SHOW_STATE_FULLSCREEN:
      return WindowState::WINDOW_STATE_FULLSCREEN;
    case ui::SHOW_STATE_MAXIMIZED:
      return WindowState::WINDOW_STATE_MAXIMIZED;
    case ui::SHOW_STATE_MINIMIZED:
      return WindowState::WINDOW_STATE_MINIMIZED;
    default:
      return WindowState::WINDOW_STATE_NORMAL;
  }
}

}  // namespace

VivaldiBrowserWindowParams::VivaldiBrowserWindowParams() = default;
VivaldiBrowserWindowParams::~VivaldiBrowserWindowParams() = default;

// VivaldiBrowserWindow --------------------------------------------------------

VivaldiBrowserWindow::VivaldiBrowserWindow() {
  if (g_first_window_creation_time.is_null()) {
    g_first_window_creation_time = base::TimeTicks::Now();
  }
}

VivaldiBrowserWindow::~VivaldiBrowserWindow() {
  OnDidFinishNavigation(false);
}

// static
base::TimeTicks VivaldiBrowserWindow::GetFirstWindowCreationTime() {
  return g_first_window_creation_time;
}

// static
VivaldiBrowserWindow* VivaldiBrowserWindow::FromBrowser(
    const Browser* browser) {
  if (!browser || !browser->is_vivaldi())
    return nullptr;
  return static_cast<VivaldiBrowserWindow*>(browser->window());
}

// static
VivaldiBrowserWindow* VivaldiBrowserWindow::CreateVivaldiBrowserWindow(
    std::unique_ptr<Browser> browser) {
  DCHECK(browser);

  VivaldiBrowserWindowParams params;
  params.minimum_size = gfx::Size(500, 300);
  params.native_decorations = browser->profile()->GetPrefs()->GetBoolean(
      vivaldiprefs::kWindowsUseNativeDecoration);
  chrome::GetSavedWindowBoundsAndShowState(
      browser.get(), &params.content_bounds, &params.state);
  params.resource_relative_url = "browser.html";

  VivaldiBrowserWindow* window = new VivaldiBrowserWindow();
  window->CreateWebContents(std::move(browser), params);

  return window;
}

void VivaldiBrowserWindow::CreateWebContents(
    std::unique_ptr<Browser> browser,
    const VivaldiBrowserWindowParams& params) {
  DCHECK(browser);
  DCHECK(!browser_);
  DCHECK(!web_contents());
  // We should always be set as vivaldi in Browser.
  DCHECK(browser->is_vivaldi());
  DCHECK(!browser->window() || browser->window() == this);
  browser_ = std::move(browser);
  DCHECK_EQ(window_type_, NORMAL);
  if (params.settings_window) {
    window_type_ = SETTINGS;
  }
  location_bar_ = std::make_unique<VivaldiLocationBar>(*this);
#if defined(OS_WIN)
  JumpListFactory::GetForProfile(browser_->profile());
#endif
  DCHECK(!extension_);
  extension_ = const_cast<extensions::Extension*>(
      extensions::ExtensionRegistry::Get(browser_->profile())
          ->GetExtensionById(vivaldi::kVivaldiAppId,
                             extensions::ExtensionRegistry::EVERYTHING));
  DCHECK(extension_);

  GURL app_url = extension_->url();
  DCHECK(app_url.possibly_invalid_spec() == vivaldi::kVivaldiAppURLDomain);

  web_contents_ =
      CreateBrowserWebContents(browser_.get(), params.creator_frame, app_url);

  web_contents_delegate_.Initialize();

  extensions::SetViewType(web_contents(),
                          extensions::mojom::ViewType::kAppWindow);

  // The following lines are copied from ChromeAppDelegate::InitWebContents().
  favicon::CreateContentFaviconDriverForWebContents(web_contents());
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents());
  apps::AudioFocusWebContentsObserver::CreateForWebContents(web_contents());
  zoom::ZoomController::CreateForWebContents(web_contents());
  // end of lines copied from ChromeAppDelegate::InitWebContents().

  extensions::ExtensionWebContentsObserver::GetForWebContents(web_contents())
      ->dispatcher()
      ->set_delegate(this);

  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(),
      autofill::ChromeAutofillClient::FromWebContents(web_contents()));
  ManagePasswordsUIController::CreateForWebContents(web_contents());
  TabDialogs::CreateForWebContents(web_contents());

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents());

  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
      ->SetDelegate(this);

  extensions::VivaldiAppHelper::CreateForWebContents(web_contents());

  views_ = VivaldiNativeAppWindowViews::Create();
  views_->Init(this, params);

  views_->SetCanResize(browser_->create_params().can_resize);

  browser_->set_initial_show_state(params.state);

  // The infobar container must come after the toolbar so its arrow paints on
  // top.
  infobar_container_ = new vivaldi::InfoBarContainerWebProxy(this);

  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (const auto& iter : extensions::IconsInfo::GetIcons(extension()).map()) {
    extensions::ExtensionResource resource =
        extension()->GetResource(iter.second);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(iter.first, iter.first), ui::SCALE_FACTOR_100P));
    }
  }
  extensions::ImageLoader* loader = extensions::ImageLoader::Get(GetProfile());
  loader->LoadImageFamilyAsync(
      extension(), info_list,
      base::BindOnce(&VivaldiBrowserWindow::OnIconImagesLoaded,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(pettern): Crashes on shutdown, fix.
  // extensions::ExtensionRegistry::Get(browser_->profile())->AddObserver(this);

  // Ensure we force show the window after some time no matter what.
  // base::Unretained() is safe as this owns the timer.
  show_delay_timeout_ = std::make_unique<base::OneShotTimer>();
  show_delay_timeout_->Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(5000),
      base::BindOnce(&VivaldiBrowserWindow::ForceShow, base::Unretained(this)));

  GURL resource_url = extension_->GetResourceURL(params.resource_relative_url);
  web_contents()->GetController().LoadURL(resource_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());
}

void VivaldiBrowserWindow::OnIconImagesLoaded(gfx::ImageFamily image_family) {
  views_->SetIconFamily(std::move(image_family));
}

void VivaldiBrowserWindow::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (vivaldi::kVivaldiAppId == extension->id())
    views_->Close();
}

void VivaldiBrowserWindow::ContentsDidStartNavigation() {
  ::vivaldi::BroadcastEvent(extensions::vivaldi::window_private::
                                OnWebContentsDidStartNavigation::kEventName,
                            extensions::vivaldi::window_private::
                                OnWebContentsDidStartNavigation::Create(id()),
                            browser_->profile());
}

void VivaldiBrowserWindow::ForceShow() {
  show_delay_timeout_.reset();
  Show();
}

void VivaldiBrowserWindow::Show() {
#if !defined(OS_WIN)
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behavior under
  // Windows and Ash, but other platforms will not trigger
  // OnWidgetActivationChanged() until we return to the runloop. Therefore any
  // calls to Browser::GetLastActive() will return the wrong result if we do not
  // explicitly set it here.
  // A similar block also appears in BrowserWindowCocoa::Show().
  if (browser()) {
    BrowserList::SetLastActive(browser());
  }
#endif

  // We delay showing it until the UI document has loaded.
  if (show_delay_timeout_)
    return;

  if (has_been_shown_)
    return;

  has_been_shown_ = true;
  is_hidden_ = false;

  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::CHROME_APP_DELEGATE, KeepAliveRestartOption::DISABLED);

  ui::WindowShowState initial_show_state = browser_->initial_show_state();
  if (initial_show_state == ui::SHOW_STATE_FULLSCREEN)
    SetFullscreen(true);
  else if (initial_show_state == ui::SHOW_STATE_MAXIMIZED)
    Maximize();
  else if (initial_show_state == ui::SHOW_STATE_MINIMIZED)
    Minimize();

  views_->Show();

  OnNativeWindowChanged();
}

void VivaldiBrowserWindow::Hide() {
  is_hidden_ = true;
  views_->Hide();
  keep_alive_.reset();
}

bool VivaldiBrowserWindow::IsVisible() const {
  return views_->IsVisible();
}

void VivaldiBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  if (views_)
    views_->SetBounds(bounds);
  return;
}

void VivaldiBrowserWindow::Close() {
  MovePinnedTabsToOtherWindowIfNeeded();

  views_->Close();
}

void VivaldiBrowserWindow::MovePinnedTabsToOtherWindowIfNeeded() {
  Browser* candidate =
      ::vivaldi::ui_tools::FindBrowserForPinnedTabs(browser_.get());
  if (!candidate) {
    return;
  }
  std::vector<int> tabs_to_move;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (tab_strip_model->IsTabPinned(i)) {
      tabs_to_move.push_back(sessions::SessionTabHelper::IdForTab(
                                 tab_strip_model->GetWebContentsAt(i))
                                 .id());
    }
  }

  // Ensure that all tabs are added after the last pinned tab in the target
  // window.
  int new_index = 0;
  tab_strip_model = candidate->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (tab_strip_model->IsTabPinned(i)) {
      new_index = i + 1;
    }
  }

  // We increment the 'new_index' by one ourselves to get all moved pinned tabs
  // alongside to each other.
  int index = 0;
  for (size_t i = 0; i < tabs_to_move.size(); i++) {
    if (::vivaldi::ui_tools::GetTabById(tabs_to_move[i], nullptr, &index)) {
      if (!::vivaldi::ui_tools::MoveTabToWindow(browser_.get(), candidate,
                                                index, &new_index, 0)) {
        NOTREACHED();
        break;
      }
      new_index += 1;
    }
  }
}

// Similar to |BrowserView::CanClose|
bool VivaldiBrowserWindow::ConfirmWindowClose() {
#if !defined(OS_MAC)
  // Is window closing due to a profile being closed?
  bool closed_due_to_profile =
      extensions::VivaldiWindowsAPI::IsWindowClosingBecauseProfileClose(
          browser());

  int tabbed_windows_cnt = vivaldi::GetBrowserCountOfType(Browser::TYPE_NORMAL);
  const PrefService* prefs = GetProfile()->GetPrefs();
  // Don't show exit dialog if the user explicitly selected exit
  // from the menu.
  if (!browser_shutdown::IsTryingToQuit() && !GetProfile()->IsGuestSession()) {
    if (prefs->GetBoolean(vivaldiprefs::kSystemShowExitConfirmationDialog)) {
      if (!quit_dialog_shown_ && browser()->type() == Browser::TYPE_NORMAL &&
          tabbed_windows_cnt == 1) {
        if (IsMinimized()) {
          // Dialog is not visible if the window is minimized, so restore it
          // now.
          Restore();
        }
        bool quiting = true;
        new vivaldi::VivaldiQuitConfirmationDialog(
            base::BindOnce(&VivaldiBrowserWindow::ContinueClose,
                           weak_ptr_factory_.GetWeakPtr(), quiting),
            nullptr, GetNativeWindow(),
            new vivaldi::VivaldiDialogQuitDelegate());
        return false;
      }
    }
  }
  // If all tabs are gone there is no need to show a confirmation dialog. This
  // is most likely a window that has been the source window of a move-tab
  // operation.
  if (!browser()->tab_strip_model()->empty() &&
      !GetProfile()->IsGuestSession() && !closed_due_to_profile) {
    if (prefs->GetBoolean(
            vivaldiprefs::kWindowsShowWindowCloseConfirmationDialog)) {
      if (!close_dialog_shown_ && !quit_dialog_shown_ &&
          !browser_shutdown::IsTryingToQuit() &&
          browser()->type() == Browser::TYPE_NORMAL &&
          tabbed_windows_cnt >= 1) {
        if (IsMinimized()) {
          // Dialog is not visible if the window is minimized, so restore it
          // now.
          Restore();
        }
        bool quiting = false;
        new vivaldi::VivaldiQuitConfirmationDialog(
            base::BindOnce(&VivaldiBrowserWindow::ContinueClose,
                           weak_ptr_factory_.GetWeakPtr(), quiting),
            nullptr, GetNativeWindow(),
            new vivaldi::VivaldiDialogCloseWindowDelegate());
        return false;
      }
    }
  }
#endif  // !defined(OS_MAC)
  if (!browser()->ShouldCloseWindow())
    return false;

  // This adds a quick hide code path to avoid VB-33480
  int count;
  if (browser()->OkToCloseWithInProgressDownloads(&count) ==
      Browser::DownloadCloseType::kOk) {
    Hide();
  }
  if (!browser()->tab_strip_model()->empty()) {
    Hide();
    browser()->OnWindowClosing();
    return false;
  }
  return true;
}

void VivaldiBrowserWindow::ContinueClose(bool quiting,
                                         bool close,
                                         bool stop_asking) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (quiting) {
    prefs->SetBoolean(vivaldiprefs::kSystemShowExitConfirmationDialog,
                      !stop_asking);
    quit_dialog_shown_ = close;
  } else {
    prefs->SetBoolean(vivaldiprefs::kWindowsShowWindowCloseConfirmationDialog,
                      !stop_asking);
    close_dialog_shown_ = close;
  }

  if (close) {
    Close();
  } else {
    // Notify about the cancellation of window close so
    // events can be sent to the web ui.
    // content::NotificationService::current()->Notify(
    //  chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
    //  content::Source<Browser>(browser()),
    //  content::NotificationService::NoDetails());
  }
}

void VivaldiBrowserWindow::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    base::OnceCallback<void(bool)> callback) {
#if defined(OS_MAC)
  // We allow closing the window here since the real quit decision on Mac is
  // made in [AppController quit:].
  std::move(callback).Run(true);
#else
  DownloadInProgressDialogView::Show(GetNativeWindow(), download_count,
                                     dialog_type, std::move(callback));
#endif  // OS_MAC
}

void VivaldiBrowserWindow::Activate() {
  views_->Activate();
  if (browser()) {
    BrowserList::SetLastActive(browser());
  }
}

void VivaldiBrowserWindow::Deactivate() {}

bool VivaldiBrowserWindow::IsActive() const {
  return views_ ? views_->IsActive() : false;
}

gfx::NativeWindow VivaldiBrowserWindow::GetNativeWindow() const {
  return views_->GetNativeWindow();
}

StatusBubble* VivaldiBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect VivaldiBrowserWindow::GetRestoredBounds() const {
  if (views_) {
    return views_->GetRestoredBounds();
  }
  return gfx::Rect();
}

ui::WindowShowState VivaldiBrowserWindow::GetRestoredState() const {
  if (views_) {
    return views_->GetRestoredState();
  }
  return ui::SHOW_STATE_DEFAULT;
}

gfx::Rect VivaldiBrowserWindow::GetBounds() const {
  if (views_) {
    gfx::Rect bounds = views_->GetBounds();
    bounds.Inset(views_->GetFrameInsets());
    return bounds;
  }
  return gfx::Rect();
}

bool VivaldiBrowserWindow::IsMaximized() const {
  if (views_) {
    return views_->IsMaximized();
  }
  return false;
}

bool VivaldiBrowserWindow::IsMinimized() const {
  if (views_) {
    return views_->IsMinimized();
  }
  return false;
}

void VivaldiBrowserWindow::Maximize() {
  if (views_) {
    return views_->Maximize();
  }
}

void VivaldiBrowserWindow::Minimize() {
  if (views_) {
    return views_->Minimize();
  }
}

void VivaldiBrowserWindow::Restore() {
  if (IsFullscreen()) {
    views_->SetFullscreen(false);
  } else {
    views_->Restore();
  }
}

bool VivaldiBrowserWindow::ShouldHideUIForFullscreen() const {
  return IsFullscreen();
}

bool VivaldiBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

LocationBar* VivaldiBrowserWindow::GetLocationBar() const {
  return location_bar_.get();
}

void VivaldiBrowserWindow::UpdateToolbar(content::WebContents* contents) {
  UpdatePageActionIcon(PageActionIconType::kManagePasswords);
}

void VivaldiBrowserWindow::HandleMouseChange(bool motion) {
  if (last_motion_ != motion || motion == false) {
    extensions::VivaldiUIEvents::SendMouseChangeEvent(browser_->profile(),
                                                      motion);
  }
  last_motion_ = motion;
}

content::KeyboardEventProcessingResult
VivaldiBrowserWindow::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool VivaldiBrowserWindow::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  bool is_auto_repeat;
#if defined(OS_MAC)
  is_auto_repeat = event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat;
#else
  // Ideally we should do what we do above for Mac, but it is not 100% reliable
  // (at least on Linux). Try pressing F1 for a while and switch to F2. The
  // first auto repeat is not flagged as such.
  is_auto_repeat = false;
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    is_auto_repeat = event.windows_key_code == last_key_code_;
    last_key_code_ = event.windows_key_code;
  } else if (event.GetType() != blink::WebInputEvent::Type::kKeyDown &&
             event.GetType() != blink::WebInputEvent::Type::kChar) {
    last_key_code_ = 0;
  }
#endif  // defined(OS_MAC)

  extensions::VivaldiUIEvents::SendKeyboardShortcutEvent(browser_->profile(),
                                                         event, is_auto_repeat);

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, views_->GetFocusManager());
}

bool VivaldiBrowserWindow::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return vivaldi::GetFixedAcceleratorForCommandId(command_id, accelerator);
}

ui::AcceleratorProvider* VivaldiBrowserWindow::GetAcceleratorProvider() {
  return this;
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

bool VivaldiBrowserWindow::IsDownloadShelfVisible() const {
  return false;
}

DownloadShelf* VivaldiBrowserWindow::GetDownloadShelf() {
  return NULL;
}

// See comments on: BrowserWindow.VivaldiShowWebSiteSettingsAt.
void VivaldiBrowserWindow::VivaldiShowWebsiteSettingsAt(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    gfx::Point pos) {
#if defined(USE_AURA)
  PageInfoBubbleView::ShowPopupAtPos(pos, profile, web_contents, url,
                                     browser_.get(), GetNativeWindow());
#elif defined(OS_MAC)
  gfx::Rect anchor_rect = gfx::Rect(pos, gfx::Size());
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          nullptr, anchor_rect, GetNativeWindow(), profile, web_contents, url,
          // Use a simple lambda for the callback. We do not do anything there.
          base::BindOnce([](views::Widget::ClosedReason closed_reason,
                            bool reload_prompt) {}));
  bubble->GetWidget()->Show();
#endif
}

std::unique_ptr<FindBar> VivaldiBrowserWindow::CreateFindBar() {
  return std::unique_ptr<FindBar>();
}

ExclusiveAccessContext* VivaldiBrowserWindow::GetExclusiveAccessContext() {
  return this;
}

void VivaldiBrowserWindow::DestroyBrowser() {
  // TODO(pettern): Crashes on shutdown, fix.
  //  extensions::ExtensionRegistry::Get(browser_->profile())->RemoveObserver(this);
  browser_.reset();
}

gfx::Size VivaldiBrowserWindow::GetContentsSize() const {
  if (views_) {
    // TODO(pettern): This is likely not correct, should be tab contents.
    gfx::Rect bounds = views_->GetBounds();
    bounds.Inset(views_->GetFrameInsets());
    return bounds.size();
  }
  return gfx::Size();
}

void VivaldiBrowserWindow::ShowEmojiPanel() {
  views_->ShowEmojiPanel();
}

std::string VivaldiBrowserWindow::GetWorkspace() const {
  return std::string();
}

bool VivaldiBrowserWindow::IsVisibleOnAllWorkspaces() const {
  return false;
}

Profile* VivaldiBrowserWindow::GetProfile() {
  return browser_->profile();
}

void VivaldiBrowserWindow::UpdateExclusiveAccessExitBubbleContent(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
    bool force_update) {}

void VivaldiBrowserWindow::OnExclusiveAccessUserInput() {}

content::WebContents* VivaldiBrowserWindow::GetActiveWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

ShowTranslateBubbleResult VivaldiBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID;
}

void VivaldiBrowserWindow::UpdateDevTools() {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();

  // Get the docking state.
  const base::DictionaryValue* prefs =
      browser_->profile()->GetPrefs()->GetDictionary(
          prefs::kDevToolsPreferences);

  std::string docking_state;
  std::string device_mode;

  // DevToolsWindow code has already activated the tab.
  content::WebContents* contents = tab_strip_model->GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
  extensions::DevtoolsConnectorAPI* api =
      extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
          browser_->profile());
  DCHECK(api);
  scoped_refptr<extensions::DevtoolsConnectorItem> item =
      base::WrapRefCounted<extensions::DevtoolsConnectorItem>(
          api->GetOrCreateDevtoolsConnectorItem(tab_id));

  // Iterate the list of inspected tabs and send events if any is
  // in the process of closing.
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* inspected_contents =
        tab_strip_model->GetWebContentsAt(i);
    DevToolsWindow* w =
        DevToolsWindow::GetInstanceForInspectedWebContents(inspected_contents);
    if (w && w->IsClosing()) {
      int id = sessions::SessionTabHelper::IdForTab(inspected_contents).id();
      extensions::DevtoolsConnectorAPI::SendClosed(browser_->profile(), id);
      ResetDockingState(id);
    }
  }
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);

  if (window) {
    // We handle the closing devtools windows above.
    if (!window->IsClosing()) {
      if (prefs->GetString("currentDockState", &docking_state)) {
        // Strip quotation marks from the state.
        base::ReplaceChars(docking_state, "\"", "", &docking_state);
        if (item->docking_state() != docking_state) {
          item->set_docking_state(docking_state);

          extensions::DevtoolsConnectorAPI::SendDockingStateChanged(
              browser_->profile(), tab_id, docking_state);
        }
      }
      if (prefs->GetString("showDeviceMode", &device_mode)) {
        base::ReplaceChars(device_mode, "\"", "", &device_mode);
        bool device_mode_enabled = device_mode == "true";
        if (item->device_mode_enabled() == device_mode_enabled) {
          item->set_device_mode_enabled(device_mode_enabled);
        }
      }
    }
  }
}

void VivaldiBrowserWindow::ResetDockingState(int tab_id) {
  extensions::DevtoolsConnectorAPI* api =
      extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
          browser_->profile());
  DCHECK(api);
  scoped_refptr<extensions::DevtoolsConnectorItem> item =
      base::WrapRefCounted<extensions::DevtoolsConnectorItem>(
          api->GetOrCreateDevtoolsConnectorItem(tab_id));

  item->ResetDockingState();

  extensions::DevtoolsConnectorAPI::SendDockingStateChanged(
      browser_->profile(), tab_id, item->docking_state());
}

bool VivaldiBrowserWindow::IsToolbarShowing() const {
  return false;
}

views::View* VivaldiBrowserWindow::GetContentsView() const {
  return views_.get();
}

gfx::NativeView VivaldiBrowserWindow::GetNativeView() {
  return views_->GetNativeView();
}

views::View* VivaldiBrowserWindow::GetBubbleDialogAnchor() const {
  return views_->web_view();
}

void VivaldiBrowserWindow::OnNativeWindowChanged(bool moved) {
  // This may be called during Init before |views_| is set.
  if (!views_)
    return;

  if (views_) {
    views_->UpdateEventTargeterWithInset();
  }

  WindowStateData old_state = window_state_data_;
  WindowStateData new_state;
  if (views_->IsFullscreenOrPending()) {
    new_state.state = ui::SHOW_STATE_FULLSCREEN;
  } else if (views_->IsMaximized()) {
    new_state.state = ui::SHOW_STATE_MAXIMIZED;
  } else if (views_->IsMinimized()) {
    new_state.state = ui::SHOW_STATE_MINIMIZED;
  } else {
    new_state.state = ui::SHOW_STATE_NORMAL;
  }
  new_state.bounds = views_->GetBounds();

  // Call the delegate so it can dispatch events to the js side
  // for any change in state.
  if (old_state.state != new_state.state) {
    OnStateChanged(new_state.state);
  }

  if (old_state.bounds.x() != new_state.bounds.x() ||
      old_state.bounds.y() != new_state.bounds.y()) {
    // We only send an event when the position of the window changes.
    OnPositionChanged();
  }

  window_state_data_ = new_state;
}

void VivaldiBrowserWindow::OnNativeClose() {
  web_modal::WebContentsModalDialogManager* modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents());
  if (modal_dialog_manager) {
    modal_dialog_manager->SetDelegate(nullptr);
  }
  web_contents_.reset();
  show_delay_timeout_.reset();

  // For a while we used a direct "delete this" here. That causes the browser_
  // object to be destoyed immediately and that will kill the private profile.
  // The missing profile will (very often) trigger a crash on Linux. VB-34358
  // TODO(all): There is missing cleanup in chromium code. See VB-34358.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&VivaldiBrowserWindow::DeleteThis,
                                     base::Unretained(this)));
}

void VivaldiBrowserWindow::DeleteThis() {
  delete this;
}

void VivaldiBrowserWindow::OnNativeWindowActivationChanged(bool active) {
  UpdateActivation(active);
  if (active && browser()) {
    BrowserList::SetLastActive(browser());
  }
}

void VivaldiBrowserWindow::UpdateActivation(bool is_active) {
  if (is_active_ != is_active) {
    is_active_ = is_active;
    OnActivationChanged(is_active_);
  }
}

void VivaldiBrowserWindow::UpdateTitleBar() {
  views_->UpdateWindowTitle();
  views_->UpdateWindowIcon();
}

std::u16string VivaldiBrowserWindow::GetTitle() {
  if (!extension_)
    return std::u16string();

  // WebContents::GetTitle() will return the page's URL if there's no <title>
  // specified. However, we'd prefer to show the name of the extension in that
  // case, so we directly inspect the NavigationEntry's title.
  std::u16string title;
  content::NavigationEntry* entry =
      web_contents() ? web_contents()->GetController().GetLastCommittedEntry()
                     : nullptr;
  if (!entry || entry->GetTitle().empty()) {
    title = base::UTF8ToUTF16(extension_->name());
  } else {
    title = web_contents()->GetTitle();
  }
  title += u" - Vivaldi";
  base::RemoveChars(title, u"\n", &title);
  return title;
}

void VivaldiBrowserWindow::OnActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  UpdateTitleBar();

  infobar_container_->ChangeInfoBarManager(
      infobars::ContentInfoBarManager::FromWebContents(new_contents));
}

void VivaldiBrowserWindow::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  // The implementation is copied from
  // ChromeAppDelegate::SetWebContentsBlocked().
  if (!blocked)
    web_contents->Focus();
  // RenderViewHost may be NULL during shutdown.
  content::RenderFrameHost* host = web_contents->GetMainFrame();
  if (host) {
    mojo::Remote<extensions::mojom::AppWindow> app_window;
    host->GetRemoteInterfaces()->GetInterface(
        app_window.BindNewPipeAndPassReceiver());
    app_window->SetVisuallyDeemphasized(blocked);
  }
}

bool VivaldiBrowserWindow::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

web_modal::WebContentsModalDialogHost*
VivaldiBrowserWindow::GetWebContentsModalDialogHost() {
  return &views_->modal_dialog_host_;
}

void VivaldiBrowserWindow::EnterFullscreen(
    const GURL& url,
    ExclusiveAccessBubbleType bubble_type,
    int64_t display_id) {
  SetFullscreen(true);
}

void VivaldiBrowserWindow::ExitFullscreen() {
  SetFullscreen(false);
}

void VivaldiBrowserWindow::SetFullscreen(bool enable) {
  views_->SetFullscreen(enable);
}

bool VivaldiBrowserWindow::IsFullscreen() const {
  return views_ ? views_->IsFullscreen() : false;
}

extensions::WindowController*
VivaldiBrowserWindow::GetExtensionWindowController() const {
  return nullptr;
}

content::WebContents* VivaldiBrowserWindow::GetAssociatedWebContents() const {
  return web_contents();
}

// infobars::InfoBarContainer::Delegate, none of these methods are used by us.
void VivaldiBrowserWindow::InfoBarContainerStateChanged(bool is_animating) {}

content::WebContents* VivaldiBrowserWindow::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void VivaldiBrowserWindow::OnStateChanged(ui::WindowShowState state) {
  if (browser_.get() == nullptr) {
    return;
  }
  using extensions::vivaldi::window_private::WindowState;
  WindowState window_state = ConvertFromWindowShowState(state);
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnStateChanged::kEventName,
      extensions::vivaldi::window_private::OnStateChanged::Create(id(), window_state),
      browser_->profile());
}

void VivaldiBrowserWindow::OnActivationChanged(bool activated) {
  // Browser can be nullptr if our UI renderer has crashed.
  if (!browser_)
    return;

  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnActivated::kEventName,
      extensions::vivaldi::window_private::OnActivated::Create(id(), activated),
      browser_->profile());
}

void VivaldiBrowserWindow::OnPositionChanged() {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnPositionChanged::kEventName,
      extensions::vivaldi::window_private::OnPositionChanged::Create(id()),
      browser_->profile());
}

bool VivaldiBrowserWindow::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  return false;
}

ui::NativeTheme* VivaldiBrowserWindow::GetNativeTheme() {
  return nullptr;
}

int VivaldiBrowserWindow::GetTopControlsHeight() const {
  return 0;
}

bool VivaldiBrowserWindow::CanUserExitFullscreen() const {
  return true;
}

VivaldiBrowserWindow::VivaldiManagePasswordsIconView::
    VivaldiManagePasswordsIconView(VivaldiBrowserWindow& window)
    : window_(window) {}

void VivaldiBrowserWindow::VivaldiManagePasswordsIconView::SetState(
    password_manager::ui::State state) {
  extensions::VivaldiUtilitiesAPI* utils_api =
      extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(
          window_.browser()->profile());
  bool show = state == password_manager::ui::State::PENDING_PASSWORD_STATE;
  show = state != password_manager::ui::State::INACTIVE_STATE;
  utils_api->OnPasswordIconStatusChanged(window_.id(), show);
}

bool VivaldiBrowserWindow::VivaldiManagePasswordsIconView::Update() {
  // contents can be null when we recover after UI process crash.
  content::WebContents* web_contents =
      window_.browser()->tab_strip_model()->GetActiveWebContents();
  if (web_contents) {
    ManagePasswordsUIController::FromWebContents(web_contents)
        ->UpdateIconAndBubbleState(this);
    return true;
  }
  return false;
}

send_tab_to_self::SendTabToSelfBubbleView*
VivaldiBrowserWindow::ShowSendTabToSelfBubble(
    content::WebContents* contents,
    send_tab_to_self::SendTabToSelfBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

sharing_hub::SharingHubBubbleView* VivaldiBrowserWindow::ShowSharingHubBubble(
    content::WebContents* contents,
    sharing_hub::SharingHubBubbleController* controller,
    bool is_user_gesture) {
  return nullptr;
}

void VivaldiBrowserWindow::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  if (changed_flags & content::INVALIDATE_TYPE_LOAD) {
    if (source == GetActiveWebContents()) {
      std::u16string statustext =
          CoreTabHelper::FromWebContents(source)->GetStatusText();
      ::vivaldi::BroadcastEvent(
          extensions::vivaldi::window_private::OnActiveTabStatusText::
              kEventName,
          extensions::vivaldi::window_private::OnActiveTabStatusText::Create(
              id(), base::UTF16ToUTF8(statustext)),
          GetProfile());
    }
  }
}

ExtensionsContainer* VivaldiBrowserWindow::GetExtensionsContainer() {
  return nullptr;
}

ui::ZOrderLevel VivaldiBrowserWindow::GetZOrderLevel() const {
  return ui::ZOrderLevel::kNormal;
}

SharingDialog* VivaldiBrowserWindow::ShowSharingDialog(
    content::WebContents* contents,
    SharingDialogData data) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool VivaldiBrowserWindow::IsOnCurrentWorkspace() const {
  return views_->IsOnCurrentWorkspace();
}

void VivaldiBrowserWindow::UpdatePageActionIcon(PageActionIconType type) {
  if (type == PageActionIconType::kManagePasswords) {
    icon_view_.Update();
  }
}

autofill::AutofillBubbleHandler*
VivaldiBrowserWindow::GetAutofillBubbleHandler() {
  if (!autofill_bubble_handler_) {
    autofill_bubble_handler_ = std::make_unique<VivaldiAutofillBubbleHandler>();
  }
  return autofill_bubble_handler_.get();
}

qrcode_generator::QRCodeGeneratorBubbleView*
VivaldiBrowserWindow::ShowQRCodeGeneratorBubble(
    content::WebContents* contents,
    qrcode_generator::QRCodeGeneratorBubbleController* controller,
    const GURL& url) {
  sessions::SessionTabHelper* const session_tab_helper =
      sessions::SessionTabHelper::FromWebContents(contents);

  // This is called if the user uses the page context menu to generate a QR
  // code.
  vivaldi::BroadcastEvent(
      extensions::vivaldi::utilities::OnShowQRCode::kEventName,
      extensions::vivaldi::utilities::OnShowQRCode::Create(
          session_tab_helper->session_id().id(), url.spec()),
      browser_->profile());

  return nullptr;
}

void VivaldiBrowserWindow::SetDidFinishNavigationCallback(
    DidFinishNavigationCallback callback) {
  DCHECK(callback);
  DCHECK(!did_finish_navigation_callback_);
  did_finish_navigation_callback_ = std::move(callback);
}

void VivaldiBrowserWindow::OnDidFinishNavigation(bool success) {
  if (did_finish_navigation_callback_) {
    std::move(did_finish_navigation_callback_).Run(success ? this : nullptr);
  }
}

std::unique_ptr<content::EyeDropper> VivaldiBrowserWindow::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return ShowEyeDropper(frame, listener);
}

FeaturePromoController* VivaldiBrowserWindow::GetFeaturePromoController() {
  return nullptr;
}
