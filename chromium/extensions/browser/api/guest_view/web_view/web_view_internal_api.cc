// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/guest_view/web_view/web_view_internal_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/url_fetcher.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_content_script_manager.h"
#include "extensions/common/api/web_view_internal.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "base/base64.h"
#include "base/strings/stringprintf.h"

#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/geometry/size_conversions.h"


#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/renderer_host/dip_util.h"
#include "skia/ext/image_operations.h"

#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "ui/gfx/scrollbar_size.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#endif  // VIVALDI_BUILD_HAS_CHROME_CODE

using content::WebContents;
using extensions::ExtensionResource;
using extensions::core_api::web_view_internal::ContentScriptDetails;
using extensions::core_api::web_view_internal::InjectionItems;
using extensions::core_api::web_view_internal::SetPermission::Params;
using extensions::core_api::extension_types::InjectDetails;
using extensions::UserScript;
using ui_zoom::ZoomController;
// error messages for content scripts:
namespace errors = extensions::manifest_errors;
namespace web_view_internal = extensions::core_api::web_view_internal;

using content::RenderViewHost;
using content::WebContentsImpl;
using content::RenderViewHostImpl;
using content::BrowserPluginGuest;

namespace {

const char kAppCacheKey[] = "appcache";
const char kCacheKey[] = "cache";
const char kCookiesKey[] = "cookies";
const char kFileSystemsKey[] = "fileSystems";
const char kIndexedDBKey[] = "indexedDB";
const char kLocalStorageKey[] = "localStorage";
const char kWebSQLKey[] = "webSQL";
const char kSinceKey[] = "since";
const char kLoadFileError[] = "Failed to load file: \"*\". ";
const char kViewInstanceIdError[] = "view_instance_id is missing.";
const char kDuplicatedContentScriptNamesError[] =
    "The given content script name already exists.";

uint32 MaskForKey(const char* key) {
  if (strcmp(key, kAppCacheKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_APPCACHE;
  if (strcmp(key, kCacheKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_CACHE;
  if (strcmp(key, kCookiesKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_COOKIES;
  if (strcmp(key, kFileSystemsKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (strcmp(key, kIndexedDBKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_INDEXEDDB;
  if (strcmp(key, kLocalStorageKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (strcmp(key, kWebSQLKey) == 0)
    return webview::WEB_VIEW_REMOVE_DATA_MASK_WEBSQL;
  return 0;
}

HostID GenerateHostIDFromEmbedder(const extensions::Extension* extension,
                                  const content::WebContents* web_contents) {
  if (extension)
    return HostID(HostID::EXTENSIONS, extension->id());

  if (web_contents && web_contents->GetWebUI()) {
    const GURL& url = web_contents->GetSiteInstance()->GetSiteURL();
    return HostID(HostID::WEBUI, url.spec());
  }
  NOTREACHED();
  return HostID();
}

// Creates content script files when parsing InjectionItems of "js" or "css"
// proterties, and stores them in the |result|.
void AddScriptFiles(const GURL& owner_base_url,
                    const extensions::Extension* extension,
                    const InjectionItems& items,
                    UserScript::FileList* result) {
  // files:
  if (items.files) {
    for (const std::string& relative : *items.files) {
      GURL url = owner_base_url.Resolve(relative);
      if (extension) {
        ExtensionResource resource = extension->GetResource(relative);
        result->push_back(UserScript::File(resource.extension_root(),
                                           resource.relative_path(), url));
      } else {
        result->push_back(extensions::UserScript::File(base::FilePath(),
                                                       base::FilePath(), url));
      }
    }
  }
  // code:
  if (items.code) {
    extensions::UserScript::File file((base::FilePath()), (base::FilePath()),
                                      GURL());
    file.set_content(*items.code);
    result->push_back(file);
  }
}

// Parses the values stored in ContentScriptDetails, and constructs a
// UserScript.
bool ParseContentScript(const ContentScriptDetails& script_value,
                        const extensions::Extension* extension,
                        const GURL& owner_base_url,
                        UserScript* script,
                        std::string* error) {
  // matches (required):
  if (script_value.matches.empty())
    return false;

  // The default for WebUI is not having special access, but we can change that
  // if needed.
  bool allowed_everywhere = false;
  if (extension &&
      extensions::PermissionsData::CanExecuteScriptEverywhere(extension))
    allowed_everywhere = true;

  for (const std::string& match : script_value.matches) {
    URLPattern pattern(UserScript::ValidUserScriptSchemes(allowed_everywhere));
    if (pattern.Parse(match) != URLPattern::PARSE_SUCCESS) {
      *error = errors::kInvalidMatches;
      return false;
    }
    script->add_url_pattern(pattern);
  }

  // exclude_matches:
  if (script_value.exclude_matches) {
    const std::vector<std::string>& exclude_matches =
        *(script_value.exclude_matches.get());
    for (const std::string& exclude_match : exclude_matches) {
      URLPattern pattern(
          UserScript::ValidUserScriptSchemes(allowed_everywhere));

      if (pattern.Parse(exclude_match) != URLPattern::PARSE_SUCCESS) {
        *error = errors::kInvalidExcludeMatches;
        return false;
      }
      script->add_exclude_url_pattern(pattern);
    }
  }
  // run_at:
  if (script_value.run_at) {
    UserScript::RunLocation run_at = UserScript::UNDEFINED;
    switch (script_value.run_at) {
      case extensions::core_api::extension_types::RUN_AT_NONE:
      case extensions::core_api::extension_types::RUN_AT_DOCUMENT_IDLE:
        run_at = UserScript::DOCUMENT_IDLE;
        break;
      case extensions::core_api::extension_types::RUN_AT_DOCUMENT_START:
        run_at = UserScript::DOCUMENT_START;
        break;
      case extensions::core_api::extension_types::RUN_AT_DOCUMENT_END:
        run_at = UserScript::DOCUMENT_END;
        break;
    }
    // The default for run_at is RUN_AT_DOCUMENT_IDLE.
    script->set_run_location(run_at);
  }

  // match_about_blank:
  if (script_value.match_about_blank)
    script->set_match_about_blank(*script_value.match_about_blank);

  // css:
  if (script_value.css) {
    AddScriptFiles(owner_base_url, extension, *script_value.css,
                   &script->css_scripts());
  }

  // js:
  if (script_value.js) {
    AddScriptFiles(owner_base_url, extension, *script_value.js,
                   &script->js_scripts());
  }

  // all_frames:
  if (script_value.all_frames)
    script->set_match_all_frames(*script_value.all_frames);

  // include_globs:
  if (script_value.include_globs) {
    for (const std::string& glob : *script_value.include_globs)
      script->add_glob(glob);
  }

  // exclude_globs:
  if (script_value.exclude_globs) {
    for (const std::string& glob : *script_value.exclude_globs)
      script->add_exclude_glob(glob);
  }

  return true;
}

bool ParseContentScripts(
    std::vector<linked_ptr<ContentScriptDetails>> content_script_list,
    const extensions::Extension* extension,
    const HostID& host_id,
    bool incognito_enabled,
    const GURL& owner_base_url,
    std::set<UserScript>* result,
    std::string* error) {
  if (content_script_list.empty())
    return false;

  std::set<std::string> names;
  for (const linked_ptr<ContentScriptDetails> script_value :
       content_script_list) {
    const std::string& name = script_value->name;
    if (!names.insert(name).second) {
      // The name was already in the list.
      *error = kDuplicatedContentScriptNamesError;
      return false;
    }

    UserScript script;
    if (!ParseContentScript(*script_value, extension, owner_base_url, &script,
                            error))
      return false;

    script.set_id(UserScript::GenerateUserScriptID());
    script.set_name(name);
    script.set_incognito_enabled(incognito_enabled);
    script.set_host_id(host_id);
    script.set_consumer_instance_type(UserScript::WEBVIEW);
    result->insert(script);
  }
  return true;
}

}  // namespace

namespace {
SkBitmap SmartCropAndSize(const SkBitmap& capture,
                          int target_width,
                          int target_height) {
  thumbnails::ClipResult clip_result = thumbnails::CLIP_RESULT_NOT_CLIPPED;
  // Clip it to a more reasonable position.
  SkBitmap clipped_bitmap = thumbnails::SimpleThumbnailCrop::GetClippedBitmap(
      capture, target_width, target_height, &clip_result);
  // Resize the result to the target size.
  SkBitmap result = skia::ImageOperations::Resize(
      clipped_bitmap, skia::ImageOperations::RESIZE_BEST, target_width,
      target_height);

  // NOTE(pettern): Copied from SimpleThumbnailCrop::CreateThumbnail():
#if !defined(USE_AURA)
  // This is a bit subtle. SkBitmaps are refcounted, but the magic
  // ones in PlatformCanvas can't be assigned to SkBitmap with proper
  // refcounting.  If the bitmap doesn't change, then the downsampler
  // will return the input bitmap, which will be the reference to the
  // weird PlatformCanvas one insetad of a regular one. To get a
  // regular refcounted bitmap, we need to copy it.
  //
  // On Aura, the PlatformCanvas is platform-independent and does not have
  // any native platform resources that can't be refounted, so this issue does
  // not occur.
  //
  // Note that GetClippedBitmap() does extractSubset() but it won't copy
  // the pixels, hence we check result size == clipped_bitmap size here.
  if (clipped_bitmap.width() == result.width() &&
      clipped_bitmap.height() == result.height())
    clipped_bitmap.copyTo(&result, kN32_SkColorType);
#endif
  return result;
}

}  // namespace

namespace extensions {

bool WebViewInternalExtensionFunction::RunAsync() {
  int instance_id = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &instance_id));
  WebViewGuest* guest = WebViewGuest::From(
      render_frame_host()->GetProcess()->GetID(), instance_id);
  if (!guest)
    return false;
  // Vivaldi customization
  guest->InitListeners(); // Make sure we set a mouse event callback.
                          // Note: This can be removed if all mouse-gestures is
                          //       moved to the client.

  return RunAsyncSafe(guest);
}

bool WebViewInternalNavigateFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::Navigate::Params> params(
      web_view_internal::Navigate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  std::string src = params->src;
  bool wasTyped = params->was_typed;
  guest->NavigateGuest(src, true /* force_navigation */, wasTyped);
  return true;
}

WebViewInternalExecuteCodeFunction::WebViewInternalExecuteCodeFunction()
    : guest_instance_id_(0), guest_src_(GURL::EmptyGURL()) {
}

WebViewInternalExecuteCodeFunction::~WebViewInternalExecuteCodeFunction() {
}

bool WebViewInternalExecuteCodeFunction::Init() {
  if (details_.get())
    return true;

  if (!args_->GetInteger(0, &guest_instance_id_))
    return false;

  if (!guest_instance_id_)
    return false;

  std::string src;
  if (!args_->GetString(1, &src))
    return false;

  guest_src_ = GURL(src);
  if (!guest_src_.is_valid())
    guest_src_ = GURL::EmptyGURL();

  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(2, &details_value))
    return false;
  scoped_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return false;

  details_ = details.Pass();

  if (extension()) {
    set_host_id(HostID(HostID::EXTENSIONS, extension()->id()));
    return true;
  }

  WebContents* web_contents = GetSenderWebContents();
  if (web_contents && web_contents->GetWebUI()) {
    const GURL& url = render_frame_host()->GetSiteInstance()->GetSiteURL();
    set_host_id(HostID(HostID::WEBUI, url.spec()));
    return true;
  }
  return false;
}

bool WebViewInternalExecuteCodeFunction::ShouldInsertCSS() const {
  return false;
}

bool WebViewInternalExecuteCodeFunction::CanExecuteScriptOnPage() {
  return true;
}

extensions::ScriptExecutor*
WebViewInternalExecuteCodeFunction::GetScriptExecutor() {
  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return NULL;
  WebViewGuest* guest = WebViewGuest::From(
      render_frame_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest)
    return NULL;

  return guest->script_executor();
}

bool WebViewInternalExecuteCodeFunction::IsWebView() const {
  return true;
}

const GURL& WebViewInternalExecuteCodeFunction::GetWebViewSrc() const {
  return guest_src_;
}

bool WebViewInternalExecuteCodeFunction::LoadFileForWebUI(
    const std::string& file_src,
    const WebUIURLFetcher::WebUILoadFileCallback& callback) {
  if (!render_frame_host() || !render_frame_host()->GetProcess())
    return false;
  WebViewGuest* guest = WebViewGuest::From(
      render_frame_host()->GetProcess()->GetID(), guest_instance_id_);
  if (!guest || host_id().type() != HostID::WEBUI)
    return false;

  GURL owner_base_url(guest->GetOwnerSiteURL().GetWithEmptyPath());
  GURL file_url(owner_base_url.Resolve(file_src));

  url_fetcher_.reset(new WebUIURLFetcher(
      this->browser_context(), render_frame_host()->GetProcess()->GetID(),
      render_view_host_do_not_use()->GetRoutingID(), file_url, callback));
  url_fetcher_->Start();
  return true;
}

bool WebViewInternalExecuteCodeFunction::LoadFile(const std::string& file) {
  if (!extension()) {
    if (LoadFileForWebUI(
            *details_->file,
            base::Bind(
                &WebViewInternalExecuteCodeFunction::DidLoadAndLocalizeFile,
                this, file)))
      return true;

    SendResponse(false);
    error_ = ErrorUtils::FormatErrorMessage(kLoadFileError, file);
    return false;
  }
  return ExecuteCodeFunction::LoadFile(file);
}

WebViewInternalExecuteScriptFunction::WebViewInternalExecuteScriptFunction() {
}

void WebViewInternalExecuteScriptFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& on_url,
    const base::ListValue& result) {
  if (error.empty())
    SetResult(result.DeepCopy());
  WebViewInternalExecuteCodeFunction::OnExecuteCodeFinished(
      error, on_url, result);
}

WebViewInternalInsertCSSFunction::WebViewInternalInsertCSSFunction() {
}

bool WebViewInternalInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

WebViewInternalAddContentScriptsFunction::
    WebViewInternalAddContentScriptsFunction() {
}

WebViewInternalAddContentScriptsFunction::
    ~WebViewInternalAddContentScriptsFunction() {
}

ExecuteCodeFunction::ResponseAction
WebViewInternalAddContentScriptsFunction::Run() {
  scoped_ptr<web_view_internal::AddContentScripts::Params> params(
      web_view_internal::AddContentScripts::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->instance_id)
    return RespondNow(Error(kViewInstanceIdError));

  GURL owner_base_url(
      render_frame_host()->GetSiteInstance()->GetSiteURL().GetWithEmptyPath());
  std::set<UserScript> result;

  content::WebContents* sender_web_contents = GetSenderWebContents();
  HostID host_id = GenerateHostIDFromEmbedder(extension(), sender_web_contents);
  bool incognito_enabled = browser_context()->IsOffTheRecord();

  if (!ParseContentScripts(params->content_script_list, extension(), host_id,
                           incognito_enabled, owner_base_url, &result, &error_))
    return RespondNow(Error(error_));

  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);

  manager->AddContentScripts(
      sender_web_contents->GetRenderProcessHost()->GetID(),
      render_view_host_do_not_use(), params->instance_id, host_id, result);

  return RespondNow(NoArguments());
}

WebViewInternalRemoveContentScriptsFunction::
    WebViewInternalRemoveContentScriptsFunction() {
}

WebViewInternalRemoveContentScriptsFunction::
    ~WebViewInternalRemoveContentScriptsFunction() {
}

ExecuteCodeFunction::ResponseAction
WebViewInternalRemoveContentScriptsFunction::Run() {
  scoped_ptr<web_view_internal::RemoveContentScripts::Params> params(
      web_view_internal::RemoveContentScripts::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->instance_id)
    return RespondNow(Error(kViewInstanceIdError));

  WebViewContentScriptManager* manager =
      WebViewContentScriptManager::Get(browser_context());
  DCHECK(manager);

  content::WebContents* sender_web_contents = GetSenderWebContents();
  HostID host_id = GenerateHostIDFromEmbedder(extension(), sender_web_contents);

  std::vector<std::string> script_name_list;
  if (params->script_name_list)
    script_name_list.swap(*params->script_name_list);
  manager->RemoveContentScripts(
      sender_web_contents->GetRenderProcessHost()->GetID(),
      params->instance_id, host_id, script_name_list);
  return RespondNow(NoArguments());
}

WebViewInternalSetNameFunction::WebViewInternalSetNameFunction() {
}

WebViewInternalSetNameFunction::~WebViewInternalSetNameFunction() {
}

bool WebViewInternalSetNameFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetName::Params> params(
      web_view_internal::SetName::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetName(params->frame_name);
  SendResponse(true);
  return true;
}

WebViewInternalSetAllowTransparencyFunction::
WebViewInternalSetAllowTransparencyFunction() {
}

WebViewInternalSetAllowTransparencyFunction::
~WebViewInternalSetAllowTransparencyFunction() {
}

bool WebViewInternalSetAllowTransparencyFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetAllowTransparency::Params> params(
      web_view_internal::SetAllowTransparency::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetAllowTransparency(params->allow);
  SendResponse(true);
  return true;
}

WebViewInternalSetAllowScalingFunction::
    WebViewInternalSetAllowScalingFunction() {
}

WebViewInternalSetAllowScalingFunction::
    ~WebViewInternalSetAllowScalingFunction() {
}

bool WebViewInternalSetAllowScalingFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetAllowScaling::Params> params(
      web_view_internal::SetAllowScaling::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetAllowScaling(params->allow);
  SendResponse(true);
  return true;
}

WebViewInternalSetZoomFunction::WebViewInternalSetZoomFunction() {
}

WebViewInternalSetZoomFunction::~WebViewInternalSetZoomFunction() {
}

bool WebViewInternalSetZoomFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetZoom::Params> params(
      web_view_internal::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetZoom(params->zoom_factor);

  SendResponse(true);
  return true;
}

WebViewInternalGetZoomFunction::WebViewInternalGetZoomFunction() {
}

WebViewInternalGetZoomFunction::~WebViewInternalGetZoomFunction() {
}

bool WebViewInternalGetZoomFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::GetZoom::Params> params(
      web_view_internal::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  double zoom_factor = guest->GetZoom();
  SetResult(new base::FundamentalValue(zoom_factor));
  SendResponse(true);
  return true;
}

WebViewInternalSetZoomModeFunction::WebViewInternalSetZoomModeFunction() {
}

WebViewInternalSetZoomModeFunction::~WebViewInternalSetZoomModeFunction() {
}

bool WebViewInternalSetZoomModeFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetZoomMode::Params> params(
      web_view_internal::SetZoomMode::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_mode) {
    case web_view_internal::ZOOM_MODE_PER_ORIGIN:
      zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
      break;
    case web_view_internal::ZOOM_MODE_PER_VIEW:
      zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      break;
    case web_view_internal::ZOOM_MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
      break;
    default:
      NOTREACHED();
  }

  guest->SetZoomMode(zoom_mode);

  SendResponse(true);
  return true;
}

WebViewInternalGetZoomModeFunction::WebViewInternalGetZoomModeFunction() {
}

WebViewInternalGetZoomModeFunction::~WebViewInternalGetZoomModeFunction() {
}

bool WebViewInternalGetZoomModeFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::GetZoomMode::Params> params(
      web_view_internal::GetZoomMode::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  web_view_internal::ZoomMode zoom_mode = web_view_internal::ZOOM_MODE_NONE;
  switch (guest->GetZoomMode()) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_mode = web_view_internal::ZOOM_MODE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_mode = web_view_internal::ZOOM_MODE_PER_VIEW;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_mode = web_view_internal::ZOOM_MODE_DISABLED;
      break;
    default:
      NOTREACHED();
  }

  SetResult(new base::StringValue(web_view_internal::ToString(zoom_mode)));
  SendResponse(true);
  return true;
}

WebViewInternalFindFunction::WebViewInternalFindFunction() {
}

WebViewInternalFindFunction::~WebViewInternalFindFunction() {
}

bool WebViewInternalFindFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::Find::Params> params(
      web_view_internal::Find::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Convert the std::string search_text to string16.
  base::string16 search_text;
  base::UTF8ToUTF16(
      params->search_text.c_str(), params->search_text.length(), &search_text);

  // Set the find options to their default values.
  blink::WebFindOptions options;
  if (params->options) {
    options.forward =
        params->options->backward ? !*params->options->backward : true;
    options.matchCase =
        params->options->match_case ? *params->options->match_case : false;
  }

  guest->StartFindInternal(search_text, options, this);
  return true;
}

WebViewInternalStopFindingFunction::WebViewInternalStopFindingFunction() {
}

WebViewInternalStopFindingFunction::~WebViewInternalStopFindingFunction() {
}

bool WebViewInternalStopFindingFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::StopFinding::Params> params(
      web_view_internal::StopFinding::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Set the StopFindAction.
  content::StopFindAction action;
  switch (params->action) {
    case web_view_internal::STOP_FINDING_ACTION_CLEAR:
      action = content::STOP_FIND_ACTION_CLEAR_SELECTION;
      break;
    case web_view_internal::STOP_FINDING_ACTION_KEEP:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
      break;
    case web_view_internal::STOP_FINDING_ACTION_ACTIVATE:
      action = content::STOP_FIND_ACTION_ACTIVATE_SELECTION;
      break;
    default:
      action = content::STOP_FIND_ACTION_KEEP_SELECTION;
  }

  guest->StopFindingInternal(action);
  return true;
}

WebViewInternalLoadDataWithBaseUrlFunction::
    WebViewInternalLoadDataWithBaseUrlFunction() {
}

WebViewInternalLoadDataWithBaseUrlFunction::
    ~WebViewInternalLoadDataWithBaseUrlFunction() {
}

bool WebViewInternalLoadDataWithBaseUrlFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<web_view_internal::LoadDataWithBaseUrl::Params> params(
      web_view_internal::LoadDataWithBaseUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // If a virtual URL was provided, use it. Otherwise, the user will be shown
  // the data URL.
  std::string virtual_url =
      params->virtual_url ? *params->virtual_url : params->data_url;

  bool successful = guest->LoadDataWithBaseURL(
      params->data_url, params->base_url, virtual_url, &error_);
  SendResponse(successful);
  return successful;
}

WebViewInternalGoFunction::WebViewInternalGoFunction() {
}

WebViewInternalGoFunction::~WebViewInternalGoFunction() {
}

bool WebViewInternalGoFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::Go::Params> params(
      web_view_internal::Go::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool successful = guest->Go(params->relative_index);
  SetResult(new base::FundamentalValue(successful));
  SendResponse(true);
  return true;
}

WebViewInternalReloadFunction::WebViewInternalReloadFunction() {
}

WebViewInternalReloadFunction::~WebViewInternalReloadFunction() {
}

bool WebViewInternalReloadFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Reload();
  return true;
}

WebViewInternalSetPermissionFunction::WebViewInternalSetPermissionFunction() {
}

WebViewInternalSetPermissionFunction::~WebViewInternalSetPermissionFunction() {
}

bool WebViewInternalSetPermissionFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetPermission::Params> params(
      web_view_internal::SetPermission::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebViewPermissionHelper::PermissionResponseAction action =
      WebViewPermissionHelper::DEFAULT;
  switch (params->action) {
    case core_api::web_view_internal::SET_PERMISSION_ACTION_ALLOW:
      action = WebViewPermissionHelper::ALLOW;
      break;
    case core_api::web_view_internal::SET_PERMISSION_ACTION_DENY:
      action = WebViewPermissionHelper::DENY;
      break;
    case core_api::web_view_internal::SET_PERMISSION_ACTION_DEFAULT:
      break;
    default:
      NOTREACHED();
  }

  std::string user_input;
  if (params->user_input)
    user_input = *params->user_input;

  WebViewPermissionHelper* web_view_permission_helper =
      WebViewPermissionHelper::FromWebContents(guest->web_contents());

  WebViewPermissionHelper::SetPermissionResult result =
      web_view_permission_helper->SetPermission(
          params->request_id, action, user_input);

  EXTENSION_FUNCTION_VALIDATE(result !=
                              WebViewPermissionHelper::SET_PERMISSION_INVALID);

  SetResult(new base::FundamentalValue(
      result == WebViewPermissionHelper::SET_PERMISSION_ALLOWED));
  SendResponse(true);
  return true;
}

WebViewInternalOverrideUserAgentFunction::
    WebViewInternalOverrideUserAgentFunction() {
}

WebViewInternalOverrideUserAgentFunction::
    ~WebViewInternalOverrideUserAgentFunction() {
}

bool WebViewInternalOverrideUserAgentFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<web_view_internal::OverrideUserAgent::Params> params(
      web_view_internal::OverrideUserAgent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  guest->SetUserAgentOverride(params->user_agent_override);
  return true;
}

WebViewInternalStopFunction::WebViewInternalStopFunction() {
}

WebViewInternalStopFunction::~WebViewInternalStopFunction() {
}

bool WebViewInternalStopFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Stop();
  return true;
}

WebViewInternalTerminateFunction::WebViewInternalTerminateFunction() {
}

WebViewInternalTerminateFunction::~WebViewInternalTerminateFunction() {
}

bool WebViewInternalTerminateFunction::RunAsyncSafe(WebViewGuest* guest) {
  guest->Terminate();
  return true;
}

WebViewInternalClearDataFunction::WebViewInternalClearDataFunction()
    : remove_mask_(0), bad_message_(false) {
}

WebViewInternalClearDataFunction::~WebViewInternalClearDataFunction() {
}

// Parses the |dataToRemove| argument to generate the remove mask. Sets
// |bad_message_| (like EXTENSION_FUNCTION_VALIDATE would if this were a bool
// method) if 'dataToRemove' is not present.
uint32 WebViewInternalClearDataFunction::GetRemovalMask() {
  base::DictionaryValue* data_to_remove;
  if (!args_->GetDictionary(2, &data_to_remove)) {
    bad_message_ = true;
    return 0;
  }

  uint32 remove_mask = 0;
  for (base::DictionaryValue::Iterator i(*data_to_remove); !i.IsAtEnd();
       i.Advance()) {
    bool selected = false;
    if (!i.value().GetAsBoolean(&selected)) {
      bad_message_ = true;
      return 0;
    }
    if (selected)
      remove_mask |= MaskForKey(i.key().c_str());
  }

  return remove_mask;
}

// TODO(lazyboy): Parameters in this extension function are similar (or a
// sub-set) to BrowsingDataRemoverFunction. How can we share this code?
bool WebViewInternalClearDataFunction::RunAsyncSafe(WebViewGuest* guest) {
  // Grab the initial |options| parameter, and parse out the arguments.
  base::DictionaryValue* options;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &options));
  DCHECK(options);

  // If |ms_since_epoch| isn't set, default it to 0.
  double ms_since_epoch;
  if (!options->GetDouble(kSinceKey, &ms_since_epoch)) {
    ms_since_epoch = 0;
  }

  // base::Time takes a double that represents seconds since epoch. JavaScript
  // gives developers milliseconds, so do a quick conversion before populating
  // the object. Also, Time::FromDoubleT converts double time 0 to empty Time
  // object. So we need to do special handling here.
  remove_since_ = (ms_since_epoch == 0)
                      ? base::Time::UnixEpoch()
                      : base::Time::FromDoubleT(ms_since_epoch / 1000.0);

  remove_mask_ = GetRemovalMask();
  if (bad_message_)
    return false;

  AddRef();  // Balanced below or in WebViewInternalClearDataFunction::Done().

  bool scheduled = false;
  if (remove_mask_) {
    scheduled = guest->ClearData(
        remove_since_,
        remove_mask_,
        base::Bind(&WebViewInternalClearDataFunction::ClearDataDone, this));
  }
  if (!remove_mask_ || !scheduled) {
    SendResponse(false);
    Release();  // Balanced above.
    return false;
  }

  // Will finish asynchronously.
  return true;
}

void WebViewInternalClearDataFunction::ClearDataDone() {
  Release();  // Balanced in RunAsync().
  SendResponse(true);
}

WebViewInternalSetVisibleFunction::WebViewInternalSetVisibleFunction() {
}

WebViewInternalSetVisibleFunction::~WebViewInternalSetVisibleFunction() {
}

bool WebViewInternalSetVisibleFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetVisible::Params> params(
      web_view_internal::SetVisible::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetVisible(params->is_visible);
  return true;
}

namespace {
const float kDefaultThumbnailScale = 1.0f;
}  // namespace

WebViewInternalGetThumbnailFunction::WebViewInternalGetThumbnailFunction()
    : image_format_(core_api::extension_types::IMAGE_FORMAT_JPEG),  // Default
                                                                   // format is
                                                                   // PNG.
      image_quality_(90 /*kDefaultQuality*/),  // Default quality setting.
      scale_(kDefaultThumbnailScale),  // Scale of window dimension to thumb.
      height_(0),
      width_(0) {
}

WebViewInternalGetThumbnailFunction::~WebViewInternalGetThumbnailFunction() {
}

bool WebViewInternalGetThumbnailFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::GetThumbnail::Params> params(
      web_view_internal::GetThumbnail::Params::Create(*args_));

  if (params.get() && params->dimension.get()) {
    if (params->dimension.get()->scale.get())
      scale_ = *params->dimension.get()->scale.get();
    if (params->dimension.get()->width.get())
      width_ = *params->dimension.get()->width.get();
    if (params->dimension.get()->height.get())
      height_ = *params->dimension.get()->height.get();
  }

  WebContents* web_contents = guest->web_contents();

  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  content::RenderWidgetHostView* view = render_view_host->GetView();

  if (!view) {
    //error_ = keys::kInternalVisibleTabCaptureError;
    error_ = "Error: View is not available, no screenshot taken.";
    return false;
  }
  if (!guest->IsVisible()) {
    error_ = "Error: Guest is not visible, no screenshot taken.";
    return false;
  }

  RenderViewHost* embedder_render_view_host =
      guest->embedder_web_contents()->GetRenderViewHost();
  gfx::Point source_origin =
      view->GetViewBounds().origin() -
      embedder_render_view_host->GetView()->GetViewBounds().OffsetFromOrigin();
  gfx::Rect source_rect(source_origin, view->GetViewBounds().size());

  // Remove scrollbars from thumbnail (even if they're not here!)
  source_rect.set_width(
      std::max(1, source_rect.width() - gfx::scrollbar_size()));
  source_rect.set_height(
      std::max(1, source_rect.height() - gfx::scrollbar_size()));

  embedder_render_view_host->CopyFromBackingStore(
      source_rect, source_rect.size(),
      base::Bind(
          &WebViewInternalGetThumbnailFunction::CopyFromBackingStoreComplete,
          this),
      kN32_SkColorType);

  return true;
}

void WebViewInternalGetThumbnailFunction::CopyFromBackingStoreComplete(
  const SkBitmap& bitmap,
  content::ReadbackResponse response) {

  if (response == content::READBACK_SUCCESS) {
    VLOG(1) << "captureVisibleTab() got image from backing store.";
    SendResultFromBitmap(bitmap);
    return;
  }
}

bool WebViewInternalGetThumbnailFunction::EncodeBitmap(
  const SkBitmap& screen_capture, std::vector<unsigned char>& data,
  std::string& mime_type) {
  SkAutoLockPixels screen_capture_lock(screen_capture);
  gfx::Size dst_size_pixels;
  if (width_ && height_) {
    dst_size_pixels.SetSize(width_, height_);
  } else {
    dst_size_pixels = gfx::ToRoundedSize(gfx::ScaleSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_));
  }

