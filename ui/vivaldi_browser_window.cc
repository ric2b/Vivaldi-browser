// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/vivaldi_browser_window.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "browser/sessions/vivaldi_session_utils.h"
#include "browser/vivaldi_runtime_feature.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/audio_focus_web_contents_observer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
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
#include "chrome/browser/send_tab_to_self/receiving_ui_handler_registry.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/autofill/add_new_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/save_address_bubble_controller.h"
#include "chrome/browser/ui/autofill/update_address_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
//#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/views/autofill/autofill_bubble_handler_impl.h"
#include "chrome/browser/ui/views/download/download_in_progress_dialog_view.h"
#include "chrome/browser/ui/views/extensions/extension_keybinding_registry_views.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/direct_match/direct_match_service_factory.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/mojom/app_window.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "sessions/index_service_factory.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/devtools/devtools_connector.h"
#include "ui/display/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget_observer.h"

#include "app/vivaldi_constants.h"
#include "browser/menus/vivaldi_menus.h"
#include "browser/vivaldi_browser_finder.h"
#include "extensions/api/events/vivaldi_ui_events.h"
#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/api/vivaldi_utilities/vivaldi_utilities_api.h"
#include "extensions/api/window/window_private_api.h"
#include "extensions/helper/vivaldi_app_helper.h"
#include "extensions/schema/tabs_private.h"
#include "extensions/schema/vivaldi_utilities.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#if defined(ENABLE_RELAY_PROXY)
#include "proxy/launcher.h"
#endif
#include "ui/infobar_container_web_proxy.h"
#include "ui/views/vivaldi_native_widget.h"
#include "ui/views/vivaldi_window_widget_delegate.h"
#include "ui/vivaldi_location_bar.h"
#include "ui/vivaldi_quit_confirmation_dialog.h"
#include "ui/vivaldi_rootdocument_handler.h"
#include "ui/vivaldi_ui_utils.h"
#include "ui/window_registry_service.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/views/theme_profile_key.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_MAC)
#import "browser/vivaldi_app_observer.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/win/jumplist_factory.h"
#include "ui/gfx/win/hwnd_util.h"

#include "browser/win/vivaldi_utils.h"
#endif

// Used by Chrome's send tab to self functionality to pass data as events to JS.
VivaldiUIRelay::VivaldiUIRelay(Profile* profile)
  :profile_(profile) {}

void VivaldiUIRelay::DisplayNewEntries(
  const std::vector<const send_tab_to_self::SendTabToSelfEntry*>& new_entries) {
  std::vector<extensions::vivaldi::tabs_private::SendTabToSelfEntry> list;
  for (auto entry : new_entries) {
    extensions::vivaldi::tabs_private::SendTabToSelfEntry item;
    item.guid = entry->GetGUID();
    item.url = entry->GetURL().spec();
    item.title = entry->GetTitle();
    item.device_name = entry->GetDeviceName();
    item.shared_time = entry->GetSharedTime().InMillisecondsFSinceUnixEpoch();
    list.push_back(std::move(item));
  }
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::tabs_private::OnSendTabToSelfAdded::kEventName,
      extensions::vivaldi::tabs_private::OnSendTabToSelfAdded::Create(list),
      profile_);
}

void VivaldiUIRelay::DismissEntries(const std::vector<std::string>& guids) {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::tabs_private::OnSendTabToSelfDismissed::kEventName,
      extensions::vivaldi::tabs_private::OnSendTabToSelfDismissed::Create(guids),
      profile_);
}

const Profile* VivaldiUIRelay::profile() const {
  return profile_;
}


namespace {

Browser* FindActiveBrowser() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->window()->IsActive()) {
      return browser;
    }
  }
  return nullptr;
}
}  // namespace

// The document loaded in portal-windows.
#define VIVALDI_WINDOW_DOCUMENT "window.html"

using extensions::vivaldi::window_private::WindowState;
WindowState ConvertToJSWindowState(ui::mojom::WindowShowState state) {
  switch (state) {
    case ui::mojom::WindowShowState::kFullscreen:
      return WindowState::kFullscreen;
    case ui::mojom::WindowShowState::kMaximized:
      return WindowState::kMaximized;
    case ui::mojom::WindowShowState::kMinimized:
      return WindowState::kMinimized;
    default:
      return WindowState::kNormal;
  }
  NOTREACHED();
  //return WindowState::kNormal;
}

