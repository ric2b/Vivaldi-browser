// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "components/browsing_data/storage_partition_http_cache_data_remover.h"
#include "components/guest_view/browser/guest_view_event.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/media_stream_request.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/url_constants.h"

#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
#include "chrome/common/pref_names.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#endif // VIVALDI_BUILD_HAS_CHROME_CODE

using base::UserMetricsAction;
using content::GlobalRequestID;
using content::RenderFrameHost;
using content::ResourceType;
using content::StoragePartition;
using content::WebContents;
using guest_view::GuestViewBase;
using guest_view::GuestViewEvent;
using guest_view::GuestViewManager;
using ui_zoom::ZoomController;

// TODO: andre@vivaldi.com : Move this into some other space than global.
extensions::WebViewGuest* s_current_webviewguest = NULL;

namespace extensions {

namespace {

// Returns storage partition removal mask from web_view clearData mask. Note
// that storage partition mask is a subset of webview's data removal mask.
uint32 GetStoragePartitionRemovalMask(uint32 web_view_removal_mask) {
  uint32 mask = 0;
  if (web_view_removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_APPCACHE)
    mask |= StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  if (web_view_removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_COOKIES)
    mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  if (web_view_removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_FILE_SYSTEMS)
    mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (web_view_removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_INDEXEDDB)
    mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  if (web_view_removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_LOCAL_STORAGE)
    mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (web_view_removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_WEBSQL)
    mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;

  return mask;
}

std::string WindowOpenDispositionToString(
  WindowOpenDisposition window_open_disposition) {
  switch (window_open_disposition) {
    case IGNORE_ACTION:
      return "ignore";
    case SAVE_TO_DISK:
      return "save_to_disk";
    case CURRENT_TAB:
      return "current_tab";
    case NEW_BACKGROUND_TAB:
      return "new_background_tab";
    case NEW_FOREGROUND_TAB:
      return "new_foreground_tab";
    case NEW_WINDOW:
      return "new_window";
    case NEW_POPUP:
      return "new_popup";
    default:
      NOTREACHED() << "Unknown Window Open Disposition";
      return "ignore";
  }
}

static std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "abnormal";
#if defined(OS_CHROMEOS)
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return "oom killed";
#endif
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "crashed";
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
static std::string SSLStateToString(connection_security::SecurityLevel status) {
  switch (status) {
  case connection_security::NONE:
    // HTTP/no URL/user is editing
    return "none";
  case connection_security::EV_SECURE:
    // HTTPS with valid EV cert
    return "secure_with_ev";
  case connection_security::SECURE:
    // HTTPS (non-EV)
    return "secure_no_ev";
  case connection_security::SECURITY_WARNING:
    // HTTPS, but unable to check certificate revocation status or with insecure
    // content on the page
    return "security_warning";
  case connection_security::SECURITY_POLICY_WARNING:
    // HTTPS, but the certificate verification chain is anchored on a certificate
    // that was installed by the system administrator
    return "security_policy_warning";
  case connection_security::SECURITY_ERROR:
    // Attempted HTTPS and failed, page not authenticated
    return "security_error";
  default:
    //fallthrough
    break;
  }
  NOTREACHED() << "Unknown connection_security::SecurityLevel";
  return "unknown";
}

static std::string TabMediaStateToString(TabMediaState status) {
  switch (status) {
  case TAB_MEDIA_STATE_NONE:
    return "none";
  case TAB_MEDIA_STATE_RECORDING:
    return "recording";
  case TAB_MEDIA_STATE_CAPTURING:
    return "capturing";
  case TAB_MEDIA_STATE_AUDIO_PLAYING:
    return "playing";
  case TAB_MEDIA_STATE_AUDIO_MUTING:
    return "muting";
  }
  NOTREACHED() << "Unknown TabMediaState Status.";
  return "unknown";
}

#endif //VIVALDI_BUILD_HAS_CHROME_CODE

std::string GetStoragePartitionIdFromSiteURL(const GURL& site_url) {
  const std::string& partition_id = site_url.query();
  bool persist_storage = site_url.path().find("persist") != std::string::npos;
  return (persist_storage ? webview::kPersistPrefix : "") + partition_id;
}

void ParsePartitionParam(const base::DictionaryValue& create_params,
                         std::string* storage_partition_id,
                         bool* persist_storage) {
  std::string partition_str;
  if (!create_params.GetString(webview::kStoragePartitionId, &partition_str)) {
    return;
  }

  // Since the "persist:" prefix is in ASCII, base::StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (base::StartsWithASCII(partition_str, "persist:", true)) {
    size_t index = partition_str.find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    *storage_partition_id = partition_str.substr(index + 1);

    if (storage_partition_id->empty()) {
      // TODO(lazyboy): Better way to deal with this error.
      return;
    }
    *persist_storage = true;
  } else {
    *storage_partition_id = partition_str;
    *persist_storage = false;
  }
}

void RemoveWebViewEventListenersOnIOThread(
    void* profile,
    int embedder_process_id,
    int view_instance_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ExtensionWebRequestEventRouter::GetInstance()->RemoveWebViewEventListeners(
      profile,
      embedder_process_id,
      view_instance_id);
}

double ConvertZoomLevelToZoomFactor(double zoom_level) {
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);
  // Because the conversion from zoom level to zoom factor isn't perfect, the
  // resulting zoom factor is rounded to the nearest 6th decimal place.
  zoom_factor = round(zoom_factor * 1000000) / 1000000;
  return zoom_factor;
}

using WebViewKey = std::pair<int, int>;
using WebViewKeyToIDMap = std::map<WebViewKey, int>;
static base::LazyInstance<WebViewKeyToIDMap> web_view_key_to_id_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
void WebViewGuest::CleanUp(int embedder_process_id, int view_instance_id) {
  GuestViewBase::CleanUp(embedder_process_id, view_instance_id);

  auto rph = content::RenderProcessHost::FromID(embedder_process_id);
  // TODO(paulmeyer): It should be impossible for rph to be nullptr here, but
  // this check is needed here for now as there seems to be occasional crashes
  // because of this (http//crbug.com/499438). This should be removed once the
  // cause is discovered and fixed.
  DCHECK(rph != nullptr)
      << "Cannot find RenderProcessHost for embedder process ID# "
      << embedder_process_id;
  if (rph == nullptr)
    return;
  auto browser_context = rph->GetBrowserContext();

  // Clean up rules registries for the WebView.
  WebViewKey key(embedder_process_id, view_instance_id);
  auto it = web_view_key_to_id_map.Get().find(key);
  if (it != web_view_key_to_id_map.Get().end()) {
    auto rules_registry_id = it->second;
    web_view_key_to_id_map.Get().erase(it);
    RulesRegistryService::Get(browser_context)
        ->RemoveRulesRegistriesByID(rules_registry_id);
  }

  // Clean up web request event listeners for the WebView.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &RemoveWebViewEventListenersOnIOThread,
          browser_context,
          embedder_process_id,
          view_instance_id));

  // Clean up content scripts for the WebView.
  auto csm = WebViewContentScriptManager::Get(browser_context);
  csm->RemoveAllContentScriptsForWebView(embedder_process_id, view_instance_id);

  // Allow an extensions browser client to potentially perform more cleanup.
  ExtensionsBrowserClient::Get()->CleanUpWebView(embedder_process_id,
                                                 view_instance_id);
}

// static
GuestViewBase* WebViewGuest::Create(content::WebContents* owner_web_contents) {
  return new WebViewGuest(owner_web_contents);
}