  SkBitmap bitmap = skia::ImageOperations::Resize(
    screen_capture,
    skia::ImageOperations::RESIZE_BEST,
    dst_size_pixels.width(),
    dst_size_pixels.height());

  bool encoded = false;

  SkAutoLockPixels lock(bitmap);

  switch (image_format_) {
  case core_api::extension_types::IMAGE_FORMAT_JPEG:
    if (bitmap.getPixels()) {
      encoded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bitmap.width(),
        bitmap.height(),
        static_cast<int>(bitmap.rowBytes()),
        image_quality_,
        &data);
      mime_type = "image/jpeg";// kMimeTypeJpeg;
    }
    break;
  case core_api::extension_types::IMAGE_FORMAT_PNG:
    encoded = gfx::PNGCodec::EncodeBGRASkBitmap(
      bitmap,
      true,  // Discard transparency.
      &data);
    mime_type = "image/png";// kMimeTypePng;
    break;
  default:
    NOTREACHED() << "Invalid image format.";
  }

  return encoded;
}

void WebViewInternalGetThumbnailFunction::SendInternalError() {
  error_ = "Internal Thumbnail error";
  SendResponse(false);
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void WebViewInternalGetThumbnailFunction::SendResultFromBitmap(
  const SkBitmap& screen_capture) {
  std::vector<unsigned char> data;
  std::string mime_type;
  gfx::Size dst_size_pixels;
  SkBitmap bitmap;

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    dst_size_pixels = gfx::ToRoundedSize(gfx::ScaleSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_));
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
  } else if (width_ != 0 && height_ != 0) {
    bitmap = SmartCropAndSize(screen_capture, width_, height_);
  } else {
    bitmap = screen_capture;
  }
  bool encoded = EncodeBitmap(bitmap, data, mime_type);
  if (!encoded) {
    error_ = "Internal Thumbnail error";
    SendResponse(false);
    return;
  }
  std::string base64_result;
  base::StringPiece stream_as_string(
    reinterpret_cast<const char*>(vector_as_array(&data)), data.size());

  base::Base64Encode(stream_as_string, &base64_result);
  base64_result.insert(0, base::StringPrintf("data:%s;base64,",
    mime_type.c_str()));
  SetResult(new base::StringValue(base64_result));
  SendResponse(true);
}