class VivaldiBrowserWindow::InterfaceHelper final
    : public ExclusiveAccessContext,
      public ManagePasswordsIconView,
      public autofill::AutofillBubbleHandler,
      public extensions::ExtensionFunctionDispatcher::Delegate,
      public extensions::ExtensionKeybindingRegistry::Delegate,
      public extensions::ExtensionRegistryObserver,
      public extensions::VivaldiRootDocumentHandlerObserver,
      public ui::AcceleratorProvider,
      public views::WidgetObserver,
      public web_modal::WebContentsModalDialogHost,
      public web_modal::WebContentsModalDialogManagerDelegate {
 public:
  InterfaceHelper(VivaldiBrowserWindow& window) : window_(window) {}
  ~InterfaceHelper() override = default;

 private:
  // ExclusiveAccessContext overrides

  Profile* GetProfile() override { return window_->GetProfile(); }

  autofill::AutofillBubbleHandler* GetAutofillBubbleHandler() {
    return window_->autofill_bubble_handler_.get();
  }

  bool IsFullscreen() const override { return window_->IsFullscreen(); }

  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type,
                       int64_t display_id) override {
    window_->SetFullscreen(true);
  }

  void ExitFullscreen() override { window_->SetFullscreen(false); }

  bool CanUserExitFullscreen() const override { return true; }

  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override {}

  bool IsExclusiveAccessBubbleDisplayed() const override { return false; }

  void OnExclusiveAccessUserInput() override {}

  content::WebContents* GetWebContentsForExclusiveAccess() override {
    return window_->GetActiveWebContents();
  }

  // ManagePasswordsIconView overrides

  void SetState(password_manager::ui::State state) override {
    extensions::VivaldiUtilitiesAPI* utils_api =
        extensions::VivaldiUtilitiesAPI::GetFactoryInstance()->Get(
            window_->browser()->profile());
    bool show = state == password_manager::ui::State::PENDING_PASSWORD_STATE;
    show = state != password_manager::ui::State::INACTIVE_STATE;
    utils_api->OnPasswordIconStatusChanged(window_->id(), show);
  }

  // autofill::AutofillBubbleHandler overrides

  autofill::AutofillBubbleBase* ShowSaveCreditCardBubble(
      content::WebContents* web_contents,
      autofill::SaveCardBubbleController* controller,
      bool is_user_gesture) override {
    // Hold a AutofillBubbleHandlerImpl and implement a toolbarprovider for
    // anchor points.
    return GetAutofillBubbleHandler()->ShowSaveCreditCardBubble(
        web_contents, controller, is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowLocalCardMigrationBubble(
      content::WebContents* web_contents,
      autofill::LocalCardMigrationBubbleController* controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowLocalCardMigrationBubble(
        web_contents, controller, is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowIbanBubble(
      content::WebContents* web_contents,
      autofill::IbanBubbleController* controller,
      bool is_user_gesture,
      autofill::IbanBubbleType bubble_type) override {
    return GetAutofillBubbleHandler()->ShowIbanBubble(
        web_contents, controller, is_user_gesture, bubble_type);
  }

  autofill::AutofillBubbleBase* ShowOfferNotificationBubble(
      content::WebContents* web_contents,
      autofill::OfferNotificationBubbleController* controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowOfferNotificationBubble(
        web_contents, controller, is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowSaveAutofillPredictionImprovementsBubble(
      content::WebContents* web_contents,
      autofill::SaveAutofillPredictionImprovementsController* controller)
      override {
    return GetAutofillBubbleHandler()
        ->ShowSaveAutofillPredictionImprovementsBubble(web_contents,
                                                       controller);
  }

  autofill::AutofillBubbleBase* ShowSaveAddressProfileBubble(
      content::WebContents* web_contents,
      std::unique_ptr<autofill::SaveAddressBubbleController> controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowSaveAddressProfileBubble(
        web_contents, std::move(controller), is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowUpdateAddressProfileBubble(
      content::WebContents* web_contents,
      std::unique_ptr<autofill::UpdateAddressBubbleController> controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowUpdateAddressProfileBubble(
        web_contents, std::move(controller), is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowAddNewAddressProfileBubble(
      content::WebContents* web_contents,
      std::unique_ptr<autofill::AddNewAddressBubbleController> controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowAddNewAddressProfileBubble(
        web_contents, std::move(controller), is_user_gesture);
  }

  virtual autofill::AutofillBubbleBase* ShowFilledCardInformationBubble(
      content::WebContents* web_contents,
      autofill::FilledCardInformationBubbleController* controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowFilledCardInformationBubble(
        web_contents, std::move(controller), is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowVirtualCardEnrollBubble(
      content::WebContents* web_contents,
      autofill::VirtualCardEnrollBubbleController* controller,
      bool is_user_gesture) override {
    return GetAutofillBubbleHandler()->ShowVirtualCardEnrollBubble(
        web_contents, controller, is_user_gesture);
  }

  autofill::AutofillBubbleBase* ShowVirtualCardEnrollConfirmationBubble(
      content::WebContents* web_contents,
      autofill::VirtualCardEnrollBubbleController* controller) override {
    return GetAutofillBubbleHandler()->ShowVirtualCardEnrollConfirmationBubble(
        web_contents, controller);
  }

  autofill::AutofillBubbleBase* ShowMandatoryReauthBubble(
      content::WebContents* web_contents,
      autofill::MandatoryReauthBubbleController* controller,
      bool is_user_gesture,
      autofill::MandatoryReauthBubbleType bubble_type) override {
    return GetAutofillBubbleHandler()->ShowMandatoryReauthBubble(
        web_contents, controller, is_user_gesture, bubble_type);
  }

  autofill::AutofillBubbleBase* ShowSaveCardConfirmationBubble(
      content::WebContents* web_contents,
      autofill::SaveCardBubbleController* controller) override {
    return GetAutofillBubbleHandler()->ShowSaveCardConfirmationBubble(
        web_contents, controller);
  }

  autofill::AutofillBubbleBase* ShowSaveIbanConfirmationBubble(
    content::WebContents* web_contents,
    autofill::IbanBubbleController* controller) override {
    return GetAutofillBubbleHandler()->ShowSaveIbanConfirmationBubble(
        web_contents, controller);
  }

  // ExtensionFunctionDispatcher::Delegate overrides

  extensions::WindowController* GetExtensionWindowController() const override {
    return window_->browser()->extension_window_controller();
  }

  content::WebContents* GetAssociatedWebContents() const override {
    return window_->web_contents();
  }

  // ExtensionKeybindingRegistry::Delegate overrides

  content::WebContents* GetWebContentsForExtension() override {
    return GetWebContentsForExclusiveAccess();
  }

  // ExtensionRegistryObserver overrides

  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (vivaldi::kVivaldiAppId == extension->id()) {
      window_->Close();
    }
  }

  // VivaldiRootDocumentHandlerObserver

  void OnRootDocumentDidFinishNavigation() override {
    // This window is no longer interested in states from the root document.
    window_->root_doc_handler_->RemoveObserver(this);
  }

  content::WebContents* GetRootDocumentWebContents() override {
    return window_->web_contents_.get();
  }

  // ui::AcceleratorProvider overrides

  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return vivaldi::GetFixedAcceleratorForCommandId(command_id, accelerator);
  }

  // views::WidgetObserver overrides

  void OnWidgetDestroying(views::Widget* widget) override {
    if (window_->widget_ != widget)
      return;
    for (auto& observer : window_->modal_dialog_observers_) {
      observer.OnHostDestroying();
    }
  }

  void OnWidgetDestroyed(views::Widget* widget) override {
    if (window_->widget_ != widget)
      return;
    window_->widget_->RemoveObserver(this);
    window_->widget_ = nullptr;
    window_->OnNativeClose();

    // Reset the keybinding registry here. Otherwise its destructor will be
    // called too late and crash the browser.
    extension_keybinding_registry_.reset();
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    if (window_->widget_ != widget)
      return;
    window_->OnNativeWindowChanged();
  }

  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (window_->widget_ != widget)
      return;
    window_->OnNativeWindowChanged();
    window_->OnNativeWindowActivationChanged(active);
    Browser* browser = window_->browser();
    if (!active && browser) {
      BrowserList::NotifyBrowserNoLongerActive(browser);
    }

    if (!extension_keybinding_registry_ &&
        widget->GetFocusManager()) {  // focus manager can be null in tests.
      extension_keybinding_registry_ =
          std::make_unique<ExtensionKeybindingRegistryViews>(
              browser->profile(), widget->GetFocusManager(),
              extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS, this);
    }
  }

  // web_modal::WebContentsModalDialogHost overrides

  gfx::NativeView GetHostView() const override {
    return window_->GetNativeView();
  }

  gfx::Point GetDialogPosition(const gfx::Size& size) override {
    if (!window_->widget_)
      return gfx::Point();
    gfx::Size window_size = window_->widget_->GetWindowBoundsInScreen().size();
    return gfx::Point(window_size.width() / 2 - size.width() / 2,
                      window_size.height() / 2 - size.height() / 2);
  }

  gfx::Size GetMaximumDialogSize() override {
    if (!window_->widget_)
      return gfx::Size();
    return window_->widget_->GetWindowBoundsInScreen().size();
  }

  void AddObserver(web_modal::ModalDialogHostObserver* observer) override {
    window_->modal_dialog_observers_.AddObserver(observer);
  }

  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override {
    window_->modal_dialog_observers_.RemoveObserver(observer);
  }

  // web_modal::WebContentsModalDialogManagerDelegate overrides

  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override {
    return window_->GetWebContentsModalDialogHost();
  }

  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override {
    // The implementation is copied from
    // ChromeAppDelegate::SetWebContentsBlocked().
    if (!blocked) {
      content::RenderWidgetHostView* rwhv =
          web_contents->GetRenderWidgetHostView();
      if (rwhv)
        rwhv->Focus();
    }
    // RenderViewHost may be NULL during shutdown.
    content::RenderFrameHost* host = web_contents->GetPrimaryMainFrame();
    if (host && host->GetRemoteInterfaces()) {
      mojo::Remote<extensions::mojom::AppWindow> app_window;
      host->GetRemoteInterfaces()->GetInterface(
          app_window.BindNewPipeAndPassReceiver());
      app_window->SetVisuallyDeemphasized(blocked);
    }
  }

  bool IsWebContentsVisible(content::WebContents* web_contents) override {
    if (web_contents->GetNativeView())
      return platform_util::IsVisible(web_contents->GetNativeView());
    return false;
  }

  const raw_ref<VivaldiBrowserWindow> window_;

  // The class that registers for keyboard shortcuts for extension commands.
  std::unique_ptr<ExtensionKeybindingRegistryViews>
      extension_keybinding_registry_;
};

namespace {

static base::TimeTicks g_first_window_creation_time;

#if defined(USE_AURA)
// The class is copied from app_window_easy_resize_window_targeter.cc
// in chromium/chrome/browser/ui/views/apps.
// An EasyResizeEventTargeter whose behavior depends on the state of the app
// window.
class VivaldiWindowEasyResizeWindowTargeter
    : public wm::EasyResizeWindowTargeter {
 public:
  VivaldiWindowEasyResizeWindowTargeter(const gfx::Insets& insets,
                                        VivaldiBrowserWindow* window)
      : wm::EasyResizeWindowTargeter(insets, insets), window_(window) {}

  ~VivaldiWindowEasyResizeWindowTargeter() override = default;
  VivaldiWindowEasyResizeWindowTargeter(
      const VivaldiWindowEasyResizeWindowTargeter&) = delete;
  VivaldiWindowEasyResizeWindowTargeter& operator=(
      const VivaldiWindowEasyResizeWindowTargeter&) = delete;

 protected:
  // aura::WindowTargeter:
  bool GetHitTestRects(aura::Window* window,
                       gfx::Rect* rect_mouse,
                       gfx::Rect* rect_touch) const override {
    // EasyResizeWindowTargeter intercepts events landing at the edges of the
    // window. Since maximized and fullscreen windows can't be resized anyway,
    // skip EasyResizeWindowTargeter so that the web contents receive all mouse
    // events.
    if (window_->IsMaximized() || window_->IsFullscreen())
      return WindowTargeter::GetHitTestRects(window, rect_mouse, rect_touch);

    return EasyResizeWindowTargeter::GetHitTestRects(window, rect_mouse,
                                                     rect_touch);
  }

 private:
  raw_ptr<VivaldiBrowserWindow> window_;
};
#endif

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
    Profile* creatorprofile = Profile::FromBrowserContext(
        creator_frame->GetSiteInstance()->GetBrowserContext());

    if (!creatorprofile->IsOffTheRecord()) {
      create_params.opener_render_process_id =
          creator_frame->GetProcess()->GetID();
      create_params.opener_render_frame_id = creator_frame->GetRoutingID();

      // All windows for the same profile should share the same process.
      DCHECK(create_params.opener_render_process_id == extension_process_id);
      if (create_params.opener_render_process_id != extension_process_id) {
        LOG(ERROR)
            << "VivaldiWindow WebContents will be created in the process ("
            << extension_process_id << ") != creator ("
            << create_params.opener_render_process_id << "). Routing disabled.";
      }
    }
  }
  LOG(INFO) << "VivaldiWindow WebContents will be created in the process "
            << extension_process_id
            << ", window_id=" << browser->session_id().id();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  // Create this early as it's used in GetOrCreateWebPreferences's call to
  // VivaldiContentBrowserClientParts::OverrideWebkitPrefs.
  extensions::VivaldiAppHelper::CreateForWebContents(web_contents.get());

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

  // Enable opening of dropped files if nothing can handle the drop.
  web_contents->GetMutableRendererPrefs()->can_accept_load_drops = true;

  return web_contents;
}

// This is based on GetInitialWindowBounds() from
// chromium/extensions/browser/app_window/app_window.cc
gfx::Rect GetInitialWindowBounds(const VivaldiBrowserWindowParams& params,
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
    combined_bounds.set_height(params.content_bounds.height() +
                               frame_insets.height());
  }

  // Constrain the bounds.
  gfx::Size size = combined_bounds.size();
  size.SetToMax(params.minimum_size);
  combined_bounds.set_size(size);

  return combined_bounds;
}

}  // namespace

VivaldiBrowserWindowParams::VivaldiBrowserWindowParams() = default;
VivaldiBrowserWindowParams::~VivaldiBrowserWindowParams() = default;

// VivaldiBrowserWindow --------------------------------------------------------

VivaldiBrowserWindow::VivaldiBrowserWindow()
    : interface_helper_(std::make_unique<InterfaceHelper>(*this)) {
  if (g_first_window_creation_time.is_null()) {
    g_first_window_creation_time = base::TimeTicks::Now();
  }
}

VivaldiBrowserWindow::~VivaldiBrowserWindow() {
#if defined(ENABLE_RELAY_PROXY)
  // Mac closes connection on exit (app_controller_mac.mm),
#if !BUILDFLAG(IS_MAC)
  if (BrowserList::GetInstance()->size() <= 1) {
    // Disconnect no matter what.
    vivaldi::proxy::disconnect();
  } else if (type() == NORMAL) {
    // Disconnect if last normal window is closing.
    int num_normal = vivaldi::GetBrowserCountOfType(Browser::TYPE_NORMAL) +
      vivaldi::GetBrowserCountOfType(Browser::TYPE_POPUP);
    if (num_normal == 1) {
      vivaldi::proxy::disconnect();
    }
  }
#endif
#endif

  // The WindowRegistryService can return null.
  if (vivaldi::WindowRegistryService::Get(GetProfile())) {
    vivaldi::WindowRegistryService::Get(GetProfile())
        ->RemoveWindow(window_key_);
  }
  DCHECK(root_doc_handler_);
  root_doc_handler_->RemoveObserver(interface_helper_.get());
  OnDidFinishNavigation(false);

  if (quit_dialog_owner_ == this) {
    SetQuitDialogOwner(nullptr);
  }
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

VivaldiBrowserWindow* VivaldiBrowserWindow::FromId(
    SessionID::id_type window_id) {
  Browser* browser = vivaldi::FindBrowserByWindowId(window_id);
  VivaldiBrowserWindow* window = VivaldiBrowserWindow::FromBrowser(browser);
  if (window && !window->web_contents()) {
    // Window is about to be destroyed, do not return it.
    window = nullptr;
  }
  return window;
}

// static
VivaldiBrowserWindow* VivaldiBrowserWindow::CreateVivaldiBrowserWindow(
    std::unique_ptr<Browser> browser) {
  DCHECK(browser);

  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();

  VivaldiBrowserWindowParams params;
  // Note that this differ from VivaldiWindowClientView::GetMinimumSize() which
  // among other things determine how small a window can be resized from WM. The
  // minimum size here controls how small a window can be when opened.
  params.minimum_size = gfx::Size(std::min(500, display_size.width()),
                                  std::min(300, display_size.height()));
  params.native_decorations = browser->profile()->GetPrefs()->GetBoolean(
      vivaldiprefs::kWindowsUseNativeDecoration);

  chrome::GetSavedWindowBoundsAndShowState(
      browser.get(), &params.content_bounds, &params.state);
  params.workspace = browser->initial_workspace();
  params.visible_on_all_workspaces =
      browser->initial_visible_on_all_workspaces_state();

  VivaldiBrowserWindow* window = new VivaldiBrowserWindow();

  params.resource_relative_url = VIVALDI_WINDOW_DOCUMENT;
  window->SetWindowURL(params.resource_relative_url);
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
  with_native_frame_ = params.native_decorations;

  minimum_size_ = params.minimum_size;
  location_bar_ = std::make_unique<VivaldiLocationBar>(*this);
#if BUILDFLAG(IS_WIN)
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
      ->set_delegate(interface_helper_.get());

  autofill::ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromePasswordManagerClient::CreateForWebContents(web_contents());
  ManagePasswordsUIController::CreateForWebContents(web_contents());
  TabDialogs::CreateForWebContents(web_contents());

  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      web_contents());

  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents())
      ->SetDelegate(interface_helper_.get());

  InitWidget(params);

  browser_->set_initial_show_state(params.state);


  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (const auto& iter : extensions::IconsInfo::GetIcons(extension()).map()) {
    extensions::ExtensionResource resource =
        extension()->GetResource(iter.second);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          resource, extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(iter.first, iter.first), ui::k100Percent));
    }
  }
  extensions::ImageLoader* loader = extensions::ImageLoader::Get(GetProfile());
  loader->LoadImageFamilyAsync(
      extension(), info_list,
      base::BindOnce(&VivaldiBrowserWindow::OnIconImagesLoaded,
                     weak_ptr_factory_.GetWeakPtr()));

  // Set this as a listener for the root document holding portal-windows.
  root_doc_handler_ =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          GetProfile());

  DCHECK(root_doc_handler_);
  root_doc_handler_->AddObserver(interface_helper_.get());
  GURL resource_url = extension_->GetResourceURL(params.resource_relative_url);
  web_contents()->GetController().LoadURL(resource_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_LINK,
                                          std::string());

  toolbar_button_provider_ =
      std::make_unique<VivaldiToolbarButtonProvider>(this);

  autofill_bubble_handler_ =
      std::make_unique<autofill::AutofillBubbleHandlerImpl>(
          toolbar_button_provider_.get());

  // This will create a new instance of a VivaldiUIRelay maintained and owned by
  // the registry. The profile is key, there is only one relay per profile and
  // it is removed when profile dies.
  (void)send_tab_to_self::ReceivingUiHandlerRegistry::GetInstance()
      ->GetVivaldiUIRelayForProfile(browser_->profile());
}