// static
bool WebViewGuest::GetGuestPartitionConfigForSite(
    const GURL& site,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {

  if (!site.SchemeIs(content::kGuestScheme))
    return false;

  // Since guest URLs are only used for packaged apps, there must be an app
  // id in the URL.
  CHECK(site.has_host());
  *partition_domain = site.host();
  // Since persistence is optional, the path must either be empty or the
  // literal string.
  *in_memory = (site.path() != "/persist");
  // The partition name is user supplied value, which we have encoded when the
  // URL was created, so it needs to be decoded.
  *partition_name =
      net::UnescapeURLComponent(site.query(), net::UnescapeRule::NORMAL);
  return true;
}

// static
const char WebViewGuest::Type[] = "webview";

// static
int WebViewGuest::GetOrGenerateRulesRegistryID(
    int embedder_process_id,
    int webview_instance_id) {
  bool is_web_view = embedder_process_id && webview_instance_id;
  if (!is_web_view)
    return RulesRegistryService::kDefaultRulesRegistryID;

  WebViewKey key = std::make_pair(embedder_process_id, webview_instance_id);
  auto it = web_view_key_to_id_map.Get().find(key);
  if (it != web_view_key_to_id_map.Get().end())
    return it->second;

  auto rph = content::RenderProcessHost::FromID(embedder_process_id);
  int rules_registry_id =
      RulesRegistryService::Get(rph->GetBrowserContext())->
          GetNextRulesRegistryID();
  web_view_key_to_id_map.Get()[key] = rules_registry_id;
  return rules_registry_id;
}

bool WebViewGuest::CanRunInDetachedState() const {
  return true;
}

WebContents::CreateParams WebViewGuest::GetWebContentsCreateParams(
    content::BrowserContext* context,
    const GURL site) {
  // If we already have a webview tag in the same app using the same storage
  // partition, we should use the same SiteInstance so the existing tag and
  // the new tag can script each other.
  auto guest_view_manager = GuestViewManager::FromBrowserContext(context);
  content::SiteInstance* guest_site_instance =
      guest_view_manager ? guest_view_manager->GetGuestSiteInstance(site)
                         : nullptr;
  if (!guest_site_instance) {
    // Create the SiteInstance in a new BrowsingInstance, which will ensure
    // that webview tags are also not allowed to send messages across
    // different partitions.
    guest_site_instance = content::SiteInstance::CreateForURL(context, site);
  }

  WebContents::CreateParams params(context, guest_site_instance);
  // As the tabstrip-content is not a guest we need to delete it and
  // re-create it as a guest then attach all the webcontents observers and
  // in addition replace the tabstrip content with the new guest content.
  params.guest_delegate = this;
  return params;
}

void WebViewGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    const WebContentsCreatedCallback& callback) {
  content::RenderProcessHost* owner_render_process_host =
      owner_web_contents()->GetRenderProcessHost();
  std::string storage_partition_id;
  bool persist_storage = false;
  ParsePartitionParam(create_params, &storage_partition_id, &persist_storage);
  // Validate that the partition id coming from the renderer is valid UTF-8,
  // since we depend on this in other parts of the code, such as FilePath
  // creation. If the validation fails, treat it as a bad message and kill the
  // renderer process.
  if (!base::IsStringUTF8(storage_partition_id)) {
    content::RecordAction(
        base::UserMetricsAction("BadMessageTerminate_BPGM"));
    owner_render_process_host->Shutdown(content::RESULT_CODE_KILLED_BAD_MESSAGE,
                                        false);
    callback.Run(nullptr);
    return;
  }
  std::string url_encoded_partition = net::EscapeQueryParamValue(
      storage_partition_id, false);
  std::string partition_domain = GetOwnerSiteURL().host();

  GURL guest_site;
  std::string new_url;
  if (owner_host() == "mpognobbkildjkofajifpdfhcoklimli" &&
    create_params.GetString(webview::kNewURL, &new_url)) {
    guest_site = GURL(new_url);
  } else {
     guest_site = GURL(base::StringPrintf("%s://%s/%s?%s",
                                     content::kGuestScheme,
                                     partition_domain.c_str(),
                                     persist_storage ? "persist" : "",
                                     url_encoded_partition.c_str()));
  }

  WebContents* newcontents = nullptr;

  // If we created the WebContents through CreateNewWindow and created this
  // guest with InitWithWebContents we cannot delete the tabstrip contents, and
  // we don't need to recreate the webcontents either. Just use the WebContents
  // owned by the tab-strip. It is already a guest so no need to recreate it.
  // This is the reason for webcontents_was_created_as_guest_. Also see
  // GuestViewInternalCreateGuestFunction::RunAsync

  std::string tab_id_as_string;
  if (create_params.GetString("tab_id", &tab_id_as_string) ) {
    int tab_id = atoi(tab_id_as_string.c_str());;
    content::WebContents* tabstrip_contents = NULL;
    bool include_incognito = true;
    Browser* browser;
    Profile* profile =
      Profile::FromBrowserContext(owner_web_contents()->GetBrowserContext());
    int tab_index;
    if (extensions::ExtensionTabUtil::GetTabById(tab_id, profile,
      include_incognito, &browser, NULL, &tabstrip_contents, &tab_index)) {
      if(webcontents_was_created_as_guest_) {
        newcontents = tabstrip_contents;
      } else {

        // We must use the Browser Profile when creating the WebContents. This
        // controls the incognito mode.
        WebContents::CreateParams params =
            GetWebContentsCreateParams(browser->profile(), guest_site);

        newcontents = WebContents::Create(params);

        newcontents->GetController().SetBrowserContext(params.browser_context);

        // copy extdata, tab-tiling information, tab id etc.
        newcontents->SetExtData(tabstrip_contents->GetExtData());

        // Copy the history from |tabstrip_contents|
        newcontents->GetController().CopyStateFrom(
          tabstrip_contents->GetController());

        TabStripModel* tab_strip = browser->tab_strip_model();
        newcontents->SetDelegate(this);
        tab_strip->ReplaceWebContentsAt(tab_index, newcontents);
        delete tabstrip_contents;
      }
    }
  } else {
    // Look up the correct Browser object to use as the Profile owner.
    content::BrowserContext* context =
        owner_render_process_host->GetBrowserContext();
    std::string window_id;
    if (create_params.GetString("window_id", &window_id)) {
      std::string windowId;
      BrowserList* list =
          BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_FIRST);
      for (size_t i = 0; i < list->size(); i++) {
        if (ExtensionActionUtil::GetWindowIdFromExtData(
                list->get(i)->ext_data(), windowId)) {
          if (windowId == window_id) {
            context = list->get(i)->profile();
            break;
          }
        }
      }
    }
    WebContents::CreateParams params =
        GetWebContentsCreateParams(context, guest_site);
    newcontents = WebContents::Create(params);
    newcontents->GetController().SetBrowserContext(context);
  }
  callback.Run(newcontents);
}

void WebViewGuest::DidAttachToEmbedder() {
  ApplyAttributes(*attach_params());
}

void WebViewGuest::DidDropLink(const GURL& url) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guest_view::kUrl, url.spec());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventDropLink, args.Pass()));
}

void WebViewGuest::DidInitialize(const base::DictionaryValue& create_params) {
  script_executor_.reset(
      new ScriptExecutor(web_contents(), &script_observers_));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                              content::Source<WebContents>(web_contents()));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT,
                              content::Source<WebContents>(web_contents()));

  // For Vivaldi the web contents is created through the browser and the
  // helpers are attached there (tab_helpers.cc).

  // Note this is not the case anymore as we replace the |WebContents|

  // Note:  Even though you can attach helpers multiple times there are
  //        some helpers we want to exclude for Vivaldi and we want to control
  //        that in one place).
  if(!base::CommandLine::ForCurrentProcess()->IsRunningVivaldi())
  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnDidInitialize();
  ExtensionsAPIClient::Get()->AttachWebContentsHelpers(web_contents());
  web_view_permission_helper_.reset(new WebViewPermissionHelper(this));

  rules_registry_id_ = GetOrGenerateRulesRegistryID(
      owner_web_contents()->GetRenderProcessHost()->GetID(),
      view_instance_id());

  // We must install the mapping from guests to WebViews prior to resuming
  // suspended resource loads so that the WebRequest API will catch resource
  // requests.
  PushWebViewStateToIOThread();

  ApplyAttributes(create_params);
}

void WebViewGuest::ClearDataInternal(base::Time remove_since,
                                     uint32 removal_mask,
                                     const base::Closure& callback) {
  uint32 storage_partition_removal_mask =
      GetStoragePartitionRemovalMask(removal_mask);
  if (!storage_partition_removal_mask) {
    callback.Run();
    return;
  }
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents()->GetBrowserContext(),
          web_contents()->GetSiteInstance());
  partition->ClearData(
      storage_partition_removal_mask,
      content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL, GURL(),
      content::StoragePartition::OriginMatcherFunction(), remove_since,
      base::Time::Now(), callback);
}

void WebViewGuest::GuestViewDidStopLoading() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadStop, args.Pass()));
}

void WebViewGuest::EmbedderFullscreenToggled(bool entered_fullscreen) {
  is_embedder_fullscreen_ = entered_fullscreen;
  // If the embedder has got out of fullscreen, we get out of fullscreen
  // mode as well.
  if (!entered_fullscreen)
    SetFullscreenState(false);
}

const char* WebViewGuest::GetAPINamespace() const {
  return webview::kAPINamespace;
}

int WebViewGuest::GetTaskPrefix() const {
  return IDS_EXTENSION_TASK_MANAGER_WEBVIEW_TAG_PREFIX;
}

void WebViewGuest::GuestDestroyed() {
  RemoveWebViewStateFromIOThread(web_contents());
}

void WebViewGuest::GuestReady() {
  // The guest RenderView should always live in an isolated guest process.

#ifndef VIVALDI_BUILD_HAS_CHROME_CODE
  CHECK(web_contents()->GetRenderProcessHost()->IsForGuestsOnly());
#endif // VIVALDI_BUILD_HAS_CHROME_CODE

  Send(new ExtensionMsg_SetFrameName(web_contents()->GetRoutingID(), name_));

  // We don't want to accidentally set the opacity of an interstitial page.
  // WebContents::GetRenderWidgetHostView will return the RWHV of an
  // interstitial page if one is showing at this time. We only want opacity
  // to apply to web pages.
  if (allow_transparency_) {
    web_contents()->GetRenderViewHost()->GetView()->SetBackgroundColor(
        SK_ColorTRANSPARENT);
  } else {
    web_contents()
        ->GetRenderViewHost()
        ->GetView()
        ->SetBackgroundColorToDefault();
  }
}