WebViewInternalGetThumbnailFromServiceFunction::
    WebViewInternalGetThumbnailFromServiceFunction() {
}

WebViewInternalGetThumbnailFromServiceFunction::
    ~WebViewInternalGetThumbnailFromServiceFunction() {
}

bool WebViewInternalGetThumbnailFromServiceFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<web_view_internal::AddToThumbnailService::Params> params(
      web_view_internal::AddToThumbnailService::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  url_ = guest->web_contents()->GetURL();

  return WebViewInternalGetThumbnailFunction::RunAsyncSafe(guest);
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void WebViewInternalGetThumbnailFromServiceFunction::SendResultFromBitmap(
  const SkBitmap& screen_capture) {

  // TODO:  For now we partially use the thumbnail_service, as we take our own
  // thumbnail but store it in the service.  When thumbnailing webviews bug
  // (http://crbug.com//327035) is fixed we should consider also letting the
  // the thumbnail_service take the thumbnail.

#ifdef VIVALDI_BUILD_HAS_CHROME_CODE
  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
    ThumbnailServiceFactory::GetForProfile(profile);

  // Scale the  bitmap.
  SkAutoLockPixels screen_capture_lock(screen_capture);
  gfx::Size dst_size_pixels;
  SkBitmap bitmap;

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    dst_size_pixels = gfx::ToRoundedSize(gfx::ScaleSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_));
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
  } else {
    bitmap = SmartCropAndSize(screen_capture, width_, height_);
  }
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);
  scoped_refptr<thumbnails::ThumbnailingContext> context(
      new thumbnails::ThumbnailingContext(url_, thumbnail_service.get()));
  context->score.force_update = true;

  if (!context->url.is_valid()) {
    SendResponse(false);
    return;
  }

  thumbnail_service->AddForcedURL(context->url);
  thumbnail_service->SetPageThumbnail(*context, image);

  SetResult(new base::StringValue(std::string("chrome://thumb/") +
                                  context->url.spec()));