void VivaldiBrowserWindow::InitWidget(
    const VivaldiBrowserWindowParams& create_params) {
  widget_delegate_ = std::make_unique<VivaldiWindowWidgetDelegate>(this);
  widget_delegate_->SetCanResize(browser_->create_params().can_resize);

  // widget_ is owned by the native widget it creates.
  widget_ = new views::Widget();
  widget_->AddObserver(interface_helper_.get());

  views::Widget::InitParams init_params(views::Widget::InitParams::TYPE_WINDOW);

  init_params.delegate = widget_delegate_.get();

  // On Windows it is not enough just to set this flag in InitParams to control
  // the native frame. ShouldUseNativeFrame() and GetFrameMode() methods in
  // VivaldiDesktopWindowTreeHostWin should be overwritten as well.
  init_params.remove_standard_frame = !with_native_frame_;

  if (create_params.alpha_enabled) {
    init_params.opacity =
        views::Widget::InitParams::WindowOpacity::kTranslucent;
    if (!with_native_frame_) {
      init_params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    }
  }
  init_params.visible_on_all_workspaces =
      create_params.visible_on_all_workspaces;
  init_params.workspace = create_params.workspace;

#if BUILDFLAG(IS_MAC)
  // Widget does manual memory management of NativeWidget
  init_params.native_widget = CreateVivaldiNativeWidget(this).release();
#elif BUILDFLAG(IS_LINUX)
  init_params.wm_class_name = shell_integration_linux::GetProgramClassName();
  init_params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  init_params.wayland_app_id = init_params.wm_class_class;
  const char kX11WindowRoleBrowser[] = "browser";
  const char kX11WindowRolePopup[] = "pop-up";
  init_params.wm_role_name =
      type() == VivaldiBrowserWindow::WindowType::SETTINGS
          ? std::string(kX11WindowRolePopup)
          : std::string(kX11WindowRoleBrowser);
#elif BUILDFLAG(IS_WIN)
  // Widget does manual memory management of NativeWidget
  init_params.native_widget = CreateVivaldiNativeWidget(this).release();
#endif

  widget_->Init(std::move(init_params));

  // Stow a pointer to the browser's profile onto the window handle so that we
  // can get it later when all we have is a native view.
  widget_->SetNativeWindowProperty(Profile::kProfileKey, browser()->profile());

  // The frame insets are required to resolve the bounds specifications
  // correctly. So we set the window bounds and constraints now.
  gfx::Insets frame_insets = GetFrameInsets();

  widget_->OnSizeConstraintsChanged();

  gfx::Rect window_bounds = GetInitialWindowBounds(create_params, frame_insets);
  if (!window_bounds.IsEmpty()) {
    if (window_bounds.x() != VivaldiBrowserWindowParams::kUnspecifiedPosition &&
        window_bounds.y() != VivaldiBrowserWindowParams::kUnspecifiedPosition) {
      widget_->SetBounds(window_bounds);
    } else {
      widget_->CenterWindow(window_bounds.size());
    }
  }

#if BUILDFLAG(IS_WIN)
  SetupShellIntegration(create_params);
#endif

#if BUILDFLAG(IS_LINUX)
  // This is required to make the code work.
  SetThemeProfileForWindow(GetNativeWindow(), GetProfile());

  // Setting the native theme on the top widget improves performance as the
  // widget code would otherwise have to do more work in every call to
  // Widget::GetNativeTheme(). Chrome does this in
  // BrowserFrame::SelectNativeTheme()
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  const auto* linux_ui_theme =
      ui::LinuxUiTheme::GetForWindow(GetNativeWindow());
  if (linux_ui_theme) {
    native_theme = linux_ui_theme->GetNativeTheme();
  }
  widget_->SetNativeThemeForTest(native_theme);
#endif
}