void WebViewGuest::GuestSizeChangedDueToAutoSize(const gfx::Size& old_size,
                                                 const gfx::Size& new_size) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kOldHeight, old_size.height());
  args->SetInteger(webview::kOldWidth, old_size.width());
  args->SetInteger(webview::kNewHeight, new_size.height());
  args->SetInteger(webview::kNewWidth, new_size.width());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventSizeChanged, args.Pass()));
}

bool WebViewGuest::IsAutoSizeSupported() const {
  return true;
}

void WebViewGuest::GuestZoomChanged(double old_zoom_level,
                                    double new_zoom_level) {
  // Dispatch the zoomchange event.
  double old_zoom_factor = ConvertZoomLevelToZoomFactor(old_zoom_level);
  double new_zoom_factor = ConvertZoomLevelToZoomFactor(new_zoom_level);
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetDouble(webview::kOldZoomFactor, old_zoom_factor);
  args->SetDouble(webview::kNewZoomFactor, new_zoom_factor);
  DispatchEventToView(
      new GuestViewEvent(webview::kEventZoomChange, args.Pass()));
}

void WebViewGuest::WillDestroy() {
  if (!attached() && GetOpener())
    GetOpener()->pending_new_windows_.erase(this);

  // Vivaldi: Need to remove notifications since webcontents might be changed.
  notification_registrar_.RemoveAll();
}

bool WebViewGuest::AddMessageToConsole(WebContents* source,
                                       int32 level,
                                       const base::string16& message,
                                       int32 line_no,
                                       const base::string16& source_id) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  // Log levels are from base/logging.h: LogSeverity.
  args->SetInteger(webview::kLevel, level);
  args->SetString(webview::kMessage, message);
  args->SetInteger(webview::kLine, line_no);
  args->SetString(webview::kSourceId, source_id);
  DispatchEventToView(
      new GuestViewEvent(webview::kEventConsoleMessage, args.Pass()));
  return true;
}

void WebViewGuest::CloseContents(WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventClose, args.Pass()));
}

void WebViewGuest::FindReply(WebContents* source,
                             int request_id,
                             int number_of_matches,
                             const gfx::Rect& selection_rect,
                             int active_match_ordinal,
                             bool final_update) {
  find_helper_.FindReply(request_id,
                         number_of_matches,
                         selection_rect,
                         active_match_ordinal,
                         final_update);
}

double WebViewGuest::GetZoom() const {
  double zoom_level =
      ZoomController::FromWebContents(web_contents())->GetZoomLevel();
  return ConvertZoomLevelToZoomFactor(zoom_level);
}

ZoomController::ZoomMode WebViewGuest::GetZoomMode() {
  return ZoomController::FromWebContents(web_contents())->zoom_mode();
}

bool WebViewGuest::HandleContextMenu(
    const content::ContextMenuParams& params) {
  if (!web_view_guest_delegate_)
    return false;
  return web_view_guest_delegate_->HandleContextMenu(params);
}

void WebViewGuest::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (HandleKeyboardShortcuts(event))
    return;

  GuestViewBase::HandleKeyboardEvent(source, event);
}

bool WebViewGuest::PreHandleGestureEvent(content::WebContents* source,
                                         const blink::WebGestureEvent& event) {
  return !allow_scaling_ && GuestViewBase::PreHandleGestureEvent(source, event);
}

void WebViewGuest::LoadProgressChanged(content::WebContents* source,
                                       double progress) {
#if 0  // NOTE(pettern): Ignore as we get the extended event below instead.
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guest_view::kUrl, web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadProgress, args.Pass()));
#endif
}

void WebViewGuest::ExtendedLoadProgressChanged(WebContents* source,
                                               double progress,
                                               double loaded_bytes,
                                               int loaded_elements,
                                               int total_elements) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guest_view::kUrl, web_contents()->GetURL().spec());
  args->SetDouble(webview::kProgress, progress);
  args->SetDouble(webview::kLoadedBytes, loaded_bytes);
  args->SetInteger(webview::kLoadedElements, loaded_elements);
  args->SetInteger(webview::kTotalElements, total_elements);
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadProgress, args.Pass()));
}

void WebViewGuest::LoadAbort(bool is_top_level,
                             const GURL& url,
                             int error_code,
                             const std::string& error_type) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guest_view::kIsTopLevel, is_top_level);
  args->SetString(guest_view::kUrl, url.possibly_invalid_spec());
  args->SetInteger(guest_view::kCode, error_code);
  args->SetString(guest_view::kReason, error_type);
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadAbort, args.Pass()));
}

void WebViewGuest::CreateNewGuestWebViewWindow(
    const content::OpenURLParams& params) {
  GuestViewManager* guest_manager =
      GuestViewManager::FromBrowserContext(browser_context());
  // Set the attach params to use the same partition as the opener.
  // We pull the partition information from the site's URL, which is of the
  // form guest://site/{persist}?{partition_name}.
  const GURL& site_url = web_contents()->GetSiteInstance()->GetSiteURL();
  const std::string storage_partition_id =
      GetStoragePartitionIdFromSiteURL(site_url);
  base::DictionaryValue create_params;
  create_params.SetString(webview::kStoragePartitionId, storage_partition_id);
  // gisli@vivaldi.com:  Add the url so we can create the right site instance
  // when creating the webcontents.
  create_params.SetString(webview::kNewURL, params.url.spec());

  // Vivaldi; in private mode we need to make sure the correct Browser is used
  // as the Profile follows this object correctly.
  if (params.source_site_instance &&
      params.source_site_instance->GetBrowserContext()->IsOffTheRecord()) {
    Profile* current_profile = Profile::FromBrowserContext(
        params.source_site_instance->GetBrowserContext());
    Browser* current_browser = chrome::FindLastActiveWithProfile(
        current_profile, chrome::GetActiveDesktop());
    std::string window_id;
    current_browser->ext_data();
    if (ExtensionActionUtil::GetWindowIdFromExtData(current_browser->ext_data(),
                                                    window_id)) {
      create_params.SetString("window_id", window_id);
    }
  }

  guest_manager->CreateGuest(WebViewGuest::Type,
                             embedder_web_contents(),
                             create_params,
                             base::Bind(&WebViewGuest::NewGuestWebViewCallback,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        params));
}

void WebViewGuest::NewGuestWebViewCallback(
    const content::OpenURLParams& params,
    content::WebContents* guest_web_contents) {
  WebViewGuest* new_guest = WebViewGuest::FromWebContents(guest_web_contents);
  new_guest->SetOpener(this);

  // Take ownership of |new_guest|.
  pending_new_windows_.insert(
      std::make_pair(new_guest, NewWindowInfo(params.url, std::string())));

  // Request permission to show the new window.
  RequestNewWindowPermission(params.disposition,
                             gfx::Rect(),
                             params.user_gesture,
                             new_guest->web_contents());
}

// TODO(fsamuel): Find a reliable way to test the 'responsive' and
// 'unresponsive' events.
void WebViewGuest::RendererResponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventResponsive, args.Pass()));
}

void WebViewGuest::RendererUnresponsive(content::WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventUnresponsive, args.Pass()));
}