#endif  // VIVALDI_BUILD_HAS_CHROME_CODE
  SendResponse(true);
}

WebViewInternalAddToThumbnailServiceFunction::
    WebViewInternalAddToThumbnailServiceFunction() {
}

WebViewInternalAddToThumbnailServiceFunction::
    ~WebViewInternalAddToThumbnailServiceFunction() {
}

bool WebViewInternalAddToThumbnailServiceFunction::RunAsyncSafe(
    WebViewGuest* guest) {
  scoped_ptr<web_view_internal::AddToThumbnailService::Params> params(
      web_view_internal::AddToThumbnailService::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!params->key.empty())
    key_ = params->key;

  url_ = guest->web_contents()->GetURL();

  if (params->dimensions.get()) {
    if (params->dimensions.get()->store_as_current_url.get()) {
      store_as_current_url_ =
          *params->dimensions.get()->store_as_current_url.get();
    }
    if (params->dimensions.get()->scale.get())
      scale_ = *params->dimensions.get()->scale.get();
    if (params->dimensions.get()->width.get())
      width_ = *params->dimensions.get()->width.get();
    if (params->dimensions.get()->height.get())
      height_ = *params->dimensions.get()->height.get();
  }

  return WebViewInternalGetThumbnailFunction::RunAsyncSafe(guest);
}