views::View* VivaldiBrowserWindow::GetWebView() const {
  if (!widget_)
    return nullptr;
  views::ClientView* client_view = widget_->client_view();
  if (!client_view || client_view->children().empty())
    return nullptr;
  return client_view->children()[0];
}

void VivaldiBrowserWindow::OnIconImagesLoaded(gfx::ImageFamily image_family) {
  icon_family_ = std::move(image_family);
  if (widget_) {
    widget_->UpdateWindowIcon();
  }
}

void VivaldiBrowserWindow::ContentsDidStartNavigation() {}

void VivaldiBrowserWindow::ContentsLoadCompletedInMainFrame() {
  // inject the browser id when the document is done loading
  std::string js = base::StringPrintf(
      "window.vivaldiWindowId = %i; window.selectWindow = map => map.get(%i);",
      id(), id());
  std::u16string script;
  if (!base::UTF8ToUTF16(js.c_str(), js.length(), &script)) {
    NOTREACHED();
    //return;
  }

  // Unretained is safe here because VivaldiBrowserWindow owns everything.
  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(
      script,
      base::BindOnce(&VivaldiBrowserWindow::InjectVivaldiWindowIdComplete,
                     base::Unretained(this)));
}

void VivaldiBrowserWindow::InjectVivaldiWindowIdComplete(base::Value result) {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnWebContentsHasWindow::kEventName,
      extensions::vivaldi::window_private::OnWebContentsHasWindow::Create(id()),
      browser_->profile());
}

void VivaldiBrowserWindow::OnUIReady() {
#if BUILDFLAG(IS_MAC)
  vivaldi::VivaldiAppObserver::Get(browser_->profile())->
      OnWindowShown(this, true);
#endif
}

void VivaldiBrowserWindow::Show() {
  // This function can not do the actual showing of window. It is called too
  // early by StartupBrowserCreatorImpl::OpenTabsInBrowser() &&
  // SessionRestoreImpl::RestoreTab() for VivaldiSplashBackground to be ready.
  // Actual showing of window takes place in ShowForReal().
#if !BUILDFLAG(IS_WIN)
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

#if BUILDFLAG(IS_MAC)
  // VB-97912 Opening a new Window there is no focus on this window.
  ui::mojom::WindowShowState current_state = GetRestoredState();
  widget_->SetInitialFocus(current_state);
#endif
}

void VivaldiBrowserWindow::ShowForReal() {
  is_hidden_ = false;

  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::CHROME_APP_DELEGATE, KeepAliveRestartOption::DISABLED);

  if (!widget_)
    return;

  // In maximized state IsVisible is true and activate does not show a
  // hidden window.
  ui::mojom::WindowShowState current_state = GetRestoredState();
  if (widget_->IsVisible() &&
      current_state != ui::mojom::WindowShowState::kMaximized) {
    widget_->Activate();
  } else {
    widget_->Show();
  }

  ui::mojom::WindowShowState initial_show_state =
      browser_->initial_show_state();
  if (initial_show_state == ui::mojom::WindowShowState::kFullscreen)
    SetFullscreen(true);
  else if (initial_show_state == ui::mojom::WindowShowState::kMaximized)
    Maximize();
  else if (initial_show_state == ui::mojom::WindowShowState::kMinimized)
    Minimize();

  OnNativeWindowChanged();

  // If not already present, load any persistent tabs (pinned and ws) that were
  // present when last window was closed without quitting application. Only Mac
  // saves this informantion now, but we may have to extend this for all due to
  // extension that allow the application to run in background with no windows.
  // These tabs are already present when we open the window using the profile
  // selector.
  sessions::OpenPersistentTabs(browser_.get(), HasPersistentTabs());
}

void VivaldiBrowserWindow::Hide() {
  is_hidden_ = true;
  if (widget_) {
    widget_->Hide();
  }
  keep_alive_.reset();
}

bool VivaldiBrowserWindow::IsVisible() const {
  if (!widget_)
    return false;
  return widget_->IsVisible();
}

void VivaldiBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  if (!widget_)
    return;
  widget_->SetBounds(bounds);
}

// Close can be called three times when closing a window:
// WM-'x' button -> Close() -> ConfirmWindowClose() -> Close() ->
// ConfirmWindowClose() -> Browser::ShouldCloseWindow() -> Close() ->
// ConfirmWindowClose()
// Any dialogs in ConfirmWindowClose() are only showed the first time but it
// is broken due to that onbeforeunload can interfere.
void VivaldiBrowserWindow::Close() {
#if BUILDFLAG(IS_WIN)
  // This must be as early as possible.
  bool should_quit_if_last_browser =
      browser_shutdown::IsTryingToQuit() ||
      KeepAliveRegistry::GetInstance()->IsKeepingAliveOnlyByBrowserOrigin();
  if (should_quit_if_last_browser) {
    vivaldi::OnShutdownStarted();
  }
#endif  // BUILDFLAG(IS_WIN)

  if (widget_) {
    // This can trigger our confirm dialogs and onbeforeunload events.
    widget_->Close();
  } else {
    // No widget - no callback to ConfirmWindowClose() or its delegate.
    CloseCleanup();
  }
}

// This function should:
// 1 Ideally be called only once during the window-close-sequence.
// 2 Must be called before any tabs have been removed.
// 3 Must be called after any dialog or code that can abort the close-sequence
//   (note: onbeforeunload)
void VivaldiBrowserWindow::CloseCleanup() {
  MovePersistentTabsToOtherWindowIfNeeded();
  extensions::DevtoolsConnectorAPI::CloseDevtoolsForBrowser(GetProfile(),
                                                            browser());
}

bool VivaldiBrowserWindow::HasPersistentTabs() {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (tab_strip_model->IsTabPinned(i)) {
      return true;
    }
    content::WebContents* content = tab_strip_model->GetWebContentsAt(i);
    if (extensions::IsTabInAWorkspace(content)) {
      return true;
    }
  }
  return false;
}

std::vector<int> VivaldiBrowserWindow::GetPersistentTabIds() {
  std::vector<int> ids;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (tab_strip_model->IsTabPinned(i)) {
      content::WebContents* content = tab_strip_model->GetWebContentsAt(i);
      ids.push_back(sessions::SessionTabHelper::IdForTab(content).id());
    } else {
      content::WebContents* content = tab_strip_model->GetWebContentsAt(i);
      if (extensions::IsTabInAWorkspace(content)) {
        ids.push_back(sessions::SessionTabHelper::IdForTab(content).id());
      }
    }
  }
  return ids;
}

void VivaldiBrowserWindow::MovePersistentTabsToOtherWindowIfNeeded() {
  Browser* candidate =
      ::vivaldi::ui_tools::FindBrowserForPersistentTabs(browser_.get());
  if (!candidate) {
    return;
  }

  is_moving_persistent_tabs_ = true;

  std::vector<int> pinned_tabs_to_move;
  std::vector<int> workspace_tabs_to_move;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* content = tab_strip_model->GetWebContentsAt(i);
    if (tab_strip_model->IsTabPinned(i)) {
      pinned_tabs_to_move.push_back(
          sessions::SessionTabHelper::IdForTab(content).id());
    } else if (extensions::IsTabInAWorkspace(content)) {
      workspace_tabs_to_move.push_back(
          sessions::SessionTabHelper::IdForTab(content).id());
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
  for (size_t i = 0; i < pinned_tabs_to_move.size(); i++) {
    if (::vivaldi::ui_tools::GetTabById(pinned_tabs_to_move[i], nullptr,
                                        &index)) {
      if (!::vivaldi::ui_tools::MoveTabToWindow(browser_.get(), candidate,
                                                index, &new_index, 0,
                                                AddTabTypes::ADD_PINNED)) {
        NOTREACHED();
        //break;
      }
      new_index += 1;
    }
  }

  for (size_t i = 0; i < workspace_tabs_to_move.size(); i++) {
    if (::vivaldi::ui_tools::GetTabById(workspace_tabs_to_move[i], nullptr,
                                        &index)) {
      if (!::vivaldi::ui_tools::MoveTabToWindow(browser_.get(), candidate,
                                                index, &new_index, 0,
                                                AddTabTypes::ADD_NONE)) {
        NOTREACHED();
        //break;
      }
      new_index += 1;
    }
  }
  is_moving_persistent_tabs_ = false;
}

// Saves to session if possible.
void VivaldiBrowserWindow::AutoSaveSession() {
#if !BUILDFLAG(IS_MAC)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->is_vivaldi()) {
      VivaldiBrowserWindow* window =
          static_cast<VivaldiBrowserWindow*>(browser->window());
      Profile* profile = window->GetProfile();
      if (!profile->IsGuestSession()) {
        if (sessions::IndexServiceFactory::GetForBrowserContextIfExists(
            profile)) {
          sessions::AutoSave(profile, true);
          break;
        }
      }
    }
  }