void WebViewGuest::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(), web_contents());
      if (content::Source<WebContents>(source).ptr() == web_contents())
        LoadHandlerCalled();
      break;
    }
    case content::NOTIFICATION_RESOURCE_RECEIVED_REDIRECT: {
      DCHECK_EQ(content::Source<WebContents>(source).ptr(), web_contents());
      content::ResourceRedirectDetails* resource_redirect_details =
          content::Details<content::ResourceRedirectDetails>(details).ptr();
      bool is_top_level = resource_redirect_details->resource_type ==
                          content::RESOURCE_TYPE_MAIN_FRAME;
      LoadRedirect(resource_redirect_details->url,
                   resource_redirect_details->new_url,
                   is_top_level);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void WebViewGuest::StartFindInternal(
    const base::string16& search_text,
    const blink::WebFindOptions& options,
    scoped_refptr<WebViewInternalFindFunction> find_function) {
  find_helper_.Find(web_contents(), search_text, options, find_function);
}

void WebViewGuest::StopFindingInternal(content::StopFindAction action) {
  find_helper_.CancelAllFindSessions();
  web_contents()->StopFinding(action);
}

bool WebViewGuest::Go(int relative_index) {
  content::NavigationController& controller = web_contents()->GetController();
  if (!controller.CanGoToOffset(relative_index))
    return false;

  controller.GoToOffset(relative_index);
  return true;
}

void WebViewGuest::Reload() {
  // TODO(fsamuel): Don't check for repost because we don't want to show
  // Chromium's repost warning. We might want to implement a separate API
  // for registering a callback if a repost is about to happen.
  web_contents()->GetController().Reload(false);
}

void WebViewGuest::SetUserAgentOverride(
    const std::string& user_agent_override) {
  is_overriding_user_agent_ = !user_agent_override.empty();
  if (is_overriding_user_agent_) {
    content::RecordAction(UserMetricsAction("WebView.Guest.OverrideUA"));
  }
  web_contents()->SetUserAgentOverride(user_agent_override);
}

void WebViewGuest::Stop() {
  web_contents()->Stop();
}

void WebViewGuest::Terminate() {
  content::RecordAction(UserMetricsAction("WebView.Guest.Terminate"));
  base::ProcessHandle process_handle =
      web_contents()->GetRenderProcessHost()->GetHandle();
  if (process_handle)
    web_contents()->GetRenderProcessHost()->Shutdown(
        content::RESULT_CODE_KILLED, false);
}

bool WebViewGuest::ClearData(base::Time remove_since,
                             uint32 removal_mask,
                             const base::Closure& callback) {
  content::RecordAction(UserMetricsAction("WebView.Guest.ClearData"));
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartition(
          web_contents()->GetBrowserContext(),
          web_contents()->GetSiteInstance());

  if (!partition)
    return false;

  if (removal_mask & webview::WEB_VIEW_REMOVE_DATA_MASK_CACHE) {
    // First clear http cache data and then clear the rest in
    // |ClearDataInternal|.
    int render_process_id = web_contents()->GetRenderProcessHost()->GetID();
    // We need to clear renderer cache separately for our process because
    // StoragePartitionHttpCacheDataRemover::ClearData() does not clear that.
    web_cache::WebCacheManager::GetInstance()->Remove(render_process_id);
    web_cache::WebCacheManager::GetInstance()->ClearCacheForProcess(
        render_process_id);

    base::Closure cache_removal_done_callback = base::Bind(
        &WebViewGuest::ClearDataInternal, weak_ptr_factory_.GetWeakPtr(),
        remove_since, removal_mask, callback);
    // StoragePartitionHttpCacheDataRemover removes itself when it is done.
    // components/, move |ClearCache| to WebViewGuest: http//crbug.com/471287.
    browsing_data::StoragePartitionHttpCacheDataRemover::CreateForRange(
        partition, remove_since, base::Time::Now())
        ->Remove(cache_removal_done_callback);

    return true;
  }

  ClearDataInternal(remove_since, removal_mask, callback);
  return true;
}

WebViewGuest::WebViewGuest(content::WebContents* owner_web_contents)
    : GuestView<WebViewGuest>(owner_web_contents),
      rules_registry_id_(RulesRegistryService::kInvalidRulesRegistryID),
      find_helper_(this),
      is_overriding_user_agent_(false),
      has_left_mousebutton_down_(false),
      has_right_mousebutton_down_(false),
      eat_next_right_mouseup_(false),
      current_host_(NULL),
      gesture_state_(GestureStateNone),
      allow_transparency_(false),
      javascript_dialog_helper_(this),
      allow_scaling_(false),
      is_guest_fullscreen_(false),
      is_embedder_fullscreen_(false),
      last_fullscreen_permission_was_allowed_by_embedder_(false),
      pending_zoom_factor_(0.0),
      media_state_(TAB_MEDIA_STATE_NONE),
      is_visible_(false),
      is_fullscreen_(false),
      window_state_prior_to_fullscreen_(ui::SHOW_STATE_NORMAL),
      webcontents_was_created_as_guest_(false),
      weak_ptr_factory_(this) {
  web_view_guest_delegate_.reset(
    ExtensionsAPIClient::Get()->CreateWebViewGuestDelegate(this));
}

WebViewGuest::~WebViewGuest() {
  if (s_current_webviewguest == this)
    s_current_webviewguest = NULL;

}

// Initialize listeners (cannot do it in constructor as RenderViewHost not ready.)
void WebViewGuest::InitListeners() {
  content::RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
  if (render_view_host && current_host_ != render_view_host) {
    // Add mouse event listener, only one for every new render_view_host
    render_view_host->AddMouseEventCallback(base::Bind(&WebViewGuest::OnMouseEvent, base::Unretained(this)));
    current_host_ = render_view_host;
  }
}

void WebViewGuest::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  if (!render_frame_host->GetParent()) {
    src_ = url;
    // Handle a pending zoom if one exists.
    if (pending_zoom_factor_) {
      SetZoom(pending_zoom_factor_);
      pending_zoom_factor_ = 0.0;
    }
  }
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guest_view::kUrl, url.spec());
  args->SetBoolean(guest_view::kIsTopLevel, !render_frame_host->GetParent());
  args->SetString(webview::kInternalBaseURLForDataURL,
                  web_contents()
                      ->GetController()
                      .GetLastCommittedEntry()
                      ->GetBaseURLForDataURL()
                      .spec());
  args->SetInteger(webview::kInternalCurrentEntryIndex,
                   web_contents()->GetController().GetCurrentEntryIndex());
  args->SetInteger(webview::kInternalEntryCount,
                   web_contents()->GetController().GetEntryCount());
  args->SetInteger(webview::kInternalProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadCommit, args.Pass()));

  find_helper_.CancelAllFindSessions();
}

void WebViewGuest::DidFailProvisionalLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  LoadAbort(!render_frame_host->GetParent(), validated_url, error_code,
            net::ErrorToShortString(error_code));
}

void WebViewGuest::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(guest_view::kUrl, validated_url.spec());
  args->SetBoolean(guest_view::kIsTopLevel, !render_frame_host->GetParent());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadStart, args.Pass()));
}

void WebViewGuest::RenderProcessGone(base::TerminationStatus status) {
  // Cancel all find sessions in progress.
  find_helper_.CancelAllFindSessions();

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetInteger(webview::kProcessId,
                   web_contents()->GetRenderProcessHost()->GetID());
  args->SetString(webview::kReason, TerminationStatusToString(status));
  DispatchEventToView(
      new GuestViewEvent(webview::kEventExit, args.Pass()));
}

void WebViewGuest::UserAgentOverrideSet(const std::string& user_agent) {
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (!entry)
    return;
  entry->SetIsOverridingUserAgent(!user_agent.empty());
  web_contents()->GetController().Reload(false);
}

void WebViewGuest::FrameNameChanged(RenderFrameHost* render_frame_host,
                                    const std::string& name) {
  if (render_frame_host->GetParent())
    return;

  if (name_ == name)
    return;

  ReportFrameNameChange(name);
}

void WebViewGuest::ReportFrameNameChange(const std::string& name) {
  name_ = name;
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kName, name);
  DispatchEventToView(
      new GuestViewEvent(webview::kEventFrameNameChanged, args.Pass()));
}

void WebViewGuest::LoadHandlerCalled() {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventContentLoad, args.Pass()));
}

void WebViewGuest::LoadRedirect(const GURL& old_url,
                                const GURL& new_url,
                                bool is_top_level) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean(guest_view::kIsTopLevel, is_top_level);
  args->SetString(webview::kNewURL, new_url.spec());
  args->SetString(webview::kOldURL, old_url.spec());
  DispatchEventToView(
      new GuestViewEvent(webview::kEventLoadRedirect, args.Pass()));
}

void WebViewGuest::PushWebViewStateToIOThread() {
  const GURL& site_url = web_contents()->GetSiteInstance()->GetSiteURL();
  std::string partition_domain;
  std::string partition_id;
  bool in_memory;
  if (!GetGuestPartitionConfigForSite(
          site_url, &partition_domain, &partition_id, &in_memory)) {

    // Vivaldi - geir: This check started kicking in when we started swithcing
    // instances for the guest view (VB-2455) - see VB-2539 for a TODO
    //NOTREACHED();
    return;
  }

  WebViewRendererState::WebViewInfo web_view_info;
  web_view_info.embedder_process_id =
      owner_web_contents()->GetRenderProcessHost()->GetID();
  web_view_info.instance_id = view_instance_id();
  web_view_info.partition_id = partition_id;
  web_view_info.owner_host = owner_host();
  web_view_info.rules_registry_id = rules_registry_id_;
  web_view_info.type = WebViewGuest::Type;

  // Get content scripts IDs added by the guest.
  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);
  web_view_info.content_script_ids = manager->GetContentScriptIDSet(
      web_view_info.embedder_process_id, web_view_info.instance_id);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&WebViewRendererState::AddGuest,
                 base::Unretained(WebViewRendererState::GetInstance()),
                 web_contents()->GetRenderProcessHost()->GetID(),
                 web_contents()->GetRoutingID(),
                 web_view_info));
}

// static
void WebViewGuest::RemoveWebViewStateFromIOThread(
    WebContents* web_contents) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(
          &WebViewRendererState::RemoveGuest,
          base::Unretained(WebViewRendererState::GetInstance()),
          web_contents->GetRenderProcessHost()->GetID(),
          web_contents->GetRoutingID()));
}

void WebViewGuest::RequestMediaAccessPermission(
    content::WebContents* source,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  web_view_permission_helper_->RequestMediaAccessPermission(source,
                                                            request,
                                                            callback);
}

bool WebViewGuest::CheckMediaAccessPermission(content::WebContents* source,
                                              const GURL& security_origin,
                                              content::MediaStreamType type) {
  return web_view_permission_helper_->CheckMediaAccessPermission(
      source, security_origin, type);
}

void WebViewGuest::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const content::DownloadInformation& info,
    const base::Callback<void(const content::DownloadItemAction&)>& callback) {
  web_view_permission_helper_->CanDownload(url, request_method,
                                           info,
                                           callback);
}

void WebViewGuest::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    const base::Callback<void(bool)>& callback) {
  web_view_permission_helper_->RequestPointerLockPermission(
      user_gesture,
      last_unlocked_by_target,
      callback);
}