void WebViewInternalAddToThumbnailServiceFunction::SetPageThumbnailOnUIThread(
    bool send_result,
    scoped_refptr<thumbnails::ThumbnailService> thumbnail_service,
    scoped_refptr<thumbnails::ThumbnailingContext> context,
    const gfx::Image& thumbnail) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  thumbnail_service->SetPageThumbnail(*context, thumbnail);

  if (send_result) {
    SetResult(new base::StringValue(std::string("chrome://thumb/") +
                                    context->url.spec()));
    SendResponse(true);
  }
  Release();
}

// Turn a bitmap of the screen into an image, set that image as the result,
// and call SendResponse().
void WebViewInternalAddToThumbnailServiceFunction::SendResultFromBitmap(
  const SkBitmap& screen_capture) {

  // TODO:  For now we partially use the thumbnail_service, as we take our own
  // thumbnail but store it in the service.  When thumbnailing webviews bug
  // (http://crbug.com//327035) is fixed we should consider also letting the
  // the thumbnail_service take the thumbnail.

  Profile* profile = Profile::FromBrowserContext(browser_context());
  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service =
    ThumbnailServiceFactory::GetForProfile(profile);

  // Scale the  bitmap.
  SkAutoLockPixels screen_capture_lock(screen_capture);
  gfx::Size dst_size_pixels;
  SkBitmap bitmap;

  if (scale_ != kDefaultThumbnailScale) {
    // Scale has changed, use that.
    dst_size_pixels = gfx::ToRoundedSize(gfx::ScaleSize(
        gfx::Size(screen_capture.width(), screen_capture.height()), scale_));
    bitmap = skia::ImageOperations::Resize(
        screen_capture, skia::ImageOperations::RESIZE_BEST,
        dst_size_pixels.width(), dst_size_pixels.height());
  } else {
    bitmap = SmartCropAndSize(screen_capture, width_, height_);
  }
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(bitmap);

  scoped_refptr<thumbnails::ThumbnailingContext> context(
      new thumbnails::ThumbnailingContext(GURL(key_), thumbnail_service.get()));
  context->score.force_update = true;

  if (!context->url.is_valid()) {
    SendResponse(false);
    return;
  }
  AddRef();

  // NOTE(pettern): AddForcedURL is async, so we need to ensure adding the
  // thumbnail is too, to avoid it being added as a non-known url.
  if (store_as_current_url_) {
    AddRef();
    scoped_refptr<thumbnails::ThumbnailingContext> url_context(
        new thumbnails::ThumbnailingContext(url_, thumbnail_service.get()));
    thumbnail_service->AddForcedURL(url_context->url);
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&WebViewInternalAddToThumbnailServiceFunction::
                       SetPageThumbnailOnUIThread,
                   this, false, thumbnail_service, url_context, image),
        base::TimeDelta::FromMilliseconds(200));
  }
  thumbnail_service->AddForcedURL(context->url);
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&WebViewInternalAddToThumbnailServiceFunction::
                     SetPageThumbnailOnUIThread,
                 this, true, thumbnail_service, context, image),
      base::TimeDelta::FromMilliseconds(200));
}