#endif  // !IS_MAC
}

// Similar to `CanClose()` and `OnWindowCloseRequested()` in views::BrowserView
bool VivaldiBrowserWindow::ConfirmWindowClose() {
  if (is_moving_persistent_tabs_) {
    // Returning false means the window will not be closed.
    return false;
  }

#if !BUILDFLAG(IS_MAC)
  int tabbed_windows_cnt = vivaldi::GetBrowserCountOfType(Browser::TYPE_NORMAL);
  bool isQuit = browser_shutdown::IsTryingToQuit() || tabbed_windows_cnt == 1;
  if (isQuit) {
    switch (GetQuitAction()) {
      case QuitAction::SaveSessionOnQuit:
        AutoSaveSession();
        break;
      case QuitAction::ShowDialogOnQuit: {
        // Only one window (in case there are more) shall open dialog.
        bool show = AcquireQuitDialog();
        if (show) {
          // We can attempt a close for a non-active window from the window panel.
          if (!IsActive()) {
            this->Activate();
          }
          // Dialog needs a visible window.
          if (IsMinimized()) {
            Restore();
          }
          new vivaldi::VivaldiQuitConfirmationDialog(
              base::BindOnce(&VivaldiBrowserWindow::ContinueClose,
                             weak_ptr_factory_.GetWeakPtr(),
                             CloseDialogMode::QuitApplication),
              nullptr, GetNativeWindow(),
              new vivaldi::VivaldiDialogQuitDelegate());
        }
        return false;
      }
      default:
        break;
    }
  }
  if (!browser_shutdown::IsTryingToQuit() && tabbed_windows_cnt >= 1) {
    if (ShouldShowDialogOnCloseWindow()) {
      // We can attempt a close for a non-active window from the window panel.
      if (!IsActive()) {
        this->Activate();
      }
      // Dialog needs a visible window.
      if (IsMinimized()) {
        Restore();
      }
      new vivaldi::VivaldiQuitConfirmationDialog(
          base::BindOnce(&VivaldiBrowserWindow::ContinueClose,
                         weak_ptr_factory_.GetWeakPtr(),
                         CloseDialogMode::CloseWindow),
          nullptr, GetNativeWindow(),
          new vivaldi::VivaldiDialogCloseWindowDelegate());
      return false;
    }
  }
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_MAC)
  // Save pinned tabs and/or WS tabs to a session file when we close the last
  // normal window (without quitting). It will be used to restore those tabs if
  // we open a new window.
  if (!browser_shutdown::IsTryingToQuit() &&
      ShouldSavePersistentTabsOnCloseWindow()) {
    std::vector<int> ids = GetPersistentTabIds();
    sessions::SavePersistentTabs(GetProfile(), ids);
  }
#endif  // BUILDFLAG(IS_MAC)

  // Since we do not show any dialog we have to call code that the delegate
  // would otherwise do.
  // TODO: This is too early (because of ShouldCloseWindow() below), but we do
  // not have a later hook at the moment that can be used with all tabs in
  // place.
  CloseCleanup();

  if (browser()->HandleBeforeClose() != BrowserClosingStatus::kPermitted) {
    // Onbeforunload events may have been fired with the call above. This means
    // the whole close operation can still be called off.
    return false;
  }

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

void VivaldiBrowserWindow::ContinueClose(CloseDialogMode mode,
                                         bool accepted,
                                         bool stop_asking) {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (accepted) {
    quit_dialog_shown_ = true;

    if (mode == CloseDialogMode::QuitApplication) {
      SetQuitDialogOwner(nullptr);
      prefs->SetBoolean(vivaldiprefs::kSystemShowExitConfirmationDialog,
                        !stop_asking);

      // Only one window shows a dialog and the rest must follow.
      AcceptQuitForAllWindows();
    } else {
      prefs->SetBoolean(vivaldiprefs::kWindowsShowWindowCloseConfirmationDialog,
                        !stop_asking);
      // TODO: This is too early as the browser() may fire onbeforeunload events
      // that again can abort the close sequence.
      CloseCleanup();
      Close();
    }
  } else {
    browser_shutdown::SetTryingToQuit(false);
  }
}

// static
// We used this function from Chromium to signal that a close action
// (application or window) has been stopped by the user due to beforeunload
// handling
void VivaldiBrowserWindow::CancelWindowClose() {
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b->is_vivaldi()) {
      VivaldiBrowserWindow* window =
          static_cast<VivaldiBrowserWindow*>(b->window());
      window->quit_dialog_shown_ = false;
      window->close_dialog_shown_ = false;
    }
  }
}

VivaldiBrowserWindow::QuitAction VivaldiBrowserWindow::GetQuitAction() {
  bool closed_due_to_profile =
      extensions::VivaldiWindowsAPI::IsWindowClosingBecauseProfileClose(
          browser());
  if (!closed_due_to_profile && !quit_dialog_shown_ &&
      browser()->type() == Browser::TYPE_NORMAL &&
      !GetProfile()->IsGuestSession()) {
    PrefService* prefs = GetProfile()->GetPrefs();
    return prefs->GetBoolean(vivaldiprefs::kSystemShowExitConfirmationDialog)
        ? QuitAction::ShowDialogOnQuit : QuitAction::SaveSessionOnQuit;
  } else {
    return QuitAction::DoNothingOnQuit;
  }
}

bool VivaldiBrowserWindow::ShouldShowDialogOnCloseWindow() {
  const PrefService* prefs = GetProfile()->GetPrefs();
  bool prompt_on_close = prefs->GetBoolean(
      vivaldiprefs::kWindowsShowWindowCloseConfirmationDialog);
  bool closed_due_to_profile =
      extensions::VivaldiWindowsAPI::IsWindowClosingBecauseProfileClose(
          browser());
  return prompt_on_close && !closed_due_to_profile && !quit_dialog_shown_ &&
         !close_dialog_shown_ &&
         // Can happen if all tabs have been moved (eg, if all are pinned)
         !browser()->tab_strip_model()->empty() &&
         browser()->type() == Browser::TYPE_NORMAL &&
         !GetProfile()->IsGuestSession();
}

// To be used when window closes while the application keeps running (typical
// behavior for Mac). Returns true if pinned tabs and open workspaces
// should be saved to a separate session entry. Normal session code does not
// handle saving pinned tabs and workspaces when last window closes. Behavior is
// to discard all. We want to keep it.
bool VivaldiBrowserWindow::ShouldSavePersistentTabsOnCloseWindow() {
  if (GetProfile()->IsGuestSession() || GetProfile()->IsOffTheRecord()) {
    return false;
  }
  int tabbed_windows_cnt = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->type() == Browser::TYPE_NORMAL) {
      if (!browser->profile()->IsGuestSession() &&
          !browser->profile()->IsOffTheRecord()) {
        tabbed_windows_cnt++;
      }
    }
  }
  if (tabbed_windows_cnt != 1) {
    return false;
  }

  return HasPersistentTabs();
}

void VivaldiBrowserWindow::SetQuitDialogOwner(VivaldiBrowserWindow* owner) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->is_vivaldi()) {
      static_cast<VivaldiBrowserWindow*>(browser->window())
          ->quit_dialog_owner_ = owner;
    }
  }
}

// We may have several windows. They will all call this function almost at the
// same time on quit. Only one will show the dialog.
bool VivaldiBrowserWindow::AcquireQuitDialog() {
  if (!quit_dialog_owner_) {
    if (vivaldi::GetBrowserCountOfType(Browser::TYPE_NORMAL) == 1) {
      SetQuitDialogOwner(this);
    } else {
      Browser* browser = FindActiveBrowser();
      if (!browser || browser == browser_.get()) {
        SetQuitDialogOwner(this);
      }
    }
  }
  return quit_dialog_owner_ == this;
}

// This is to signal a quit to all windows. Even those the do not show the
// dialog.
void VivaldiBrowserWindow::AcceptQuitForAllWindows() {
  AutoSaveSession();
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->is_vivaldi()) {
      VivaldiBrowserWindow* window =
          static_cast<VivaldiBrowserWindow*>(browser->window());
      window->quit_dialog_shown_ = true;
      window->CloseCleanup();
      window->Close();
    }
  }
}