void WebViewGuest::SignalWhenReady(const base::Closure& callback) {
  auto manager = WebViewContentScriptManager::Get(browser_context());
  manager->SignalOnScriptsLoaded(callback);
}

void WebViewGuest::WillAttachToEmbedder() {
  rules_registry_id_ = GetOrGenerateRulesRegistryID(
      owner_web_contents()->GetRenderProcessHost()->GetID(),
      view_instance_id());

  // We must install the mapping from guests to WebViews prior to resuming
  // suspended resource loads so that the WebRequest API will catch resource
  // requests.
  PushWebViewStateToIOThread();
}

content::JavaScriptDialogManager* WebViewGuest::GetJavaScriptDialogManager(
    WebContents* source) {
  return &javascript_dialog_helper_;
}

void WebViewGuest::NavigateGuest(const std::string& src,
                                 bool force_navigation,
                                 bool wasTyped) {
  // gisli@vivaldi.com:  For Vivaldi we want to be able to force
  //                     navigation even if the guest is not attached.
  if (!attached() &&
    !(base::CommandLine::ForCurrentProcess()->IsRunningVivaldi() &&
      force_navigation))
    return;

  if (src.empty())
    return;

  GURL url = ResolveURL(src);

  ui::PageTransition transition_type = wasTyped ?
                                       ui::PAGE_TRANSITION_TYPED :
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL;

  // We wait for all the content scripts to load and then navigate the guest
  // if the navigation is embedder-initiated. For browser-initiated navigations,
  // content scripts will be ready.
  if (force_navigation) {
    SignalWhenReady(base::Bind(
        &WebViewGuest::LoadURLWithParams, weak_ptr_factory_.GetWeakPtr(), url,
        content::Referrer(), transition_type,
        GlobalRequestID(), force_navigation));
    return;
  }
  LoadURLWithParams(url, content::Referrer(), transition_type,
                    GlobalRequestID(), force_navigation);
}

bool WebViewGuest::HandleKeyboardShortcuts(
    const content::NativeWebKeyboardEvent& event) {
  // For Vivaldi we want this triggered regardless inside an extension or not.
  // Note andre@vivaldi.com; this should maybe be fixed by setting
  // |owner_extension_id_| to Vivaldi.
  // <webview> outside of Chrome Apps do not handle keyboard shortcuts.
  if (!GuestViewManager::FromBrowserContext(browser_context())->
          IsOwnedByExtension(this) &&
          !base::CommandLine::ForCurrentProcess()->IsRunningVivaldi()) {
    return false;
  }

  if (event.type != blink::WebInputEvent::RawKeyDown)
    return false;

  // We need to go out of fullscreen mode here since the window is forced out of
  // fullscreen and we want the document as well. "fullscreen pseudo"
  if (event.windowsKeyCode == ui::VKEY_ESCAPE) {
    ExitFullscreenModeForTab(web_contents());
  }

  // If the user hits the escape key without any modifiers then unlock the
  // mouse if necessary.
  if ((event.windowsKeyCode == ui::VKEY_ESCAPE) &&
      !(event.modifiers & blink::WebInputEvent::InputModifiers)) {
    return web_contents()->GotResponseToLockMouseRequest(false);
  }

#if defined(OS_MACOSX)
  if (event.modifiers != blink::WebInputEvent::MetaKey)
    return false;

  if (event.windowsKeyCode == ui::VKEY_OEM_4) {
    Go(-1);
    return true;
  }

  if (event.windowsKeyCode == ui::VKEY_OEM_6) {
    Go(1);
    return true;
  }
#else
  if (event.windowsKeyCode == ui::VKEY_BROWSER_BACK) {
    Go(-1);
    return true;
  }

  if (event.windowsKeyCode == ui::VKEY_BROWSER_FORWARD) {
    Go(1);
    return true;
  }
#endif

  return false;
}

void WebViewGuest::ApplyAttributes(const base::DictionaryValue& params) {
  std::string name;
  if (params.GetString(webview::kAttributeName, &name)) {
    // If the guest window's name is empty, then the WebView tag's name is
    // assigned. Otherwise, the guest window's name takes precedence over the
    // WebView tag's name.
    if (name_.empty())
      SetName(name);
  }
  if (attached())
    ReportFrameNameChange(name_);

  std::string user_agent_override;
  params.GetString(webview::kParameterUserAgentOverride, &user_agent_override);
  SetUserAgentOverride(user_agent_override);

  bool allow_transparency = false;
  if (params.GetBoolean(webview::kAttributeAllowTransparency,
      &allow_transparency)) {
    // We need to set the background opaque flag after navigation to ensure that
    // there is a RenderWidgetHostView available.
    SetAllowTransparency(allow_transparency);
  }

  bool allow_scaling = false;
  if (params.GetBoolean(webview::kAttributeAllowScaling, &allow_scaling))
    SetAllowScaling(allow_scaling);

  // Check for a pending zoom from before the first navigation.
  params.GetDouble(webview::kInitialZoomFactor, &pending_zoom_factor_);

  bool is_pending_new_window = false;
  if (GetOpener()) {
    // We need to do a navigation here if the target URL has changed between
    // the time the WebContents was created and the time it was attached.
    // We also need to do an initial navigation if a RenderView was never
    // created for the new window in cases where there is no referrer.
    auto it = GetOpener()->pending_new_windows_.find(this);
    if (it != GetOpener()->pending_new_windows_.end()) {
      const NewWindowInfo& new_window_info = it->second;
      if (new_window_info.changed || !web_contents()->HasOpener())
        NavigateGuest(new_window_info.url.spec(), false /* force_navigation */);

      // Once a new guest is attached to the DOM of the embedder page, then the
      // lifetime of the new guest is no longer managed by the opener guest.
      GetOpener()->pending_new_windows_.erase(this);

      is_pending_new_window = true;
    }
  }

  // Only read the src attribute if this is not a New Window API flow.
  if (!is_pending_new_window) {
    std::string src;
    if (params.GetString(webview::kAttributeSrc, &src))
      NavigateGuest(src, true /* force_navigation */);
  }
}

void WebViewGuest::ShowContextMenu(
    int request_id,
    const WebViewGuestDelegate::MenuItemVector* items) {
  if (web_view_guest_delegate_)
    web_view_guest_delegate_->OnShowContextMenu(request_id, items);
}

void WebViewGuest::SetName(const std::string& name) {
  if (name_ == name)
    return;
  name_ = name;

  Send(new ExtensionMsg_SetFrameName(routing_id(), name_));
}

void WebViewGuest::SetZoom(double zoom_factor) {
  auto zoom_controller = ZoomController::FromWebContents(web_contents());
  DCHECK(zoom_controller);
  double zoom_level = content::ZoomFactorToZoomLevel(zoom_factor);
  zoom_controller->SetZoomLevel(zoom_level);
}

void WebViewGuest::SetZoomMode(ZoomController::ZoomMode zoom_mode) {
  ZoomController::FromWebContents(web_contents())->SetZoomMode(zoom_mode);
}

void WebViewGuest::SetAllowTransparency(bool allow) {
  if (allow_transparency_ == allow)
    return;

  allow_transparency_ = allow;
  if (!web_contents()->GetRenderViewHost()->GetView())
    return;

  if (allow_transparency_) {
    web_contents()->GetRenderViewHost()->GetView()->SetBackgroundColor(
        SK_ColorTRANSPARENT);
  } else {
    web_contents()
        ->GetRenderViewHost()
        ->GetView()
        ->SetBackgroundColorToDefault();
  }
}

void WebViewGuest::SetAllowScaling(bool allow) {
  allow_scaling_ = allow;
}

bool WebViewGuest::LoadDataWithBaseURL(const std::string& data_url,
                                       const std::string& base_url,
                                       const std::string& virtual_url,
                                       std::string* error) {
  // Make GURLs from URLs.
  const GURL data_gurl = GURL(data_url);
  const GURL base_gurl = GURL(base_url);
  const GURL virtual_gurl = GURL(virtual_url);

  // Check that the provided URLs are valid.
  // |data_url| must be a valid data URL.
  if (!data_gurl.is_valid() || !data_gurl.SchemeIs(url::kDataScheme)) {
    base::SStringPrintf(
        error, webview::kAPILoadDataInvalidDataURL, data_url.c_str());
    return false;
  }
  // |base_url| must be a valid URL.
  if (!base_gurl.is_valid()) {
    base::SStringPrintf(
        error, webview::kAPILoadDataInvalidBaseURL, base_url.c_str());
    return false;
  }
  // |virtual_url| must be a valid URL.
  if (!virtual_gurl.is_valid()) {
    base::SStringPrintf(
        error, webview::kAPILoadDataInvalidVirtualURL, virtual_url.c_str());
    return false;
  }

  // Set up the parameters to load |data_url| with the specified |base_url|.
  content::NavigationController::LoadURLParams load_params(data_gurl);
  load_params.load_type = content::NavigationController::LOAD_TYPE_DATA;
  load_params.base_url_for_data_url = base_gurl;
  load_params.virtual_url_for_data_url = virtual_gurl;
  load_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_INHERIT;

  // Navigate to the data URL.
  GuestViewBase::LoadURLWithParams(load_params);

  return true;
}

