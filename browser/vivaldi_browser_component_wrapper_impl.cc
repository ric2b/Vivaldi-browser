// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "base/logging.h"
#include "browser/startup_vivaldi_browser.h"
#include "browser/translate/vivaldi_translate_client.h"
#include "browser/vivaldi_browser_finder.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_about_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/mixed_content_settings_tab_helper.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/external_install_error_desktop.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_collector.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/views/eye_dropper/eye_dropper.h"
#include "chrome/browser/ui/views/tab_dialogs_views.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/content_settings/common/content_settings_agent.mojom.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"

#include "components/history/core/browser/top_sites.h"

#include "components/sessions/core/tab_restore_service.h"

#include "components/tabs/public/tab_interface.h"
#include "components/tabs/tab_helpers.h"

#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"

#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_frame_host.h"

#include "extensions/api/tabs/tabs_private_api.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/commands/commands_handler.h"
#include "extensions/common/api/extension_action/action_info.h"

#include "extensions/schema/browser_action_utilities.h"
#include "extensions/schema/window_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "extensions/vivaldi_associated_tabs.h"

#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

#include "ui/devtools/devtools_connector.h"
#include "ui/vivaldi_browser_window.h"
#include "ui/vivaldi_rootdocument_handler.h"
#include "ui/vivaldi_ui_utils.h"

#include "ui/window_registry_service.h"

#include "chrome/browser/send_tab_to_self/receiving_ui_handler.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "components/send_tab_to_self/send_tab_to_self_bridge.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"

// Note; |vivaldi_browser_component_wrapper_impl| needs to be included last
// because trickery.
#include "browser/vivaldi_browser_component_wrapper_impl.h"

/*static*/ VivaldiBrowserComponentWrapperImpl* wrapper_impl_ = nullptr;

// Helpers

namespace {

void SetAllowRunningInsecureContent(content::RenderFrameHost* frame) {
  mojo::AssociatedRemote<content_settings::mojom::ContentSettingsAgent>
      renderer;
  frame->GetRemoteAssociatedInterfaces()->GetInterface(&renderer);
  renderer->SetAllowRunningInsecureContent();
}

}  // namespace

VivaldiBrowserComponentWrapperImpl::VivaldiBrowserComponentWrapperImpl() {}

VivaldiBrowserComponentWrapperImpl::~VivaldiBrowserComponentWrapperImpl() {}

/* static */ void VivaldiBrowserComponentWrapper::CreateImpl() {
  if (!wrapper_impl_) {
    wrapper_impl_ = new VivaldiBrowserComponentWrapperImpl();
  }
  VivaldiBrowserComponentWrapper::SetInstance(wrapper_impl_);
}

/////////*ContentSettingChangedBridgeImpl*///////

ContentSettingChangedBridgeImpl::ContentSettingChangedBridgeImpl() {}

ContentSettingChangedBridgeImpl::~ContentSettingChangedBridgeImpl() {}

void VivaldiBrowserComponentWrapperImpl::AddContentSettingChangeObserver(
    content::BrowserContext* context,
    ContentSettingChangedBridge::Observer* observer) {
  raw_ptr<ContentSettingChangedBridgeImpl> observerimpl =
      profile_content_bridge_impl_[context];

  if (!observerimpl) {
    observerimpl = new ContentSettingChangedBridgeImpl();
    // Store it away for bookeeping.
    profile_content_bridge_impl_[context] = observerimpl;
  }

  observerimpl->AddObserver(observer);

  const raw_ptr<HostContentSettingsMap> host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(context);

  observerimpl->StartObserving(host_content_settings_map);
}

void VivaldiBrowserComponentWrapperImpl::RemoveContentSettingChangeObserver(
    content::BrowserContext* context,
    ContentSettingChangedBridge::Observer* observer) {
  ContentSettingChangedBridgeImpl *observerimpl =
      profile_content_bridge_impl_[context];

  CHECK(observerimpl);

  observerimpl->RemoveObserver(observer);
  observerimpl->StopObserving();

  if (!observerimpl->HasObservers()) {
    profile_content_bridge_impl_.erase(context);
    delete observerimpl;
  }
}

void ContentSettingChangedBridgeImpl::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  int content_type_enum = static_cast<int>(content_type);

  for (ContentSettingChangedBridge::Observer& observer : observers_) {
    observer.OnContentSettingChanged(primary_pattern, secondary_pattern,
                                     content_type_enum);
  }
}

void TabResourceUsageCollectorBridgeImpl::OnTabResourceMetricsRefreshed() {
  for (VivaldiBrowserComponentWrapperImpl::TabResourceUsageCollectorBridge::
           Observer& observer : observers_) {
    observer.OnTabResourceMetricsRefreshed();
  }
}

void VivaldiBrowserComponentWrapperImpl::AddExtensionActionDispatcherObserver(
    content::BrowserContext* context,
    ExtensionActionDispatcherBridge::Observer* observer) {
  raw_ptr<ExtensionActionDispatcherBridgeImpl> observerimpl =
      extension_action_dispatcher_bridge_impl_[context];

  if (!observerimpl) {
    observerimpl = new ExtensionActionDispatcherBridgeImpl();
    // Store it away for bookeeping.
    extension_action_dispatcher_bridge_impl_[context] = observerimpl;
  }

  observerimpl->AddObserver(context, observer);
}

void VivaldiBrowserComponentWrapperImpl::
    RemoveExtensionActionDispatcherObserver(
        content::BrowserContext* context,
        ExtensionActionDispatcherBridge::Observer* observer) {
  ExtensionActionDispatcherBridgeImpl *observerimpl =
      extension_action_dispatcher_bridge_impl_[context];

  CHECK(observerimpl);

  observerimpl->RemoveObserver(context, observer);

  if (!observerimpl->HasObservers()) {
    extension_action_dispatcher_bridge_impl_.erase(context);
    delete observerimpl;
  }
}

void ExtensionActionDispatcherBridgeImpl::OnExtensionActionUpdated(
    extensions::ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  for (VivaldiBrowserComponentWrapperImpl::ExtensionActionDispatcherBridge::
           Observer& observer : observers_) {
    observer.OnExtensionActionUpdated(extension_action, web_contents,
                                      browser_context);
  }
}

// ***********************
// External methods below.

int VivaldiBrowserComponentWrapperImpl::BrowserListGetCount() {
  return BrowserList::GetInstance()->size();
}

bool VivaldiBrowserComponentWrapperImpl::BrowserListHasActive() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->window() && browser->window()->IsActive()) {
      return true;
    }
  }
  return false;
}

void VivaldiBrowserComponentWrapperImpl::BrowserListInitVivaldiCommandState() {
  for (Browser* browser : *BrowserList::GetInstance()) {
    chrome::BrowserCommandController* command_controller =
        browser->command_controller();
    command_controller->InitVivaldiCommandState();
  }
}