DownloadBubbleUIController*
VivaldiBrowserWindow::GetDownloadBubbleUIController() {
  return nullptr;
}

void VivaldiBrowserWindow::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    base::OnceCallback<void(bool)> callback) {
#if BUILDFLAG(IS_MAC)
  // We allow closing the window here since the real quit decision on Mac is
  // made in [AppController quit:].
  std::move(callback).Run(true);
#else
  DownloadInProgressDialogView::Show(GetNativeWindow(), download_count,
                                     dialog_type, std::move(callback));
#endif  // OS_MAC
}

void VivaldiBrowserWindow::Activate() {
  if (!widget_)
    return;
  widget_->Activate();
  if (browser()) {
    BrowserList::SetLastActive(browser());
  }
}

void VivaldiBrowserWindow::Deactivate() {}

bool VivaldiBrowserWindow::IsActive() const {
  if (!widget_)
    return false;
  return widget_->IsActive();
}

gfx::NativeWindow VivaldiBrowserWindow::GetNativeWindow() const {
  if (!widget_)
    return gfx::NativeWindow();
  return widget_->GetNativeWindow();
}

StatusBubble* VivaldiBrowserWindow::GetStatusBubble() {
  return NULL;
}

gfx::Rect VivaldiBrowserWindow::GetRestoredBounds() const {
  if (!widget_) {
    return gfx::Rect();
  }

  gfx::Rect bounds = widget_->GetRestoredBounds();
#if BUILDFLAG(IS_WIN)
  // NOTE(andre@vivaldi.com) : VB-111399.
  // If the current window is maximized we need to take into account the magic
  // happening in the hwnd_message_handler where borders are added and removed
  // based on window state. This is just to make sure that we don't bleed onto
  // another monitor space and any new windows that should go to the same as
  // |this| stays. A border of 1 pixel is added in maximized state.
  if (IsMaximized()) {
    bounds.set_x(bounds.x() + 2);
    bounds.set_width(bounds.width() - 2);

    bounds.set_y(bounds.y() + 2);
    bounds.set_height(bounds.height() - 2);
  }
#endif //IS_WIN
  return bounds;
}

ui::mojom::WindowShowState VivaldiBrowserWindow::GetRestoredState() const {
  if (!widget_)
    return ui::mojom::WindowShowState::kDefault;

  // First normal states are checked.
  if (IsFullscreen())
    return ui::mojom::WindowShowState::kFullscreen;
  if (IsMaximized())
    return ui::mojom::WindowShowState::kMaximized;

#if defined(USE_AURA)
  // Use kRestoreShowStateKey in case a window is minimized/hidden.
  ui::mojom::WindowShowState restore_state =
      widget_->GetNativeWindow()->GetProperty(
      aura::client::kRestoreShowStateKey);

  // Whitelist states to return so that invalid and transient states
  // are not saved and used to restore windows when they are recreated.
  switch (restore_state) {
    case ui::mojom::WindowShowState::kNormal:
    case ui::mojom::WindowShowState::kMaximized:
    case ui::mojom::WindowShowState::kFullscreen:
      return restore_state;
    default:
      break;
  }
#endif

  return ui::mojom::WindowShowState::kNormal;
}

gfx::Rect VivaldiBrowserWindow::GetBounds() const {
  if (!widget_)
    return gfx::Rect();

  gfx::Rect bounds = widget_->GetWindowBoundsInScreen();
  gfx::Insets frame_insets = GetFrameInsets();
  bounds.Inset(frame_insets);
  return bounds;
}

gfx::Insets VivaldiBrowserWindow::GetFrameInsets() const {
  gfx::Insets frame_insets = gfx::Insets();
#if !BUILDFLAG(IS_WIN)
  if (with_native_frame_) {
    // The pretend client_bounds passed in need to be large enough to ensure
    // that GetWindowBoundsForClientBounds() doesn't decide that it needs more
    // than the specified amount of space to fit the window controls in, and
    // return a number larger than the real frame insets. Most window controls
    // are smaller than 1000x1000px, so this should be big enough.
    gfx::Rect client_bounds = gfx::Rect(1000, 1000);
    gfx::Rect window_bounds =
        widget_->non_client_view()->GetWindowBoundsForClientBounds(
            client_bounds);
    frame_insets = window_bounds.InsetsFrom(client_bounds);
  }
#endif
  return frame_insets;
}

bool VivaldiBrowserWindow::IsMaximized() const {
  if (!widget_)
    return false;
  return widget_->IsMaximized();
}

bool VivaldiBrowserWindow::IsMinimized() const {
  if (!widget_)
    return false;
  return widget_->IsMinimized();
}

void VivaldiBrowserWindow::Maximize() {
  if (!widget_)
    return;
  widget_->Maximize();
}

void VivaldiBrowserWindow::Minimize() {
  if (!widget_)
    return;
  widget_->Minimize();
}

void VivaldiBrowserWindow::Restore() {
  if (!widget_)
    return;
  if (IsFullscreen()) {
    widget_->SetFullscreen(false);
  } else {
    widget_->Restore();
  }
}

bool VivaldiBrowserWindow::ShouldHideUIForFullscreen() const {
  return IsFullscreen();
}

bool VivaldiBrowserWindow::IsFullscreenBubbleVisible() const {
  return false;
}

bool VivaldiBrowserWindow::IsForceFullscreen() const {
  return false;
}

LocationBar* VivaldiBrowserWindow::GetLocationBar() const {
  return location_bar_.get();
}

void VivaldiBrowserWindow::UpdateToolbar(content::WebContents* contents) {
  UpdatePageActionIcon(PageActionIconType::kManagePasswords);
}

bool VivaldiBrowserWindow::UpdateToolbarSecurityState() {
  // We may end up here during destruction.
  /* if (toolbar_.get()) {
    return toolbar_->UpdateSecurityState();
  }*/

  return false;
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
    const input::NativeWebKeyboardEvent& event) {
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool VivaldiBrowserWindow::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  bool is_auto_repeat;
#if BUILDFLAG(IS_MAC)
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
#endif  // BUILDFLAG(IS_MAC)

  extensions::VivaldiUIEvents::SendKeyboardShortcutEvent(
      id(), browser_->profile(), event, is_auto_repeat);

  if (!widget_)
    return false;

  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, widget_->GetFocusManager());
}

ui::AcceleratorProvider* VivaldiBrowserWindow::GetAcceleratorProvider() {
  return interface_helper_.get();
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
  return nullptr;
}

views::View* VivaldiBrowserWindow::GetTopContainer() {
  // TODO: Will implement js-events in VB-111658.
//  return GetContentsView();
  return nullptr;
}

// See comments on: BrowserWindow.VivaldiShowWebSiteSettingsAt.
void VivaldiBrowserWindow::VivaldiShowWebsiteSettingsAt(
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    gfx::Point pos) {
#if defined(USE_AURA)
  gfx::Rect anchor_rect = gfx::Rect();
#else  // Mac
  gfx::Rect anchor_rect = gfx::Rect(pos, gfx::Size());
#endif
  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          nullptr, anchor_rect, GetNativeWindow(), web_contents, url,
          base::DoNothing(),
          base::BindOnce(&VivaldiBrowserWindow::OnWebsiteSettingsStatClosed,
                         weak_ptr_factory_.GetWeakPtr()),
          false);
  bubble->SetAnchorRect(gfx::Rect(pos, gfx::Size()));
  bubble->GetWidget()->Show();
  ReportWebsiteSettingsState(true);
}

void VivaldiBrowserWindow::OnWebsiteSettingsStatClosed(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  ReportWebsiteSettingsState(false);
}

void VivaldiBrowserWindow::ReportWebsiteSettingsState(bool visible) {
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnPageInfoPopupChanged::kEventName,
      extensions::vivaldi::window_private::OnPageInfoPopupChanged::Create(
          id(), visible),
      GetProfile());
}

std::unique_ptr<FindBar> VivaldiBrowserWindow::CreateFindBar() {
  return std::unique_ptr<FindBar>();
}

ExclusiveAccessContext* VivaldiBrowserWindow::GetExclusiveAccessContext() {
  return interface_helper_.get();
}

void VivaldiBrowserWindow::DestroyBrowser() {
  // TODO(pettern): Crashes on shutdown, fix.
  //  extensions::ExtensionRegistry::Get(browser_->profile())->RemoveObserver(this);
  if (!browser_)
    return;
  browser_.reset();
}

gfx::Size VivaldiBrowserWindow::GetContentsSize() const {
  // TODO(pettern): This is likely not correct, should be tab contents.
  return GetBounds().size();
}

void VivaldiBrowserWindow::ShowEmojiPanel() {
  if (!widget_)
    return;
  widget_->ShowEmojiPanel();
}