void WebViewGuest::AddNewContents(content::WebContents* source,
                                  content::WebContents* new_contents,
                                  WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_rect,
                                  bool user_gesture,
                                  bool* was_blocked) {
  if (was_blocked)
    *was_blocked = false;
  RequestNewWindowPermission(disposition,
                             initial_rect,
                             user_gesture,
                             new_contents);
}

content::WebContents* WebViewGuest::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // Most navigations should be handled by WebViewGuest::LoadURLWithParams,
  // which takes care of blocking chrome:// URLs and other web-unsafe schemes.
  // (NavigateGuest and CreateNewGuestWebViewWindow also go through
  // LoadURLWithParams.)
  //
  // We make an exception here for context menu items, since the Language
  // Settings item uses a browser-initiated navigation to a chrome:// URL.
  // These can be passed to the embedder's WebContentsDelegate so that the
  // browser performs the action for the <webview>.
  if (!base::CommandLine::ForCurrentProcess()->IsRunningVivaldi()) {
  if (!params.is_renderer_initiated &&
      !content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
          params.url.scheme())) {
    if (!owner_web_contents()->GetDelegate())
      return nullptr;
    return owner_web_contents()->GetDelegate()->OpenURLFromTab(
        owner_web_contents(), params);
  }
  }

  // If the guest wishes to navigate away prior to attachment then we save the
  // navigation to perform upon attachment. Navigation initializes a lot of
  // state that assumes an embedder exists, such as RenderWidgetHostViewGuest.
  // Navigation also resumes resource loading which we don't want to allow
  // until attachment.
  if (!attached()) {
    WebViewGuest* opener = GetOpener();
#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
    if (!opener)
      return NULL;
#endif //VIVALDI_BUILD_HAS_CHROME_CODE
    auto it = opener->pending_new_windows_.find(this);
    if (it == opener->pending_new_windows_.end())
      return nullptr;
    const NewWindowInfo& info = it->second;
    NewWindowInfo new_window_info(params.url, info.name);
    new_window_info.changed = new_window_info.url != info.url;
    it->second = new_window_info;
    return nullptr;
  }

  // This code path is taken if RenderFrameImpl::DecidePolicyForNavigation
  // decides that a fork should happen. At the time of writing this comment,
  // the only way a well behaving guest could hit this code path is if it
  // navigates to a URL that's associated with the default search engine.
  // This list of URLs is generated by chrome::GetSearchURLs. Validity checks
  // are performed inside LoadURLWithParams such that if the guest attempts
  // to navigate to a URL that it is not allowed to navigate to, a 'loadabort'
  // event will fire in the embedder, and the guest will be navigated to
  // about:blank.
  if (params.disposition == CURRENT_TAB) {
    LoadURLWithParams(params.url, params.referrer, params.transition,
                      params.transferred_global_request_id,
                      true /* force_navigation */);
    return web_contents();
  }

  // This code path is taken if Ctrl+Click, middle click or any of the
  // keyboard/mouse combinations are used to open a link in a new tab/window.
  // This code path is also taken on client-side redirects from about:blank.
  CreateNewGuestWebViewWindow(params);
  return nullptr;
}

void WebViewGuest::WebContentsCreated(WebContents* source_contents,
                                      int opener_render_frame_id,
                                      const std::string& frame_name,
                                      const GURL& target_url,
                                      content::WebContents* new_contents) {
  auto guest = WebViewGuest::FromWebContents(new_contents);
  CHECK(guest);
  guest->SetOpener(this);
  guest->name_ = frame_name;
  pending_new_windows_.insert(
      std::make_pair(guest, NewWindowInfo(target_url, frame_name)));
}

void WebViewGuest::EnterFullscreenModeForTab(content::WebContents* web_contents,
                                             const GURL& origin) {
  // Ask the embedder for permission.
  base::DictionaryValue request_info;
  request_info.SetString(webview::kOrigin, origin.spec());
  web_view_permission_helper_->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FULLSCREEN, request_info,
      base::Bind(&WebViewGuest::OnFullscreenPermissionDecided,
                 weak_ptr_factory_.GetWeakPtr()),
      false /* allowed_by_default */);

  // TODO(lazyboy): Right now the guest immediately goes fullscreen within its
  // bounds. If the embedder denies the permission then we will see a flicker.
  // Once we have the ability to "cancel" a renderer/ fullscreen request:
  // http://crbug.com/466854 this won't be necessary and we should be
  // Calling SetFullscreenState(true) once the embedder allowed the request.
  // Otherwise we would cancel renderer/ fullscreen if the embedder denied.
  SetFullscreenState(true);
  ToggleFullscreenModeForTab(web_contents, true);
}

void WebViewGuest::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  SetFullscreenState(false);
  ToggleFullscreenModeForTab(web_contents, false);
}

void WebViewGuest::ToggleFullscreenModeForTab(
    content::WebContents* web_contents,
    bool enter_fullscreen) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetBoolean("enterFullscreen", enter_fullscreen);

  bool state_changed = enter_fullscreen != is_fullscreen_;
  is_fullscreen_ = enter_fullscreen;

  extensions::AppWindow* app_win = GetAppWindow();
  if (app_win) {

    extensions::NativeAppWindow* native_app_window = app_win->GetBaseWindow();
    ui::WindowShowState current_window_state =
        native_app_window->GetRestoredState();

    if (enter_fullscreen) {
      window_state_prior_to_fullscreen_ = current_window_state;
      app_win->Fullscreen();
    }
    else
    {
      switch (window_state_prior_to_fullscreen_) {
      case ui::SHOW_STATE_MAXIMIZED:
      case ui::SHOW_STATE_NORMAL:
      case ui::SHOW_STATE_DEFAULT:
        // If state did not change we had a plugin that came out of fullscreen.
        // Only HTML-element fullscreen changes the appwindow state.
        if (state_changed) {
          app_win->Restore();
        }
        break;
      case ui::SHOW_STATE_FULLSCREEN:
        app_win->Fullscreen();
        break;
      default:
        NOTREACHED() << "uncovered state";
        break;
      }
    }
  }
  if (state_changed) {
    DispatchEventToView(
        new GuestViewEvent("webViewInternal.onFullscreen", args.Pass()));
  }
}

bool WebViewGuest::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) const {
  return is_guest_fullscreen_;
}

void WebViewGuest::LoadURLWithParams(
    const GURL& url,
    const content::Referrer& referrer,
    ui::PageTransition transition_type,
    const GlobalRequestID& transferred_global_request_id,
    bool force_navigation) {
  // Do not allow navigating a guest to schemes other than known safe schemes.
  // This will block the embedder trying to load unwanted schemes, e.g.
  // chrome://.
  bool scheme_is_blocked =
      (!content::ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
           url.scheme()) &&
       !url.SchemeIsFile() &&                           // Vivaldi custom
       !url.SchemeIs(url::kAboutScheme) &&
       !url.SchemeIs(content::kViewSourceScheme)) ||    // Vivaldi custom
      url.SchemeIs(url::kJavaScriptScheme);
  if (scheme_is_blocked || !url.is_valid()) {
    LoadAbort(true /* is_top_level */, url, net::ERR_ABORTED,
              net::ErrorToShortString(net::ERR_ABORTED));
    NavigateGuest(url::kAboutBlankURL, false /* force_navigation */);
    return;
  }

  if (!force_navigation && (src_ == url))
    return;

  GURL validated_url(url);
  web_contents()->GetRenderProcessHost()->FilterURL(false, &validated_url);
  // As guests do not swap processes on navigation, only navigations to
  // normal web URLs are supported.  No protocol handlers are installed for
  // other schemes (e.g., WebUI or extensions), and no permissions or bindings
  // can be granted to the guest process.
  content::NavigationController::LoadURLParams load_url_params(validated_url);
  load_url_params.referrer = referrer;
  load_url_params.transition_type = transition_type;
  load_url_params.extra_headers = std::string();
  load_url_params.transferred_global_request_id = transferred_global_request_id;
  if (is_overriding_user_agent_) {
    load_url_params.override_user_agent =
        content::NavigationController::UA_OVERRIDE_TRUE;
  }
  GuestViewBase::LoadURLWithParams(load_url_params);

  src_ = validated_url;
}