Browser* VivaldiBrowserComponentWrapperImpl::FindBrowserWithTab(
    content::WebContents* tab) {
  return chrome::FindBrowserWithTab(tab);
}

Browser* VivaldiBrowserComponentWrapperImpl::FindBrowserWithWindowId(
    int window_id) {
  BrowserList* list = BrowserList::GetInstance();
  for (size_t i = 0; i < list->size(); i++) {
    if (list->get(i)->session_id().id() == window_id) {
      return list->get(i);
    }
  }
  return nullptr;
}

Browser* VivaldiBrowserComponentWrapperImpl::FindLastActiveBrowserWithProfile(
    Profile* profile) {
  return chrome::FindLastActiveWithProfile(profile);
}

void VivaldiBrowserComponentWrapperImpl::BrowserDoCloseContents(
    content::WebContents* tab) {
  Browser* browser = chrome::FindBrowserWithTab(tab);
  if (browser) {
    browser->DoCloseContents(tab);
  }
}

Browser* VivaldiBrowserComponentWrapperImpl::FindBrowserForEmbedderWebContents(
    content::WebContents* web_contents) {
  return vivaldi::FindBrowserForEmbedderWebContents(web_contents);
}

void VivaldiBrowserComponentWrapperImpl::ShowExtensionErrorDialog(
    Browser* browser,
    extensions::ExternalInstallError* error) {
  static_cast<extensions::ExternalInstallErrorDesktop*>(error)->ShowDialog(
      browser);
}

void VivaldiBrowserComponentWrapperImpl::EnsureTabDialogsCreated(
    content::WebContents* web_contents) {
  if (!TabDialogs::FromWebContents(web_contents)) {
    TabDialogs::CreateForWebContents(web_contents);
  }
}

content::WebContents* VivaldiBrowserComponentWrapperImpl::BrowserAddNewContents(
    Browser* browser,
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  return browser->AddNewContentsVivaldi(
      source, std::move(new_contents), target_url, disposition, window_features,
      user_gesture, was_blocked);
}

content::WebContents*
VivaldiBrowserComponentWrapperImpl::WebViewGuestOpenUrlFromTab(
    content::WebContents* guest_webcontents,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // NOTE(pettern@vivaldi.com): Fix for VB-43122. Let devtools handle opening
  // links from devtools.
  DevToolsWindow* window = DevToolsWindow::AsDevToolsWindow(guest_webcontents);
  if (window) {
    return window->OpenURLFromTab(source, params,
                                  /*navigation_handle_callback=*/{});
  }

  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());

  if (params.disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }

  Browser* browser = chrome::FindBrowserWithTab(source);
  if (!browser || params.disposition == WindowOpenDisposition::OFF_THE_RECORD) {
    browser = chrome::FindTabbedBrowser(profile, false);
  }

  if (!browser && params.disposition != WindowOpenDisposition::OFF_THE_RECORD) {
    // This is triggered from embedded content not in a tab. I.e. a mailview
    // or extension browser action popup. Was added via VB-112248.
    browser = ::vivaldi::FindBrowserWithNonTabContent(source);
  }

  if (!browser && Browser::GetCreationStatusForProfile(profile) ==
                      Browser::CreationStatus::kOk) {
    browser =
        Browser::Create(Browser::CreateParams(profile, params.user_gesture));
  }

  NavigateParams nav_params(browser, params.url, params.transition);

  nav_params.FillNavigateParamsFromOpenURLParams(params);
  nav_params.source_contents = source;
  nav_params.tabstrip_add_types = AddTabTypes::ADD_NONE;
  nav_params.should_create_guestframe = true;
  if (params.user_gesture) {
    nav_params.window_action = NavigateParams::SHOW_WINDOW;
  }

  if (params.disposition != WindowOpenDisposition::CURRENT_TAB) {
    // Navigate assumes target_contents has already been navigated.
    content::NavigationController::LoadURLParams load_url_params(
        nav_params.url);

    load_url_params.initiator_frame_token = nav_params.initiator_frame_token;
    load_url_params.initiator_process_id = nav_params.initiator_process_id;
    load_url_params.initiator_origin = nav_params.initiator_origin;
    load_url_params.initiator_base_url = nav_params.initiator_base_url;
    load_url_params.source_site_instance = nav_params.source_site_instance;
    load_url_params.referrer = nav_params.referrer;
    load_url_params.frame_name = nav_params.frame_name;
    load_url_params.frame_tree_node_id = nav_params.frame_tree_node_id;
    load_url_params.redirect_chain = nav_params.redirect_chain;
    load_url_params.transition_type = nav_params.transition;
    load_url_params.extra_headers = nav_params.extra_headers;
    load_url_params.should_replace_current_entry =
        nav_params.should_replace_current_entry;
    load_url_params.is_renderer_initiated = nav_params.is_renderer_initiated;
    load_url_params.started_from_context_menu =
        nav_params.started_from_context_menu;
    load_url_params.has_user_gesture = nav_params.user_gesture;
    load_url_params.blob_url_loader_factory =
        nav_params.blob_url_loader_factory;
    load_url_params.input_start = nav_params.input_start;
    load_url_params.was_activated = nav_params.was_activated;
    load_url_params.href_translate = nav_params.href_translate;
    load_url_params.reload_type = nav_params.reload_type;
    load_url_params.impression = nav_params.impression;
    load_url_params.suggested_system_entropy =
        nav_params.suggested_system_entropy;

    if (nav_params.post_data) {
      load_url_params.load_type =
          content::NavigationController::LOAD_TYPE_HTTP_POST;
      load_url_params.post_data = nav_params.post_data;
    }

    // Create new webcontents and navigate this.
    scoped_refptr<content::SiteInstance>
        initial_site_instance_for_new_contents =
            tab_util::GetSiteInstanceForNewTab(browser->profile(), params.url);

    content::WebContents::CreateParams webcontents_create_params(
        browser->profile(), initial_site_instance_for_new_contents);

    // Filter out data that must not be shared between profiles while loading.
    Profile* navigation_profile = browser->profile();
    if (nav_params.source_site_instance) {
      navigation_profile = Profile::FromBrowserContext(
          nav_params.source_site_instance->GetBrowserContext());
    }
    if (nav_params.source_contents) {
      navigation_profile = Profile::FromBrowserContext(
          nav_params.source_contents->GetBrowserContext());
    }

    // A tab is being opened from a link from a different profile, we must
    // reset source information that may cause state to be shared.
    if (navigation_profile != browser->profile()) {
      nav_params.opener = nullptr;
      nav_params.source_contents = nullptr;
      nav_params.source_site_instance = nullptr;
      nav_params.referrer = content::Referrer();

      load_url_params.source_site_instance = nullptr;
      load_url_params.referrer = content::Referrer();

      webcontents_create_params.opener_render_frame_id = MSG_ROUTING_NONE;
      webcontents_create_params.opener_render_process_id =
          content::ChildProcessHost::kInvalidUniqueID;

      load_url_params.load_type =
          content::NavigationController::LOAD_TYPE_DEFAULT;
      load_url_params.post_data.reset();
    }

    if (params.disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
      webcontents_create_params.initially_hidden = true;
    }

#if defined(USE_AURA)
    if (browser->window() && browser->window()->GetNativeWindow()) {
      webcontents_create_params.context = browser->window()->GetNativeWindow();
    }
#endif

    webcontents_create_params.always_create_guest = true;

    std::unique_ptr<content::WebContents> target_contents =
        content::WebContents::Create(webcontents_create_params);

    // |frame_tree_node_id| is invalid for main frame navigations.
    if (params.frame_tree_node_id.is_null()) {
      bool force_no_https_upgrade =
          nav_params.url_typed_with_http_scheme ||
          nav_params.captive_portal_window_type !=
              captive_portal::CaptivePortalWindowType::kNone;
      std::unique_ptr<ChromeNavigationUIData> navigation_ui_data =
          ChromeNavigationUIData::CreateForMainFrameNavigation(
              target_contents.get(),
              nav_params.is_using_https_as_default_scheme,
              force_no_https_upgrade);
      navigation_ui_data->set_navigation_initiated_from_sync(
          nav_params.navigation_initiated_from_sync);
      load_url_params.navigation_ui_data = std::move(navigation_ui_data);
    }

    // Attaching the helpers now as they will be attached anyway. Preventing
    // potential crash in WebUI (VB-116726)
    TabHelpers::AttachTabHelpers(target_contents.get());

    target_contents->GetController().LoadURLWithParams(load_url_params);

    nav_params.contents_to_insert = std::move(target_contents);
    // Inserts the navigated contents into the tabstrip of the right browser.
    Navigate(&nav_params);
    return nav_params.navigated_or_inserted_contents;
  } else {
    Navigate(&nav_params);
    return nullptr;
  }
}