std::string VivaldiBrowserWindow::GetWorkspace() const {
  if (!widget_)
    return std::string();
  return widget_->GetWorkspace();
}

bool VivaldiBrowserWindow::IsVisibleOnAllWorkspaces() const {
  if (!widget_)
    return false;
  return widget_->IsVisibleOnAllWorkspaces();
}

Profile* VivaldiBrowserWindow::GetProfile() const {
  return browser_ ? browser_->profile() : nullptr;
}

content::WebContents* VivaldiBrowserWindow::GetActiveWebContents() const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

ShowTranslateBubbleResult VivaldiBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  return ShowTranslateBubbleResult::BROWSER_WINDOW_NOT_VALID;
}

void VivaldiBrowserWindow::UpdateDevTools() {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();

  // DevToolsWindow code has already activated the tab.
  content::WebContents* contents = tab_strip_model->GetActiveWebContents();
  int tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
  extensions::DevtoolsConnectorAPI* api =
      extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
          browser_->profile());
  DCHECK(api);

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

  // Get the docking state.
  auto& prefs =
      browser_->profile()->GetPrefs()->GetDict(prefs::kDevToolsPreferences);

  std::string docking_state;

  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);

  if (window) {
    // We handle the closing devtools windows above.
    if (!window->IsClosing()) {
      const std::string* tmp_str = prefs.FindString("currentDockState");
      extensions::DevtoolsConnectorItem* item =
          api->GetOrCreateDevtoolsConnectorItem(tab_id);
      if (tmp_str) {
        docking_state = *tmp_str;
        // Strip quotation marks from the state.
        base::ReplaceChars(docking_state, "\"", "", &docking_state);
        if (item->docking_state() != docking_state) {
          item->set_docking_state(docking_state);

          extensions::DevtoolsConnectorAPI::SendDockingStateChanged(
              browser_->profile(), tab_id, docking_state);
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

  extensions::DevtoolsConnectorItem* item =
      api->GetOrCreateDevtoolsConnectorItem(tab_id);

  item->ResetDockingState();

  extensions::DevtoolsConnectorAPI::SendDockingStateChanged(
      browser_->profile(), tab_id, item->docking_state());
}

bool VivaldiBrowserWindow::IsToolbarShowing() const {
  return false;
}

bool VivaldiBrowserWindow::IsLocationBarVisible() const {
  return false;
}

views::View* VivaldiBrowserWindow::GetContentsView() const {
  if (!widget_)
    return nullptr;
  return widget_->GetContentsView();
}

gfx::NativeView VivaldiBrowserWindow::GetNativeView() {
  if (!widget_)
    return gfx::NativeView();
  return widget_->GetNativeView();
}

views::View* VivaldiBrowserWindow::GetBubbleDialogAnchor() const {
  return GetWebView();
}

void VivaldiBrowserWindow::OnNativeWindowChanged(bool moved) {
  // This may be called during Init before |widget_| is set.
  if (!widget_)
    return;

#if defined(USE_AURA)
  int resize_inside =
      (IsFullscreen() || IsMaximized()) ? 0 : resize_inside_bounds_size();
  gfx::Insets inset = gfx::Insets::TLBR(resize_inside, resize_inside,
                                        resize_inside, resize_inside);
  if (aura::Window* native_window = GetNativeWindow()) {
    // Add the targeter on the window, not its root window. The root window does
    // not have a delegate, which is needed to handle the event in Linux.
    std::unique_ptr<ui::EventTargeter> old_eventtarget =
        native_window->SetEventTargeter(
            std::make_unique<VivaldiWindowEasyResizeWindowTargeter>(
                gfx::Insets(inset), this));
  }
#endif

  WindowStateData old_state = window_state_data_;
  WindowStateData new_state;
  if (widget_->IsFullscreen()) {
    new_state.state = ui::mojom::WindowShowState::kFullscreen;
  } else if (widget_->IsMaximized()) {
    new_state.state = ui::mojom::WindowShowState::kMaximized;
  } else if (widget_->IsMinimized()) {
    new_state.state = ui::mojom::WindowShowState::kMinimized;
  } else {
    new_state.state = ui::mojom::WindowShowState::kNormal;
  }
  new_state.bounds = widget_->GetWindowBoundsInScreen();

  // Call the delegate so it can dispatch events to the js side.
  // Ignore the case when moving away from the initial value
  //  ui::mojom::WindowShowState::kDefault to the first valid state.
  if (old_state.state != ui::mojom::WindowShowState::kDefault &&
      old_state.state != new_state.state) {
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

  web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
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

void VivaldiBrowserWindow::OnViewWasResized() {
  for (auto& observer : modal_dialog_observers_) {
    observer.OnPositionRequiresUpdate();
  }
}

void VivaldiBrowserWindow::UpdateTitleBar() {
  if (!widget_)
    return;
  widget_->UpdateWindowTitle();
  widget_->UpdateWindowIcon();
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

  extensions::VivaldiRootDocumentHandler* rootdocument_handler =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          browser_->profile());

  rootdocument_handler->InfoBarContainer()->ChangeInfoBarManager(
      infobars::ContentInfoBarManager::FromWebContents(new_contents));
}

web_modal::WebContentsModalDialogHost*
VivaldiBrowserWindow::GetWebContentsModalDialogHost() {
  return interface_helper_.get();
}

void VivaldiBrowserWindow::SetFullscreen(bool enable) {
  if (!widget_)
    return;
  widget_->SetFullscreen(enable);
}

bool VivaldiBrowserWindow::IsFullscreen() const {
  if (!widget_)
    return false;
  return widget_->IsFullscreen();
}

void VivaldiBrowserWindow::OnStateChanged(ui::mojom::WindowShowState state) {
  if (browser_.get() == nullptr) {
    return;
  }
  using extensions::vivaldi::window_private::WindowState;
  WindowState window_state = ConvertToJSWindowState(state);
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnStateChanged::kEventName,
      extensions::vivaldi::window_private::OnStateChanged::Create(id(),
                                                                  window_state),
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

const ui::ThemeProvider* VivaldiBrowserWindow::GetThemeProvider() const {
  return &ThemeService::GetThemeProviderForProfile(browser_->profile());
}

const ui::ColorProvider* VivaldiBrowserWindow::GetColorProvider() const {
  return nullptr;
}

ui::ElementContext VivaldiBrowserWindow::GetElementContext() {
  return {};
}

int VivaldiBrowserWindow::GetTopControlsHeight() const {
  return 0;
}

void VivaldiBrowserWindow::OnTabDetached(content::WebContents* contents, bool was_active) {
  extensions::VivaldiRootDocumentHandler* rootdocument_handler =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          browser_->profile());

  rootdocument_handler->InfoBarContainer()->ChangeInfoBarManager(nullptr);
}

sharing_hub::SharingHubBubbleView* VivaldiBrowserWindow::ShowSharingHubBubble(
    share::ShareAttempt attempt) {
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
#if BUILDFLAG(IS_WIN)
  // This is based on BrowserView::IsOnCurrentWorkspace()
  gfx::NativeWindow native_win = GetNativeWindow();
  if (!native_win)
    return true;

  if (base::win::GetVersion() < base::win::Version::WIN10)
    return true;

  Microsoft::WRL::ComPtr<IVirtualDesktopManager> virtual_desktop_manager;
  if (!SUCCEEDED(::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr,
                                    CLSCTX_ALL,
                                    IID_PPV_ARGS(&virtual_desktop_manager)))) {
    return true;
  }

  // Assume the current desktop if IVirtualDesktopManager fails.
  if (gfx::IsWindowOnCurrentVirtualDesktop(
          native_win->GetHost()->GetAcceleratedWidget(),
          virtual_desktop_manager) == false) {
    return false;
  }
#endif
  return true;
}

void VivaldiBrowserWindow::UpdatePageActionIcon(PageActionIconType type) {
  if (type == PageActionIconType::kManagePasswords) {
    // contents can be null when we recover after UI process crash.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    if (web_contents) {
      ManagePasswordsUIController::FromWebContents(web_contents)
          ->UpdateIconAndBubbleState(interface_helper_.get());
    }
  }
}

autofill::AutofillBubbleHandler*
VivaldiBrowserWindow::GetAutofillBubbleHandler() {
  return interface_helper_.get();
}

sharing_hub::ScreenshotCapturedBubble*
VivaldiBrowserWindow::ShowScreenshotCapturedBubble(
    content::WebContents* contents,
    const gfx::Image& image) {
  return nullptr;
}

qrcode_generator::QRCodeGeneratorBubbleView*
VivaldiBrowserWindow::ShowQRCodeGeneratorBubble(content::WebContents* contents,
                                                const GURL& url,
                                                bool show_back_button) {
  // This is called if the user uses the page context menu to generate a QR
  // code.
  vivaldi::BroadcastEvent(
      extensions::vivaldi::utilities::OnShowQRCode::kEventName,
      extensions::vivaldi::utilities::OnShowQRCode::Create(
        url.spec()), browser_->profile());

  auto *bubble_controler =
    qrcode_generator::QRCodeGeneratorBubbleController::Get(contents);

  DCHECK(bubble_controler);
  if (bubble_controler) {
    // This function doesn't actually open the bubble. It just broadcasts
    // the event and javascript opens the bubble.
    //
    // This makes the "C++ bubble" think, it's closed.
    bubble_controler->GetOnBubbleClosedCallback().Run();
  }
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
VivaldiBrowserWindow::ShowSendTabToSelfDevicePickerBubble(
    content::WebContents* contents) {
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
VivaldiBrowserWindow::ShowSendTabToSelfPromoBubble(
    content::WebContents* contents,
    bool show_signin_button) {
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
  // Set initial focus to the ui-document. Was VB-100061, and friends.
  web_contents()->Focus();
}

std::unique_ptr<content::EyeDropper> VivaldiBrowserWindow::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return ShowEyeDropper(frame, listener);
}

user_education::FeaturePromoController*
VivaldiBrowserWindow::GetFeaturePromoControllerImpl() {
  return nullptr;
}

bool VivaldiBrowserWindow::IsFeaturePromoActive(
    const base::Feature& iph_feature) const {
  return false;
}

user_education::FeaturePromoResult VivaldiBrowserWindow::CanShowFeaturePromo(
    const base::Feature& iph_feature) const {
  return user_education::FeaturePromoResult::Failure::kFeatureDisabled;
}

bool VivaldiBrowserWindow::AbortFeaturePromo(const base::Feature& iph_feature) {
  return false;
}

user_education::FeaturePromoHandle
VivaldiBrowserWindow::CloseFeaturePromoAndContinue(
    const base::Feature& iph_feature) {
  return {};
}

bool VivaldiBrowserWindow::NotifyFeaturePromoFeatureUsed(
    const base::Feature& feature,
    FeaturePromoFeatureUsedAction action) {
  return false;
}

user_education::DisplayNewBadge VivaldiBrowserWindow::MaybeShowNewBadgeFor(
    const base::Feature& feature) {
  return user_education::DisplayNewBadge();
}

void VivaldiBrowserWindow::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  if (with_native_frame_) {
    // The system handles the drag.
    return;
  }

  // This is based on RawDraggableRegionsToSkRegion from
  // chromium/extensions/browser/app_window/app_window.cc
  draggable_region_ = std::make_unique<SkRegion>();
  for (auto& region: regions) {
    draggable_region_->op(
        SkIRect::MakeLTRB(region->bounds.x(), region->bounds.y(),
                          region->bounds.right(), region->bounds.bottom()),
        region->draggable ? SkRegion::kUnion_Op : SkRegion::kDifference_Op);
  }

  OnViewWasResized();
}

void VivaldiBrowserWindow::UpdateMaximizeButtonPosition(const gfx::Rect& rect) {
  maximize_button_bounds_ = rect;
}

bool VivaldiBrowserWindow::IsBorderlessModeEnabled() const {
  return false;
}

void VivaldiBrowserWindow::OnCanResizeFromWebAPIChanged() {
  // NOTE(andre@vivaldi.com) : Lifted from BrowserView.
  // TODO(laurila, crbug.com/1493617): Support multi-tab apps.
  // The value can only be set in web apps, where there currently can only be 1
  // WebContents, the return value can be determined only by looking at the
  // value set by the active WebContents' primary page.
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents || !web_contents->GetPrimaryMainFrame() || !widget_) {
    return;
  }

  auto can_resize = web_contents->GetPrimaryPage().GetResizable();
  if (cached_can_resize_from_web_api_ == can_resize) {
    return;
  }

  // Setting it to std::nullopt should never be blocked.
  if (can_resize.has_value() && browser()->tab_strip_model()->count() > 1) {
    // This adds a warning to the active tab, even when another tab makes the
    // call, which also needs to be fixed as part of the multi-apps support.
    web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning,
        base::StringPrintf("window.setResizable blocked due to being called "
                           "from a multi-tab browser."));
    return;
  }

  cached_can_resize_from_web_api_ = can_resize;
  widget_->OnSizeConstraintsChanged();
}