WebViewInternalShowPageInfoFunction::WebViewInternalShowPageInfoFunction() {
}

WebViewInternalShowPageInfoFunction::~WebViewInternalShowPageInfoFunction() {
}

bool WebViewInternalShowPageInfoFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::ShowPageInfo::Params> params(
      web_view_internal::ShowPageInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  gfx::Point pos(params->position.left, params->position.top);
  guest->ShowPageInfo(pos);
  return true;
}

WebViewInternalSetIsFullscreenFunction::
    WebViewInternalSetIsFullscreenFunction() {
}

WebViewInternalSetIsFullscreenFunction::
    ~WebViewInternalSetIsFullscreenFunction() {
}

bool WebViewInternalSetIsFullscreenFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetIsFullscreen::Params> params(
      web_view_internal::SetIsFullscreen::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  guest->SetIsFullscreen(params->is_fullscreen);
  return true;
}

WebViewInternalSetShowImagesFunction::WebViewInternalSetShowImagesFunction() {}

WebViewInternalSetShowImagesFunction::~WebViewInternalSetShowImagesFunction() {}

bool WebViewInternalSetShowImagesFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::SetShowImages::Params> params(
      web_view_internal::SetShowImages::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  WebContents* web_contents = guest->web_contents();
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);

  web_contents_impl->SetShouldShowImages(params->show_images);

  if (params->only_load_from_cache) {
    web_contents_impl->SetOnlyLoadFromCache(
        *params->only_load_from_cache.get());
  }

  if (params->enable_plugins) {
    web_contents_impl->SetEnablePlugins(*params->enable_plugins.get());
  }

  content::RendererPreferences* prefs = web_contents->GetMutableRendererPrefs();
  if (params->only_load_from_cache && *params->only_load_from_cache.get() &&
      params->enable_plugins && *params->enable_plugins.get()) {
    prefs->should_ask_plugin_content = true;
  } else {
    prefs->should_ask_plugin_content = false;
  }
  web_contents->GetRenderViewHost()->SyncRendererPrefs();

  return true;
}