bool VivaldiBrowserComponentWrapperImpl::HandleNonNavigationAboutURL(
    const GURL& url) {
  return ::HandleNonNavigationAboutURL(url);
}

int VivaldiBrowserComponentWrapperImpl::GetContentSetting(
    content::WebContents* web_contents,
    const GURL& primary_url,
    const GURL& secondary_url,
    content_settings::mojom::ContentSettingsType content_type) {
  Profile* source_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  return HostContentSettingsMapFactory::GetForProfile(source_profile)
             ->GetContentSetting(primary_url, secondary_url, content_type);
}

void VivaldiBrowserComponentWrapperImpl::SetContentSettingCustomScope(
    content::WebContents* web_contents,
    bool allow,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    content_settings::mojom::ContentSettingsType content_type,
    int /* ContentSetting */ setting) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetContentSettingCustomScope(
          primary_pattern, ContentSettingsPattern::Wildcard(), content_type,
          static_cast<ContentSetting>(allow ? CONTENT_SETTING_ALLOW
                                            : CONTENT_SETTING_BLOCK));
}

void VivaldiBrowserComponentWrapperImpl::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), extension);
}

std::vector<Profile*> VivaldiBrowserComponentWrapperImpl::GetLoadedProfiles() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> active_profiles = profile_manager->GetLoadedProfiles();
  return active_profiles;
}

void VivaldiBrowserComponentWrapperImpl::CloseAllDevtools() {
  extensions::DevtoolsConnectorAPI::CloseAllDevtools();
}

void VivaldiBrowserComponentWrapperImpl::AttemptRestart() {
  chrome::AttemptRestart();
}

void VivaldiBrowserComponentWrapperImpl::UpdateFromSystemSettings(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  blink::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile);
}

std::string VivaldiBrowserComponentWrapperImpl::GetDefaultContentSetting(
    content::BrowserContext* context,
    std::string content_setting) {
  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(content_setting);
  ContentSetting default_setting = ContentSetting::CONTENT_SETTING_ASK;
  Profile* profile = Profile::FromBrowserContext(context)->GetOriginalProfile();

  default_setting = HostContentSettingsMapFactory::GetForProfile(profile)
                        ->GetDefaultContentSetting(content_type, nullptr);

  return content_settings::ContentSettingToString(default_setting);
}

void VivaldiBrowserComponentWrapperImpl::SetDefaultContentSetting(
    content::BrowserContext* context,
    std::string content_string,
    std::string default_string) {
  HostContentSettingsMap* map = HostContentSettingsMapFactory::GetForProfile(
      Profile::FromBrowserContext(context)->GetOriginalProfile());

  ContentSetting setting;
  content_settings::ContentSettingFromString(default_string, &setting);
  map->SetDefaultContentSetting(
      site_settings::ContentSettingsTypeFromGroupName(content_string), setting);
}

void VivaldiBrowserComponentWrapperImpl::SetContentSettingCustomScope(
    content::BrowserContext* context,
    std::string primary_pattern_string,
    std::string secondary_pattern_string,
    std::string content_type_string,
    std::string content_setting_string) {
  Profile* profile = Profile::FromBrowserContext(context);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);

  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString(primary_pattern_string);
  ContentSettingsPattern secondary_pattern =
      secondary_pattern_string.empty()
          ? ContentSettingsPattern::Wildcard()
          : ContentSettingsPattern::FromString(secondary_pattern_string);

  ContentSettingsType content_type =
      site_settings::ContentSettingsTypeFromGroupName(content_type_string);
  ContentSetting setting;
  content_settings::ContentSettingFromString(content_setting_string, &setting);

  map->SetContentSettingCustomScope(primary_pattern, secondary_pattern,
                                    content_type, setting);
}

Browser* VivaldiBrowserComponentWrapperImpl::GetWorkspaceBrowser(
    const double workspace_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == nullptr) {
      continue;
    }
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      auto tabWorkspaceId =
          ::vivaldi::GetTabWorkspaceId(web_contents->GetVivExtData());
      if (tabWorkspaceId.has_value() &&
          workspace_id == tabWorkspaceId.value()) {
        return browser;
      }
    }
  }
  return nullptr;
}

int VivaldiBrowserComponentWrapperImpl::CountTabsInWorkspace(
    TabStripModel* tab_strip,
    const double workspace_id) {
  int counter = 0;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
    auto tabWorkspaceId =
        ::vivaldi::GetTabWorkspaceId(web_contents->GetVivExtData());
    if (tabWorkspaceId.has_value() && workspace_id == tabWorkspaceId.value()) {
      counter++;
    }
  }
  return counter;
}