bool VivaldiBrowserWindow::GetCanResize() {
  // Will change in the future to handle multi-tab windows.
  // crbug.com/1493617 & SetCanResizeFromWebAPI.
  bool can_ever_resize = widget_->widget_delegate()
                             ? widget_->widget_delegate()->CanResize()
                             : false;

  return can_ever_resize && GetCanResizeFromWebAPI().value_or(true);
}

std::optional<bool> VivaldiBrowserWindow::GetCanResizeFromWebAPI() const {
  // TODO(laurila, crbug.com/1493617): Support multi-tab apps.
  if (browser()->tab_strip_model()->count() > 1) {
    return std::nullopt;
  }

  // The value can only be set in web apps, where there currently can only be 1
  // WebContents, the return value can be determined only by looking at the
  // value set by the active WebContents' primary page.
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents || !web_contents->GetPrimaryMainFrame()) {
    return std::nullopt;
  }

  return web_contents->GetPrimaryPage().GetResizable();
}

ui::mojom::WindowShowState VivaldiBrowserWindow::GetWindowShowState() const {
  if (IsMaximized()) {
    return ui::mojom::WindowShowState::kMaximized;
  } else if (IsMinimized()) {
    return ui::mojom::WindowShowState::kMinimized;
  } else if (IsFullscreen()) {
    return ui::mojom::WindowShowState::kFullscreen;
  } else {
    return ui::mojom::WindowShowState::kDefault;
  }
}

views::WebView* VivaldiBrowserWindow::GetContentsWebView() {
  // TODO: return correct value
  return nullptr;
}

BrowserView* VivaldiBrowserWindow::AsBrowserView() {
  return nullptr;
}

void VivaldiBrowserWindow::BeforeUnloadFired(content::WebContents* source) {
  // web_contents_delegate_ calls back when unload has fired and we can self
  // destruct. Note we cannot destruct here since cleanup is done.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&VivaldiBrowserWindow::DeleteThis,
                                base::Unretained(this)));
}

void VivaldiBrowserWindow::ShowToast(
    const ToastSpecification* spec,
    std::vector<std::u16string> body_string_replacement_params) {
  extensions::vivaldi::window_private::ToastParameters params;
  std::u16string body = l10n_util::GetStringFUTF16(spec->body_string_id(), body_string_replacement_params, nullptr);
  params.body = base::UTF16ToUTF8(body);

  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::window_private::OnToastMessage::kEventName,
      extensions::vivaldi::window_private::OnToastMessage::Create(id(), params),
      browser_->profile());
}

/*********************/

VivaldiToolbarButtonProvider::VivaldiToolbarButtonProvider(
    VivaldiBrowserWindow* window)
    : window_(window) {}

VivaldiToolbarButtonProvider::~VivaldiToolbarButtonProvider() {
  window_ = nullptr;
}

ExtensionsToolbarContainer*
VivaldiToolbarButtonProvider::GetExtensionsToolbarContainer() {
  return nullptr;
}

gfx::Size VivaldiToolbarButtonProvider::GetToolbarButtonSize() const {
  const int size = 48;
  return gfx::Size(size, size);
}

views::View*
VivaldiToolbarButtonProvider::GetDefaultExtensionDialogAnchorView() {
  return window_->GetWebView();
}

PageActionIconView* VivaldiToolbarButtonProvider::GetPageActionIconView(
    PageActionIconType type) {
  return static_cast<PageActionIconView*>(window_->GetWebView());
}

AppMenuButton* VivaldiToolbarButtonProvider::GetAppMenuButton() {
  return nullptr;
}

gfx::Rect VivaldiToolbarButtonProvider::GetFindBarBoundingBox(
    int contents_bottom) {
  return gfx::Rect();
}

void VivaldiToolbarButtonProvider::FocusToolbar() {}

views::AccessiblePaneView*
VivaldiToolbarButtonProvider::GetAsAccessiblePaneView() {
  return nullptr;
}

views::View* VivaldiToolbarButtonProvider::GetAnchorView(
    std::optional<PageActionIconType> type) {
  // Return the webview.
  return window_->GetWebView();
}

void VivaldiToolbarButtonProvider::ZoomChangedForActiveTab(
    bool can_show_bubble) {}

AvatarToolbarButton* VivaldiToolbarButtonProvider::GetAvatarToolbarButton() {
  return nullptr;
}

ToolbarButton* VivaldiToolbarButtonProvider::GetBackButton() {
  return nullptr;
}

ReloadButton* VivaldiToolbarButtonProvider::GetReloadButton() {
  return nullptr;
}

IntentChipButton* VivaldiToolbarButtonProvider::GetIntentChipButton() {
  return nullptr;
}

DownloadToolbarButtonView* VivaldiToolbarButtonProvider::GetDownloadButton() {
  return nullptr;
}

//SidePanelToolbarButton* VivaldiToolbarButtonProvider::GetSidePanelButton() {
//  return nullptr;
//}