void WebViewGuest::RequestNewWindowPermission(
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_bounds,
    bool user_gesture,
    content::WebContents* new_contents) {
  auto guest = WebViewGuest::FromWebContents(new_contents);
  if (!guest)
    return;
  auto it = pending_new_windows_.find(guest);
  if (it == pending_new_windows_.end())
    return;
  const NewWindowInfo& new_window_info = it->second;

  // Retrieve the opener partition info if we have it.
  const GURL& site_url = new_contents->GetSiteInstance()->GetSiteURL();
  std::string storage_partition_id = GetStoragePartitionIdFromSiteURL(site_url);

  base::DictionaryValue request_info;
  request_info.SetInteger(webview::kInitialHeight, initial_bounds.height());
  request_info.SetInteger(webview::kInitialWidth, initial_bounds.width());
  request_info.Set(webview::kTargetURL,
                   new base::StringValue(new_window_info.url.spec()));
  request_info.Set(webview::kName, new base::StringValue(new_window_info.name));
  request_info.SetInteger(webview::kWindowID, guest->guest_instance_id());
  // We pass in partition info so that window-s created through newwindow
  // API can use it to set their partition attribute.
  request_info.Set(webview::kStoragePartitionId,
                   new base::StringValue(storage_partition_id));
  request_info.Set(
      webview::kWindowOpenDisposition,
      new base::StringValue(WindowOpenDispositionToString(disposition)));
  request_info.SetBoolean(guest_view::kUserGesture, user_gesture);

  web_view_permission_helper_->
      RequestPermission(WEB_VIEW_PERMISSION_TYPE_NEW_WINDOW,
                        request_info,
                        base::Bind(&WebViewGuest::OnWebViewNewWindowResponse,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   guest->guest_instance_id()),
                                   false /* allowed_by_default */);
}

GURL WebViewGuest::ResolveURL(const std::string& src) {
  if (!GuestViewManager::FromBrowserContext(browser_context())->
          IsOwnedByExtension(this)) {
    return GURL(src);
  }

  GURL default_url(base::StringPrintf("%s://%s/",
                                      kExtensionScheme,
                                      owner_host().c_str()));
  return default_url.Resolve(src);
}

void WebViewGuest::OnWebViewNewWindowResponse(
    int new_window_instance_id,
    bool allow,
    const std::string& user_input) {
  auto guest =
      WebViewGuest::From(owner_web_contents()->GetRenderProcessHost()->GetID(),
                         new_window_instance_id);
  if (!guest)
    return;

  if (allow) {
#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
    if (base::CommandLine::ForCurrentProcess()->IsRunningVivaldi()) {
      //user_input is a string. on the form nn;n
      //nn is windowId,
      //n is 1 or 0
      // 1 if tab should be opened in foreground
      // 0 if tab should be opened
      std::vector<std::string> lines;
      base::SplitString(user_input, ';', &lines);
      bool foreground = true;
      int windowId = atoi(lines[0].c_str());
      if (lines.size() == 2){
        std::string strForeground = lines[1].c_str();
        foreground = strForeground == "1";
      }

      AddGuestToTabStripModel(guest, windowId, foreground);
    }
#endif //VIVALDI_BUILD_HAS_CHROME_CODE
  } else {
    guest->Destroy();
  }
}

void WebViewGuest::OnFullscreenPermissionDecided(
    bool allowed,
    const std::string& user_input) {
  last_fullscreen_permission_was_allowed_by_embedder_ = allowed;
  SetFullscreenState(allowed);
}

bool WebViewGuest::GuestMadeEmbedderFullscreen() const {
  return last_fullscreen_permission_was_allowed_by_embedder_ &&
         is_embedder_fullscreen_;
}

void WebViewGuest::SetFullscreenState(bool is_fullscreen) {
  if (is_fullscreen == is_guest_fullscreen_)
    return;

  bool was_fullscreen = is_guest_fullscreen_;
  is_guest_fullscreen_ = is_fullscreen;
  // If the embedder entered fullscreen because of us, it should exit fullscreen
  // when we exit fullscreen.
  if (was_fullscreen && GuestMadeEmbedderFullscreen()) {
    // Dispatch a message so we can call document.webkitCancelFullscreen()
    // on the embedder.
    scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
    DispatchEventToView(
        new GuestViewEvent(webview::kEventExitFullscreen, args.Pass()));
  }
  // Since we changed fullscreen state, sending a Resize message ensures that
  // renderer/ sees the change.
  web_contents()->GetRenderViewHost()->WasResized();
}

bool WebViewGuest::IsVivaldiWebPanel() {
  return name_.compare("vivaldi-webpanel") == 0;
}

void WebViewGuest::SetVisible(bool is_visible) {

  is_visible_ = is_visible;
  content::RenderWidgetHostView* widgethostview = web_contents()->GetRenderWidgetHostView();
  if (widgethostview) {
    if (is_visible && !widgethostview->IsShowing()) {
      widgethostview->Show();
      // This is called from CoreTabHelper::WasShown, and must be called because
      // the activity must be updated so that the render-state is updated. This
      // will make sure that the memory usage is on-par with what Chrome use.
      // See VB-671 for more information and comments.
      web_cache::WebCacheManager::GetInstance()->ObserveActivity(
        web_contents()->GetRenderProcessHost()->GetID());
    }
    if (!is_visible && widgethostview->IsShowing()) {
      widgethostview->Hide();
    }
  }

  // andre@vivaldi.com: Note that this assume we only have one visible VivaldiViewGuest.
  if (is_visible)
    s_current_webviewguest = this;

}

bool WebViewGuest::IsVisible() {
  return is_visible_;
}

extensions::AppWindow* WebViewGuest::GetAppWindow() {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    AppWindowRegistry* app_registry = AppWindowRegistry::Get(browser->profile());
    const AppWindowRegistry::AppWindowList& app_windows =
      app_registry->app_windows();

    AppWindowRegistry::const_iterator iter = app_windows.begin();
    if (iter != app_windows.end())
      return (*iter);
  }
#endif //!OS_ANDROID
  return NULL;
}

void WebViewGuest::ShowPageInfo(gfx::Point pos) {
#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
  content::NavigationController& controller =
    web_contents()->GetController();
  const content::NavigationEntry* activeEntry = controller.GetActiveEntry();
  if (!activeEntry) {
    return;
  }

  const GURL url = controller.GetActiveEntry()->GetURL();

  const content::NavigationEntry* entry = controller.GetVisibleEntry();
  const content::SSLStatus ssl = entry->GetSSL();
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());

  if (browser->window())
    browser->window()->VivaldiShowWebsiteSettingsAt(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
      web_contents(), url, ssl, pos);

#endif // VIVALDI_BUILD_HAS_CHROME_CODE
}

#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
void WebViewGuest::UpdateMediaState(TabMediaState state) {
  if (state != media_state_) {
    scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());

    args->SetString("activeMediaType", TabMediaStateToString(state));

    DispatchEventToView(new GuestViewEvent("webViewInternal.onMediaStateChanged", args.Pass()));

  }
  media_state_ = state;
}
#endif // VIVALDI_BUILD_HAS_CHROME_CODE

void WebViewGuest::NavigationStateChanged(content::WebContents* source, content::InvalidateTypes changed_flags) {
#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
  UpdateMediaState(chrome::GetTabMediaStateForContents(web_contents()));
  
  // TODO(gisli):  This would normally be done in the browser, but until we get
  // Vivaldi browser object we do it here (as we did remove the webcontents listener
  // for the current browser).
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    static_cast<content::WebContentsDelegate*>(browser)->NavigationStateChanged(web_contents(), changed_flags);
  }
#endif // VIVALDI_BUILD_HAS_CHROME_CODE
}

bool WebViewGuest::EmbedsFullscreenWidget() const {
  // If WebContents::GetFullscreenRenderWidgetHostView is present there is a window
  // other than this handling the fullscreen operation.
  return web_contents()->GetFullscreenRenderWidgetHostView() == NULL;
}

void WebViewGuest::SetIsFullscreen(bool is_fullscreen) {
  is_fullscreen_ = is_fullscreen;
}

void WebViewGuest::VisibleSSLStateChanged(const WebContents* source) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
  connection_security::SecurityLevel current_level =
    connection_security::GetSecurityLevelForWebContents(web_contents());
  args->SetString("SSLState", SSLStateToString(current_level));

  content::NavigationController& controller =
    web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetVisibleEntry();
  if (entry) {

    scoped_refptr<net::X509Certificate> cert;
    content::CertStore::GetInstance()->RetrieveCert(entry->GetSSL().cert_id, &cert);

    // EV are required to have an organization name and country.
    if (cert.get() && (!cert.get()->subject().organization_names.empty() &&
      !cert.get()->subject().country_name.empty())) {

      args->SetString("issuerstring", base::StringPrintf("%s [%s]",
        cert.get()->subject().organization_names[0].c_str(),
        cert.get()->subject().country_name.c_str()));

    }

  }
  DispatchEventToView(new GuestViewEvent("webViewInternal.onSSLStateChanged", args.Pass()));
#endif // VIVALDI_BUILD_HAS_CHROME_CODE
}

#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
//vivaldi addition:
bool WebViewGuest::GetMousegesturesEnabled()
{
  PrefService *pref_service = Profile::FromBrowserContext(
    web_contents()->GetBrowserContext())->GetPrefs();
  return pref_service->GetBoolean(prefs::kMousegesturesEnabled);
}
#endif //VIVALDI_BUILD_HAS_CHROME_CODE