VivaldiBrowserWindow*
VivaldiBrowserComponentWrapperImpl::FindWindowForEmbedderWebContents(
    content::WebContents* web_contents) {
  return ::vivaldi::FindWindowForEmbedderWebContents(web_contents);
}

VivaldiBrowserWindow*
VivaldiBrowserComponentWrapperImpl::VivaldiBrowserWindowFromId(int id) {
  return VivaldiBrowserWindow::FromId(id);
}

VivaldiBrowserWindow*
VivaldiBrowserComponentWrapperImpl::VivaldiBrowserWindowFromBrowser(
    Browser* browser) {
  return VivaldiBrowserWindow::FromBrowser(browser);
}

int VivaldiBrowserComponentWrapperImpl::WindowPrivateCreate(
    Profile* profile,
    extensions::vivaldi::window_private::WindowType param_window_type,
    const VivaldiBrowserWindowParams& window_params,
    const gfx::Rect& window_bounds,
    const std::string& window_key,
    const std::string& viv_ext_data,
    const std::string& tab_url,
    base::OnceCallback<void(VivaldiBrowserWindow* window)> callback) {
  VivaldiBrowserWindow* window =
      VivaldiBrowserComponentWrapper::GetInstance()
          ->WindowRegistryServiceGetNamedWindow(profile, window_key);
  if (window) {
    window->Activate();
    return window->id();
  } else {
    window = new VivaldiBrowserWindow();
  }

  if (!window_key.empty()) {
    window->SetWindowKey(window_key);
    vivaldi::WindowRegistryService::Get(profile)->AddWindow(window, window_key);
  }
  // Delay sending the response until the newly created window has finished its
  // navigation or was closed during that process.
  window->SetDidFinishNavigationCallback(std::move(callback));

  Browser::Type window_type = Browser::TYPE_NORMAL;
  // Popup and settingswindow should open as popup and not stored in session.
  if (param_window_type ==
          extensions::vivaldi::window_private::WindowType::kPopup ||
      param_window_type ==
          extensions::vivaldi::window_private::WindowType::kSettings) {
    window_type = Browser::TYPE_POPUP;
  } else if (param_window_type ==
             extensions::vivaldi::window_private::WindowType::kDevtools) {
    window_type = Browser::TYPE_DEVTOOLS;
  }

  Browser::CreateParams create_params(window_type, profile, false);

  create_params.initial_bounds = window_bounds;

  create_params.creation_source = Browser::CreationSource::kStartupCreator;
  create_params.is_vivaldi = true;
  create_params.window = window;
  create_params.viv_ext_data = viv_ext_data;
#if BUILDFLAG(IS_WIN)
  // see VB-109884
  create_params.initial_show_state = window_params.state;
#endif
  std::unique_ptr<Browser> browser(Browser::Create(create_params));
  DCHECK(browser->window() == window);
  window->SetWindowURL(window_params.resource_relative_url);
  window->CreateWebContents(std::move(browser), window_params);

  if (!tab_url.empty()) {
    content::OpenURLParams urlparams(GURL(tab_url), content::Referrer(),
                                     WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                     ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false);
    window->browser()->OpenURL(urlparams, /* navigation_handle = */ {});
  }

  // TODO(pettern): If we ever need to open unfocused windows, we need to
  // add a new method for open delayed and unfocused.
  //  window->Show(focused ? AppWindow::SHOW_ACTIVE : AppWindow::SHOW_INACTIVE);

  return 0;  // Not yet ready, will callback later
}

Browser* VivaldiBrowserComponentWrapperImpl::FindBrowserByWindowId(
    int32_t window_id) {
  return ::vivaldi::FindBrowserByWindowId(window_id);
}

bool VivaldiBrowserComponentWrapperImpl::IsOutsideAppWindow(int screen_x,
                                                            int screen_y) {
  return ::vivaldi::ui_tools::IsOutsideAppWindow(screen_x, screen_y);
}

content::WebContents*
VivaldiBrowserComponentWrapperImpl::FindActiveTabContentsInThisProfile(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator browser_iterator =
           browser_list->begin_browsers_ordered_by_activation();
       browser_iterator != browser_list->end_browsers_ordered_by_activation();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;
    // TODO: Make this into an utility-method.
    bool is_vivaldi_settings =
        (browser->is_vivaldi() &&
         static_cast<VivaldiBrowserWindow*>(browser->window())->type() ==
             VivaldiBrowserWindow::WindowType::SETTINGS);
    if (browser->profile()->GetOriginalProfile() == profile &&
        !is_vivaldi_settings) {
      return browser->tab_strip_model()->GetActiveWebContents();
    }
  }
  return nullptr;
}

void VivaldiBrowserComponentWrapperImpl::UpdateMuting(
    content::WebContents* active_web_contents,
    vivaldiprefs::TabsAutoMutingValues mute_rule) {
  RecentlyAudibleHelper* audible_helper =
      active_web_contents
          ? RecentlyAudibleHelper::FromWebContents(active_web_contents)
          : nullptr;
  bool active_is_audible =
      audible_helper ? audible_helper->WasRecentlyAudible() : false;

  Profile* active_profile =
      Profile::FromBrowserContext(active_web_contents->GetBrowserContext());

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() == active_profile) {
      for (int i = 0, tab_count = browser->tab_strip_model()->count();
           i < tab_count; ++i) {
        content::WebContents* tab =
            browser->tab_strip_model()->GetWebContentsAt(i);

        GURL url = tab->GetLastCommittedURL();
        const raw_ptr<HostContentSettingsMap> host_content_settings_map =
            HostContentSettingsMapFactory::GetForProfile(active_profile);

        bool contentsetting_says_mute =
            host_content_settings_map->GetContentSetting(
                url, url, ContentSettingsType::SOUND) == CONTENT_SETTING_BLOCK;

        if (!contentsetting_says_mute && !::vivaldi::IsTabMuted(tab)) {
          bool is_active = (tab == active_web_contents);
          bool mute = (mute_rule != vivaldiprefs::TabsAutoMutingValues::kOff);
          if (mute_rule == vivaldiprefs::TabsAutoMutingValues::kOnlyactive) {
            mute = !is_active;
          } else if (mute_rule ==
                     vivaldiprefs::TabsAutoMutingValues::kPrioritizeactive) {
            // Only unmute background tabs if the active is not audible.
            mute = (active_is_audible && !is_active);
          }
          tab->SetAudioMuted(mute);
        }
      }
    }
  }
}

int VivaldiBrowserComponentWrapperImpl::GetTabId(
    content::WebContents* contents) {
  return extensions::ExtensionTabUtil::GetTabId(contents);
}

int VivaldiBrowserComponentWrapperImpl::GetWindowIdOfTab(
    content::WebContents* contents) {
  return extensions::ExtensionTabUtil::GetWindowIdOfTab(contents);
}