WebViewInternalGetPageHistoryFunction::WebViewInternalGetPageHistoryFunction() {
}

WebViewInternalGetPageHistoryFunction::
    ~WebViewInternalGetPageHistoryFunction() {
}

bool WebViewInternalGetPageHistoryFunction::RunAsyncSafe(WebViewGuest* guest) {
  scoped_ptr<web_view_internal::GetPageHistory::Params> params(
      web_view_internal::GetPageHistory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  content::NavigationController& controller =
      guest->web_contents()->GetController();

  int currentEntryIndex = controller.GetCurrentEntryIndex();

  std::vector<linked_ptr<
      web_view_internal::GetPageHistory::Results::PageHistoryType>> history;

  for (int i = 0; i < controller.GetEntryCount(); i++) {
    content::NavigationEntry* entry = controller.GetEntryAtIndex(i);
    base::string16 name = entry->GetTitleForDisplay(std::string());
    linked_ptr<web_view_internal::GetPageHistory::Results::PageHistoryType>
        history_entry(
            new web_view_internal::GetPageHistory::Results::PageHistoryType);
    history_entry->name = base::UTF16ToUTF8(name);
    std::string urlstr = entry->GetVirtualURL().spec();
    history_entry->url = urlstr;
    history_entry->index = i;
    history.push_back(history_entry);
  }
  results_ = web_view_internal::GetPageHistory::Results::Create(
      currentEntryIndex, history);

  SendResponse(true);

  return true;
}

}  // namespace extensions