bool WebViewGuest::OnMouseEvent(const blink::WebMouseEvent& mouse_event) {

  // Rocker gestures, a.la Opera
#define ROCKER_GESTURES
#ifdef ROCKER_GESTURES
  if (has_left_mousebutton_down_ &&
    mouse_event.type == blink::WebInputEvent::MouseUp &&
    mouse_event.button == blink::WebMouseEvent::ButtonLeft) {
    has_left_mousebutton_down_ = false;
  }
  else if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonLeft) {
    has_left_mousebutton_down_ = true;
  }

  if (has_right_mousebutton_down_ &&
    mouse_event.type == blink::WebInputEvent::MouseUp &&
    mouse_event.button == blink::WebMouseEvent::ButtonRight) {
    has_right_mousebutton_down_ = false;
  }
  else if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonRight) {
    has_right_mousebutton_down_ = true;
  }

  if (mouse_event.button == blink::WebMouseEvent::ButtonNone) {
    has_right_mousebutton_down_ = false;
    has_left_mousebutton_down_ = false;
  }

  if (has_left_mousebutton_down_ &&
    (mouse_event.type == blink::WebInputEvent::MouseDown && mouse_event.button == blink::WebMouseEvent::ButtonRight)) {
    eat_next_right_mouseup_ = true;
    Go(1);
    return true;
  }

  if (has_right_mousebutton_down_ &&
    (mouse_event.type == blink::WebInputEvent::MouseDown && mouse_event.button == blink::WebMouseEvent::ButtonLeft)) {
    Go(-1);
    eat_next_right_mouseup_ = true;
    return true;
  }

  if (eat_next_right_mouseup_ && mouse_event.type == blink::WebInputEvent::MouseUp && mouse_event.button == blink::WebMouseEvent::ButtonRight) {
    eat_next_right_mouseup_ = false;
    return true;
  }
#endif // ROCKER_GESTURES

#if defined(OS_LINUX) || defined(OS_MACOSX)
// Vivaldi addition: only define mouse gesture for OSX & linux
// because context menu is shown on mouse down for those systems
#define MOUSE_GESTURES
#endif

#ifdef MOUSE_GESTURES
  if (!GetMousegesturesEnabled()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->IsRunningVivaldi() &&
      mouse_event.type == blink::WebInputEvent::MouseMove &&
      mouse_event.type != blink::WebInputEvent::MouseDown &&
      gesture_state_ == GestureStateRecording) {
    // recording a gesture when the mouse is not down does not make sense
    gesture_state_ = GestureStateNone;
  }

  // Record the gesture
  if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonRight &&
    !(mouse_event.modifiers & blink::WebInputEvent::LeftButtonDown) &&
    gesture_state_ == GestureStateNone) {
    gesture_state_ = GestureStateRecording;
    gesture_direction_candidate_x_ = x_ = mouse_event.x;
    gesture_direction_candidate_y_ = y_ = mouse_event.y;
    gesture_direction_ = GestureDirectionNONE;
    gesture_data_ = 0;
    return true;
  }
  else if (gesture_state_ == GestureStateRecording) {
    if (mouse_event.type == blink::WebInputEvent::MouseMove ||
      mouse_event.type == blink::WebInputEvent::MouseUp) {
      int dx = mouse_event.x - gesture_direction_candidate_x_;
      int dy = mouse_event.y - gesture_direction_candidate_y_;
      GestureDirection candidate_direction;
      // Find current direction from last candidate location.
      if (abs(dx) > abs(dy))
        candidate_direction = dx > 0 ? GestureDirectionRight : GestureDirectionLeft;
      else // abs(dx) <= abs(dy)
        candidate_direction = dy > 0 ? GestureDirectionDown : GestureDirectionUp;

      if (candidate_direction == gesture_direction_) {
        // The mouse is still moving in an overall direction of the last gesture direction,
        // update the candidate location.
        gesture_direction_candidate_x_ = mouse_event.x;
        gesture_direction_candidate_y_ = mouse_event.y;
      }
      else if (abs(dx) >= 5 || abs(dy) >= 5) {
        gesture_data_ = 0x1; //no more info needed since mouse gestures are handled by javascript
      }
    }

    // Map a gesture to an action
    if (mouse_event.type == blink::WebInputEvent::MouseUp &&
      mouse_event.button == blink::WebMouseEvent::ButtonRight) {
      blink::WebMouseEvent event_copy(mouse_event);
      content::RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
      // At this point we may be sending events that could look like new gestures, don't consume them.
      gesture_state_ = GestureStateBlocked;
      switch (gesture_data_) {
      case 0: // No sufficient movement
        // Send the originally-culled right mouse down at original coords
        event_copy.type = blink::WebInputEvent::MouseDown;
        event_copy.windowX -= (mouse_event.x - x_);
        event_copy.windowY -= (mouse_event.y - y_);
        event_copy.x = x_;
        event_copy.y = y_;
        render_view_host->ForwardMouseEvent(event_copy);
        break;
      default:
        //Unknown gesture, don't do anything.
        break;
      }
      gesture_state_ = GestureStateNone;
      return gesture_data_ != 0;
    }
    else if (mouse_event.type == blink::WebInputEvent::MouseDown &&
      mouse_event.button == blink::WebMouseEvent::ButtonLeft) {
      gesture_state_ = GestureStateNone;
    }
  }
  /*if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonLeft &&
    (mouse_event.modifiers & blink::WebInputEvent::RightButtonDown)) {
    if (mouse_event.modifiers & blink::WebInputEvent::ShiftKey)
      // Rewind
      ;
    else
      Go(-1);
    return true;
  }
  if (mouse_event.type == blink::WebInputEvent::MouseDown &&
    mouse_event.button == blink::WebMouseEvent::ButtonRight &&
    (mouse_event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    if (mouse_event.modifiers & blink::WebInputEvent::ShiftKey)
      // Fast-forward
      ;
    else
      Go(1);
    return true;
  }*/
#endif // MOUSE_GESTURES

  return false;
}
void WebViewGuest::UpdateTargetURL(content::WebContents* source, const GURL& url) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kNewURL, url.spec());
  DispatchEventToView(new GuestViewEvent("webViewInternal.onTargetURLChanged", args.Pass()));
}

void WebViewGuest::CreateSearch(const base::ListValue & search) {
  std::string keyword, url;

  if (!search.GetString(0, &keyword)) {
    NOTREACHED();
    return;
  }
  if (!search.GetString(1, &url)) {
    NOTREACHED();
    return;
  }

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kNewSearchName, keyword);
  args->SetString(webview::kNewSearchUrl, url);
  DispatchEventToView(new GuestViewEvent("webViewInternal.onCreateSearch", args.Pass()));
}

void WebViewGuest::PasteAndGo(const base::ListValue & search) {
  std::string clipBoardText;

  if (!search.GetString(0, &clipBoardText)) {
    NOTREACHED();
    return;
  }

  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->SetString(webview::kClipBoardText, clipBoardText);
  DispatchEventToView(new GuestViewEvent("webViewInternal.onPasteAndGo", args.Pass()));
}

void WebViewGuest::SetWebContentsWasInitiallyGuest(bool born_guest) {
  webcontents_was_created_as_guest_ = born_guest;
}

void WebViewGuest::AddGuestToTabStripModel(WebViewGuest* guest,
                                           int windowId,
                                           bool activePage) {
#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
  Browser* browser = chrome::FindBrowserWithID(windowId);

  if (!browser || !browser->window()) {
    // TODO(gisli@vivaldi.com): Error message?
    return;
  }

  guest->SetWebContentsWasInitiallyGuest(true);

  TabStripModel* tab_strip = browser->tab_strip_model();

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = activePage;
  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = false;
  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = -1;
  index = std::min(std::max(index, -1), tab_strip->count());

  int add_types = active ? TabStripModel::ADD_ACTIVE : TabStripModel::ADD_NONE;
  add_types |= TabStripModel::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;
  chrome::NavigateParams navigate_params(
    browser, guest->web_contents());
  navigate_params.disposition =
    active ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  navigate_params.source_contents = web_contents();
  chrome::Navigate(&navigate_params);

  if (active)
    navigate_params.target_contents->SetInitialFocus();

  if (navigate_params.target_contents) {
    navigate_params.target_contents->Send(new ChromeViewMsg_SetWindowFeatures(
        navigate_params.target_contents->GetRoutingID(),
        blink::WebWindowFeatures()));
  }

  // If we have not already navigated do it at this point.
  if (guest->src_.is_empty() &&
      !guest->web_contents()->GetController().GetActiveEntry()) {
    PendingWindowMap::iterator it = pending_new_windows_.find(guest);
    if (it != pending_new_windows_.end()) {
      const NewWindowInfo& new_window_info = it->second;
      if (!new_window_info.url.is_empty()) {  // Do not load blank URLs.
        content::NavigationController::LoadURLParams load_url_params(
            new_window_info.url);
        load_url_params.referrer = content::Referrer(
            src_.GetAsReferrer(), blink::WebReferrerPolicyDefault);
        guest->web_contents()->GetController().LoadURLWithParams(
            load_url_params);
      }
    }
  }

#endif //VIVALDI_BUILD_HAS_CHROME_CODE
}

bool WebViewGuest::RequestPageInfo(const GURL& url) {
  scoped_ptr<base::DictionaryValue> args(new base::DictionaryValue());
  args->Set(webview::kTargetURL, new base::StringValue(url.spec()));
  DispatchEventToView(
      new GuestViewEvent(webview::kEventRequestPageInfo, args.Pass()));
  return true;
}

}  // namespace extensions