void VivaldiBrowserComponentWrapperImpl::HandleDetachedTabForWebPanel(
    int tab_id) {
  ::vivaldi::HandleDetachedTab(tab_id);
}

content::WebContents*
VivaldiBrowserComponentWrapperImpl::GetWebContentsFromTabStrip(
    content::BrowserContext* browser_context,
    int tab_id,
    std::string* error) {
  return ::vivaldi::ui_tools::GetWebContentsFromTabStrip(
      tab_id, browser_context, error);
}

void VivaldiBrowserComponentWrapperImpl::DoBeforeUnloadFired(
    content::WebContents* web_contents,
    bool proceed,
    bool* proceed_to_fire_unload) {
  Browser* browser = ::vivaldi::FindBrowserWithTab(web_contents);
  DCHECK(browser);
  if (browser) {
    browser->DoBeforeUnloadFired(web_contents, proceed, proceed_to_fire_unload);
  }
}

void VivaldiBrowserComponentWrapperImpl::GetTabPerformanceData(
    content::WebContents* web_contents,
    uint64_t& memory_usage,
    bool& is_discarded) {
  resource_coordinator::TabLifecycleUnitExternal* tab_lifecycle_unit_external =
      resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
          web_contents);

  bool notYetLoaded =
      web_contents->GetUserData(::vivaldi::kVivaldiStartupTabUserDataKey);

  is_discarded = web_contents->WasDiscarded() ||
                 tab_lifecycle_unit_external->GetTabState() ==
                     ::mojom::LifecycleUnitState::DISCARDED ||
                 notYetLoaded;

  if (is_discarded) {
    const auto* const pre_discard_resource_usage =
        performance_manager::user_tuning::UserPerformanceTuningManager::
            PreDiscardResourceUsage::FromWebContents(web_contents);
    memory_usage =
        pre_discard_resource_usage == nullptr
            ? 0
            : pre_discard_resource_usage->memory_footprint_estimate_kb() * 1024;
  } else {
    auto* tab = tabs::TabInterface::MaybeGetFromContents(
        web_contents);
    auto* const resource_tab_helper =
        tab->GetTabFeatures()->resource_usage_helper();
    memory_usage = (resource_tab_helper->GetMemoryUsageInBytes());
  }
}

void VivaldiBrowserComponentWrapperImpl::LoadTabContentsIfNecessary(
    content::WebContents* web_contents) {
  TabStripModel* tab_strip;
  int tab_index;
  ::vivaldi::VivaldiStartupTabUserData* viv_startup_data =
      static_cast<::vivaldi::VivaldiStartupTabUserData*>(
          web_contents->GetUserData(::vivaldi::kVivaldiStartupTabUserDataKey));

  if (viv_startup_data && extensions::ExtensionTabUtil::GetTabStripModel(
                              web_contents, &tab_strip, &tab_index)) {
    // Check if we need to make a tab active, this must be done when
    // starting with tabs through the commandline or through start with pages.
    if (viv_startup_data && viv_startup_data->start_as_active()) {
      tab_strip->ActivateTabAt(tab_index);
    }
  }
  web_contents->SetUserData(::vivaldi::kVivaldiStartupTabUserDataKey, nullptr);
}

std::vector<tabs::TabAlert>
VivaldiBrowserComponentWrapperImpl::GetTabAlertStatesForContents(
    content::WebContents* web_contents) {
  return ::GetTabAlertStatesForContents(web_contents);
}

std::unique_ptr<translate::TranslateUIDelegate>
VivaldiBrowserComponentWrapperImpl::GetTranslateUIDelegate(
    content::WebContents* web_contents,
    std::string& original_language,
    std::string& target_language) {
  VivaldiTranslateClient* client =
      VivaldiTranslateClient::FromWebContents(web_contents);

  translate::TranslateManager* manager = client->GetTranslateManager();

  return std::make_unique<translate::TranslateUIDelegate>(
      manager->GetWeakPtr(), original_language, target_language);
}

void VivaldiBrowserComponentWrapperImpl::RevertTranslation(
    content::WebContents* web_contents) {
  VivaldiTranslateClient* client =
      VivaldiTranslateClient::FromWebContents(web_contents);
  client->GetTranslateManager()->RevertTranslation();
}

void VivaldiBrowserComponentWrapperImpl::ActivateWebContentsInTabStrip(
    content::WebContents* web_contents) {
  Browser* browser = ::vivaldi::FindBrowserWithTab(web_contents);
  DCHECK(browser);
  if (!browser) {
    return;
  }
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(web_contents);
  if (index != TabStripModel::kNoTab) {
    tab_strip->ActivateTabAt(index);
  }
}

bool VivaldiBrowserComponentWrapperImpl::ShowGlobalError(
    content::BrowserContext* browser_context,
    int command_id,
    int window_id) {
  extensions::VivaldiRootDocumentHandler* root_doc_handler =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          browser_context);

  GlobalError* error =
      root_doc_handler->GetGlobalErrorByMenuItemCommandID(command_id);

  Browser* browser =
      VivaldiBrowserComponentWrapper::GetInstance()->FindBrowserByWindowId(
          window_id);
  if (!error || !browser) {
    return false;
  }

  error->ShowBubbleView(browser);

  return true;
}

bool VivaldiBrowserComponentWrapperImpl::GetGlobalErrors(
    content::BrowserContext* browser_context,
    std::vector<ExtensionInstallError*>& jserrors) {
  extensions::VivaldiRootDocumentHandler* root_doc_handler =
      extensions::VivaldiRootDocumentHandlerFactory::GetForBrowserContext(
          browser_context);

  for (std::unique_ptr<extensions::VivaldiExtensionDisabledGlobalError>& error :
       root_doc_handler->errors()) {
    DCHECK(error);
    extensions::vivaldi::extension_action_utils::ExtensionInstallError*
        jserror = new extensions::vivaldi::extension_action_utils::
            ExtensionInstallError();

    // Note extensions can appear multiple times here because of how we add
    // ExtensionDisabledGlobalError errors.
    jserror->id = error->GetExtensionId();
    jserror->name = error->GetExtensionName();
    jserror->error_type = extensions::vivaldi::extension_action_utils::
        GlobalErrorType::kInstalled;
    jserror->command_id =
        root_doc_handler->GetExtensionToIdProvider().AddOrGetId(
            error->GetExtension()->id());

    jserrors.push_back(reinterpret_cast<ExtensionInstallError*>(jserror));
  }

  return true;
}

void VivaldiBrowserComponentWrapperImpl::AddGuestToTabStripModel(
    content::WebContents* source_content,
    content::WebContents* guest_content,
    int windowId,
    bool activePage,
    bool inherit_opener,
    bool is_extension_host) {
  Browser* browser = ::vivaldi::FindBrowserByWindowId(windowId);

  if (is_extension_host) {
    // This is an extension popup, split mode extensions (incognito) will have a
    // regular profile for the webcontents. So make sure we add the tab to the
    // correct browser.
    content::BrowserContext* context = guest_content->GetBrowserContext();

    Profile* profile = Profile::FromBrowserContext(context);

    browser = chrome::FindTabbedBrowser(profile, false);
    if (!browser) {
      sessions::TabRestoreService* trs =
          TabRestoreServiceFactory::GetForProfile(profile);
      DCHECK(trs);
      // Restores the last closed browser-window including the tabs.
      trs->RestoreMostRecentEntry(nullptr);
      browser = chrome::FindTabbedBrowser(profile, false);
    }
  }

  if (!browser || !browser->window()) {
    if (windowId) {
      NOTREACHED();
      // return;
    }
    // Find a suitable window.
    browser = chrome::FindTabbedBrowser(
        Profile::FromBrowserContext(guest_content->GetBrowserContext()), true);
    if (!browser || !browser->window()) {
      NOTREACHED();
      // return;
    }
  }

  TabStripModel* tab_strip = browser->tab_strip_model();
  content::WebContents* existing_tab =
      tab_strip->count() == 1 ? tab_strip->GetWebContentsAt(0) : nullptr;

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = activePage;
  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = false;
  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = -1;
  index = std::min(std::max(index, -1), 0 /* tab_strip->count()*/);

  int add_types = active ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE;
  add_types |= AddTabTypes::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= AddTabTypes::ADD_PINNED;
  if (inherit_opener) {
    add_types |= AddTabTypes::ADD_INHERIT_OPENER;
  }

  NavigateParams navigate_params(
      browser, std::unique_ptr<content::WebContents>(guest_content));
  navigate_params.disposition = active
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  navigate_params.source_contents = source_content;

  Navigate(&navigate_params);

  if (!browser->is_vivaldi()) {
    if (active)
      navigate_params.navigated_or_inserted_contents->SetInitialFocus();
  }
  if (navigate_params.navigated_or_inserted_contents) {
    content::RenderFrameHost* host =
        navigate_params.navigated_or_inserted_contents->GetPrimaryMainFrame();
    DCHECK(host);
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWindowFeatures(blink::mojom::WindowFeatures().Clone());
  }

  if (existing_tab) {
    // We had a single tab open, check if it's speed dial.
    GURL url = existing_tab->GetURL();
    if (url == GURL(::vivaldi::kVivaldiNewTabURL)) {
      // If it's Speed Dial, close it immediately. New windows always
      // get a Speed Dial tab initially as some extensions expect it.
      tab_strip->CloseWebContentsAt(
          tab_strip->GetIndexOfWebContents(existing_tab), 0);
    }
  }
}

void VivaldiBrowserComponentWrapperImpl::WindowRegistryServiceAddWindow(
    content::BrowserContext* browser_context,
    VivaldiBrowserWindow* window,
    const std::string& window_key) {
  vivaldi::WindowRegistryService::Get(browser_context)
      ->AddWindow(window, window_key);
}

VivaldiBrowserWindow*
VivaldiBrowserComponentWrapperImpl::WindowRegistryServiceGetNamedWindow(
    content::BrowserContext* browser_context,
    const std::string& window_key) {
  return vivaldi::WindowRegistryService::Get(browser_context)
      ->GetNamedWindow(window_key);
}

bool VivaldiBrowserComponentWrapperImpl::ExtensionTabUtilGetTabById(
    int tab_id,
    content::BrowserContext* browser_context,
    bool include_incognito,
    content::WebContents** contents) {
  return extensions::ExtensionTabUtil::GetTabById(tab_id, browser_context,
                                                  include_incognito, contents);
}

bool VivaldiBrowserComponentWrapperImpl::ExtensionTabUtilGetTabById(
    int tab_id,
    content::BrowserContext* browser_context,
    bool include_incognito,
    extensions::WindowController** out_window,
    content::WebContents** contents,
    int* out_tab_index) {
  return extensions::ExtensionTabUtil::GetTabById(tab_id, browser_context,
                                                  include_incognito, out_window,
                                                  contents, out_tab_index);
}

int VivaldiBrowserComponentWrapperImpl::ExtensionTabUtilGetTabId(
    const content::WebContents* contents) {
  return extensions::ExtensionTabUtil::GetTabId(contents);
}

bool VivaldiBrowserComponentWrapperImpl::TopSitesFactoryUpdateNow(
    content::BrowserContext* browser_context) {
  scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!ts)
    return false;

  ts->UpdateNow();

  return false;
}

void VivaldiBrowserComponentWrapperImpl::TopSitesFactoryAddObserver(
    content::BrowserContext* browser_context,
    history::TopSitesObserver* observer) {
  scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (ts)
    ts->AddObserver(observer);
}

void VivaldiBrowserComponentWrapperImpl::TopSitesFactoryRemoveObserver(
    content::BrowserContext* browser_context,
    history::TopSitesObserver* observer) {
  scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (ts)
    ts->RemoveObserver(observer);
}

bookmarks::BookmarkModel*
VivaldiBrowserComponentWrapperImpl::GetBookmarkModelForBrowserContext(
    content::BrowserContext* browser_context) {
  return BookmarkModelFactory::GetForBrowserContext(browser_context);
}

const bookmarks::BookmarkNode*
VivaldiBrowserComponentWrapperImpl::GetBookmarkNodeByID(
    bookmarks::BookmarkModel* model,
    int64_t id) {
  return bookmarks::GetBookmarkNodeByID(model, id);
}

bool VivaldiBrowserComponentWrapperImpl::GetControllerFromWindowID(
    ExtensionFunction* function,
    int window_id,
    extensions::WindowController** out_controller,
    std::string* error) {
  return windows_util::GetControllerFromWindowID(
      function, window_id, extensions::WindowController::GetAllWindowFilter(),
      out_controller, error);
}

void VivaldiBrowserComponentWrapperImpl::LoadViaLifeCycleUnit(
    content::WebContents* web_contents) {
  for (resource_coordinator::LifecycleUnit* lifecycle_unit :
       g_browser_process->GetTabManager()->GetSortedLifecycleUnits()) {
    resource_coordinator::TabLifecycleUnitExternal*
        tab_lifecycle_unit_external =
            lifecycle_unit->AsTabLifecycleUnitExternal();
    if (tab_lifecycle_unit_external->GetWebContents() == web_contents) {
      lifecycle_unit->Load();
      break;
    }
  }
}

bool VivaldiBrowserComponentWrapperImpl::SetTabAudioMuted(
    content::WebContents* web_contents,
    bool mute,
    TabMutedReason reason,
    const std::string& extension_id) {
  return ::SetTabAudioMuted(web_contents, mute, reason, extension_id);
}

extensions::DevtoolsConnectorItem*
VivaldiBrowserComponentWrapperImpl::ConnectDevToolsWindow(
    content::BrowserContext* browser_context,
    int tab_id,
    content::WebContents* inspected_contents,
    content::WebContentsDelegate* delegate) {
  extensions::DevtoolsConnectorAPI* api =
      extensions::DevtoolsConnectorAPI::GetFactoryInstance()->Get(
          browser_context);
  DCHECK(api);

  DevToolsWindow* devWindow =
      DevToolsWindow::GetInstanceForInspectedWebContents(inspected_contents);
  DCHECK(devWindow);
  devWindow->set_guest_delegate(delegate);
  extensions::DevtoolsConnectorItem* item =
      api->GetOrCreateDevtoolsConnectorItem(tab_id);
  DCHECK(item);
  item->set_devtools_delegate(devWindow);
  return item;
}

content::WebContents* VivaldiBrowserComponentWrapperImpl::
    DevToolsWindowGetDevtoolsWebContentsForInspectedWebContents(
        content::WebContents* contents) {
  return DevToolsWindow::GetDevtoolsWebContentsForInspectedWebContents(
      contents);
}

content::WebContents*
VivaldiBrowserComponentWrapperImpl::DevToolsWindowGetInTabWebContents(
    content::WebContents* inspected_web_contents,
    DevToolsContentsResizingStrategy* out_strategy) {
  return DevToolsWindow::GetInTabWebContents(inspected_web_contents,
                                             out_strategy);
}

void VivaldiBrowserComponentWrapperImpl::NavigationStateChanged(
    VivaldiBrowserWindow* window,
    content::WebContents* web_contents,
    int /*content::InvalidateTypes*/ changed_flags) {
  window->NavigationStateChanged(
      web_contents, static_cast<content::InvalidateTypes>(changed_flags));
}

bool VivaldiBrowserComponentWrapperImpl::GetSendTabToSelfContentHasSupport(
    content::WebContents* web_contents) {
  std::optional<send_tab_to_self::EntryPointDisplayReason> displayReason =
      send_tab_to_self::GetEntryPointDisplayReason(web_contents);
  return displayReason.has_value() &&
         displayReason.value() ==
             send_tab_to_self::EntryPointDisplayReason::kOfferFeature;
}

bool VivaldiBrowserComponentWrapperImpl::GetSendTabToSelfModelIsReady(
    Profile* profile) {
  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  return model && model->IsReady();
}

bool VivaldiBrowserComponentWrapperImpl::GetSendTabToSelfReceivedEntries(
    Profile* profile,
    std::vector<SendTabToSelfEntry*>& items) {
  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  syncer::DeviceInfoTracker* device_info_tracker =
      DeviceInfoSyncServiceFactory::GetForProfile(profile)
          ->GetDeviceInfoTracker();
  if (model && model->IsReady() && device_info_tracker) {
    std::vector<std::string> guids = model->GetAllGuids();
    for (auto guid : guids) {
      const send_tab_to_self::SendTabToSelfEntry* entry =
          model->GetEntryByGUID(guid);
      if (entry) {
        if (device_info_tracker->IsRecentLocalCacheGuid(
                entry->GetTargetDeviceSyncCacheGuid()) &&
            !entry->GetNotificationDismissed() && !entry->IsOpened()) {
          extensions::vivaldi::tabs_private::SendTabToSelfEntry* item =
              new extensions::vivaldi::tabs_private::SendTabToSelfEntry();
          item->guid = entry->GetGUID();
          item->url = entry->GetURL().spec();
          item->title = entry->GetTitle();
          item->device_name = entry->GetDeviceName();
          item->shared_time =
              entry->GetSharedTime().InMillisecondsFSinceUnixEpoch();
          items.push_back(reinterpret_cast<SendTabToSelfEntry*>(item));
        }
      }
    }
    return true;
  }
  return false;
}

bool VivaldiBrowserComponentWrapperImpl::DeleteSendTabToSelfReceivedEntries(
    Profile* profile,
    std::vector<std::string> guids) {
  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    for (const std::string& guid : guids) {
      model->DeleteEntry(guid);
    }
    return true;
  }
  return false;
}

bool VivaldiBrowserComponentWrapperImpl::DismissSendTabToSelfReceivedEntries(
    Profile* profile,
    std::vector<std::string> guids) {
  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model->IsReady()) {
    for (const std::string& guid : guids) {
      model->DismissEntry(guid);
    }
    return true;
  }
  return false;
}

bool VivaldiBrowserComponentWrapperImpl::GetSendTabToSelfTargets(
    Profile* profile,
    std::vector<SendTabToSelfTarget*>& items) {
  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model && model->IsReady()) {
    for (auto device : model->GetTargetDeviceInfoSortedList()) {
      extensions::vivaldi::tabs_private::SendTabToSelfTarget* item =
          new extensions::vivaldi::tabs_private::SendTabToSelfTarget();
      item->guid = device.cache_guid;
      item->name = device.full_name;
      if (device.form_factor == syncer::DeviceInfo::FormFactor::kPhone) {
        item->type =
            extensions::vivaldi::tabs_private::SendTabToSelfTargetType::kPhone;
      } else if (device.form_factor ==
                 syncer::DeviceInfo::FormFactor::kTablet) {
        item->type =
            extensions::vivaldi::tabs_private::SendTabToSelfTargetType::kTablet;
      } else {
        item->type = extensions::vivaldi::tabs_private::
            SendTabToSelfTargetType::kDesktop;
      }
      items.push_back(reinterpret_cast<SendTabToSelfTarget*>(item));
    }
    return true;
  }

  return false;
}

bool VivaldiBrowserComponentWrapperImpl::SendTabToSelfAddToModel(
    Profile* profile,
    GURL url,
    std::string title,
    std::string guid) {
  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(profile)
          ->GetSendTabToSelfModel();
  if (model) {
    model->AddEntry(url, title, guid);
  }
  return !!model;
}

void VivaldiBrowserComponentWrapperImpl::HandleRegisterHandlerRequest(
    content::WebContents* web_contents,
    custom_handlers::ProtocolHandler* handler) {
  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (registry->SilentlyHandleRegisterHandlerRequest(*handler)) {
    return;
  }

  auto* page_content_settings_delegate =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents);
  page_content_settings_delegate->set_pending_protocol_handler(*handler);
  page_content_settings_delegate->set_previous_protocol_handler(
      registry->GetHandlerFor(handler->protocol()));
}

void VivaldiBrowserComponentWrapperImpl::SetOrRollbackProtocolHandler(
    content::WebContents* web_contents,
    bool allow) {
  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());

  auto* content_settings =
      PageSpecificContentSettingsDelegate::FromWebContents(web_contents);
  auto pending_handler = content_settings->pending_protocol_handler();

  if (allow) {
    registry->RemoveIgnoredHandler(pending_handler);

    registry->OnAcceptRegisterProtocolHandler(pending_handler);
    PageSpecificContentSettingsDelegate::FromWebContents(web_contents)
        ->set_pending_protocol_handler_setting(CONTENT_SETTING_ALLOW);
  } else {
    registry->OnIgnoreRegisterProtocolHandler(pending_handler);
    PageSpecificContentSettingsDelegate::FromWebContents(web_contents)
        ->set_pending_protocol_handler_setting(CONTENT_SETTING_BLOCK);

    auto previous_handler = content_settings->previous_protocol_handler();
    if (previous_handler.IsEmpty()) {
      registry->ClearDefault(pending_handler.protocol());
    } else {
      registry->OnAcceptRegisterProtocolHandler(previous_handler);
    }
  }
}

extensions::VivaldiPrivateTabObserver*
VivaldiBrowserComponentWrapperImpl::VivaldiPrivateTabObserverFromWebContents(
    content::WebContents* contents) {
  return extensions::VivaldiPrivateTabObserver::FromWebContents(contents);
}

std::string VivaldiBrowserComponentWrapperImpl::GetShortcutText(
    content::BrowserContext* browser_context,
    extensions::ExtensionAction* action) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->GetExtensionById(action->extension_id(),
                             extensions::ExtensionRegistry::ENABLED);

  const extensions::Command* requested_command = nullptr;
  switch (action->action_type()) {
    case extensions::ActionInfo::Type::kAction:
      requested_command = extensions::CommandsInfo::GetActionCommand(extension);
      break;
    case extensions::ActionInfo::Type::kBrowser:
      requested_command =
          extensions::CommandsInfo::GetBrowserActionCommand(extension);
      break;
    case extensions::ActionInfo::Type::kPage:
      requested_command =
          extensions::CommandsInfo::GetPageActionCommand(extension);
      break;
  }

  if (!requested_command) {
    return std::string();
  }

  extensions::CommandService* command_service =
      extensions::CommandService::Get(browser_context);

  extensions::Command saved_command = command_service->FindCommandByName(
      action->extension_id(), requested_command->command_name());
  const ui::Accelerator shortcut_assigned = saved_command.accelerator();

  return ::vivaldi::ShortcutText(shortcut_assigned.key_code(),
                                 shortcut_assigned.modifiers(), 0);
}

bool VivaldiBrowserComponentWrapperImpl::HasBrowserShortcutPriority(
    Profile* profile,
    GURL url) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  return map->GetContentSetting(url, GURL(),
                                ContentSettingsType::KEY_SHORTCUTS) ==
         ContentSetting::CONTENT_SETTING_BLOCK;
}

content::WebContents* VivaldiBrowserComponentWrapperImpl::GetActiveWebContents(
    content::BrowserContext* browser_context,
    int32_t window_id) {
  Browser* browser =
      ::chrome::FindBrowserWithID(SessionID::FromSerializedValue(window_id));
  if (browser) {
    return browser->tab_strip_model()->GetActiveWebContents();
  }
  return nullptr;
}

std::unique_ptr<content::EyeDropper>
VivaldiBrowserComponentWrapperImpl::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  return ShowEyeDropper(frame, listener);
}

content::PictureInPictureResult
VivaldiBrowserComponentWrapperImpl::EnterPictureInPicture(
    content::WebContents* web_contents) {
  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void VivaldiBrowserComponentWrapperImpl::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

void VivaldiBrowserComponentWrapperImpl::ShowRepostFormWarningDialog(
    content::WebContents* source) {
  TabModalConfirmDialog::Create(
      std::make_unique<RepostFormWarningController>(source), source);
}

void VivaldiBrowserComponentWrapperImpl::AllowRunningInsecureContent(
    content::WebContents* web_contents) {
  MixedContentSettingsTabHelper* mixed_content_settings =
      MixedContentSettingsTabHelper::FromWebContents(web_contents);
  if (mixed_content_settings) {
    // Update browser side settings to allow active mixed content.
    mixed_content_settings->AllowRunningOfInsecureContent(
        *web_contents->GetOpener());
  }

  web_contents->ForEachRenderFrameHost(&SetAllowRunningInsecureContent);
}

void VivaldiBrowserComponentWrapperImpl::TaskManagerCreateForTabContents(
    content::WebContents* web_contents) {
  task_manager::WebContentsTags::CreateForTabContents(web_contents);
}

void VivaldiBrowserComponentWrapperImpl::
    PageSpecificContentSettingsCreateForTabContents(
        content::WebContents* web_contents) {
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents,
      std::make_unique<PageSpecificContentSettingsDelegate>(web_contents));
}

void VivaldiBrowserComponentWrapperImpl::CreateWebNavigationTabObserver(
    content::WebContents* web_contents) {
  extensions::WebNavigationTabObserver::CreateForWebContents(web_contents);
}

void VivaldiBrowserComponentWrapperImpl::OpenExtensionOptionPage(
    const extensions::Extension* extension,
    Browser* browser) {
  extensions::ExtensionTabUtil::OpenOptionsPage(extension, browser);
}

const std::vector<std::unique_ptr<extensions::MenuItem>>*
VivaldiBrowserComponentWrapperImpl::GetExtensionMenuItems(
    content::BrowserContext* browser_context,
    std::string id) {
  extensions::MenuManager* manager =
      extensions::MenuManager::Get(browser_context);
  const extensions::MenuItem::OwnedList* all_items =
      manager->MenuItems(extensions::MenuItem::ExtensionKey(id));
  return all_items;
}

bool VivaldiBrowserComponentWrapperImpl::ExecuteCommandMenuItem(
    content::BrowserContext* browser_context,
    std::string extension_id,
    int32_t window_id,
    std::string menu_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    return false;
  }

  Browser* browser =
      ::chrome::FindBrowserWithID(SessionID::FromSerializedValue(window_id));
  if (!browser) {
    return false;
  }
  // TODO: Check incognito here
  bool incognito = browser_context->IsOffTheRecord();
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  extensions::MenuItem::ExtensionKey extension_key(extension->id());
  extensions::MenuItem::Id action_id(incognito, extension_key);
  action_id.string_uid = menu_id;
  extensions::MenuManager* manager =
      extensions::MenuManager::Get(browser_context);
  extensions::MenuItem* item = manager->GetItemById(action_id);
  if (!item) {
    // This means the id might be numerical, so convert it and try
    // again.  We currently don't maintain the type through the
    // layers.
    action_id.string_uid = "";
    base::StringToInt(menu_id, &action_id.uid);

    item = manager->GetItemById(action_id);
    if (!item) {
      return false;
    }
  }
  manager->ExecuteCommand(browser_context, contents, nullptr,
                          content::ContextMenuParams(), action_id);

  return true;
}
