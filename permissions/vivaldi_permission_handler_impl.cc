// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "permissions/vivaldi_permission_handler_impl.h"

#include "components/content_settings/core/common/content_settings.h"
#include "extensions/api/tabs/tabs_private_api.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#include "components/permissions/permission_request.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

#include "base/no_destructor.h"

#include "app/vivaldi_apptools.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"

namespace vivaldi {
namespace permissions {

namespace {
bool ConvertPermissionType(::permissions::RequestType type,
                           WebViewPermissionType* out_type) {
  switch (type) {
    case ::permissions::RequestType::kNotifications:
      *out_type = WEB_VIEW_PERMISSION_TYPE_NOTIFICATION;
      return true;
    case ::permissions::RequestType::kGeolocation:
      *out_type = WEB_VIEW_PERMISSION_TYPE_GEOLOCATION;
      return true;
    case ::permissions::RequestType::kMicStream:
      *out_type = WEB_VIEW_PERMISSION_TYPE_MICROPHONE;
      return true;
    case ::permissions::RequestType::kCameraStream:
      *out_type = WEB_VIEW_PERMISSION_TYPE_CAMERA;
      return true;
    case ::permissions::RequestType::kMidiSysex:
      *out_type = WEB_VIEW_PERMISSION_TYPE_MIDI_SYSEX;
      return true;
    case ::permissions::RequestType::kRegisterProtocolHandler:
      *out_type = WEB_VIEW_PROTOCOL_HANDLING;
      return true;
    case ::permissions::RequestType::kClipboard:
      *out_type = WEB_VIEW_PERMISSION_TYPE_CLIPBOARD;
      return true;
    case ::permissions::RequestType::kIdleDetection:
      *out_type = WEB_VIEW_PERMISSION_TYPE_IDLE_DETECTION;
      return true;
    // ----------------------------------------------.
    // Following are unsupported by our handling code.
    // ----------------------------------------------.
    case ::permissions::RequestType::kArSession:
    case ::permissions::RequestType::kCameraPanTiltZoom:
    case ::permissions::RequestType::kCapturedSurfaceControl:
    case ::permissions::RequestType::kTopLevelStorageAccess:
    case ::permissions::RequestType::kDiskQuota:
    case ::permissions::RequestType::kFileSystemAccess:
    case ::permissions::RequestType::kIdentityProvider:
    case ::permissions::RequestType::kLocalFonts:
    case ::permissions::RequestType::kMultipleDownloads:
    case ::permissions::RequestType::kKeyboardLock:
    case ::permissions::RequestType::kPointerLock:
    case ::permissions::RequestType::kStorageAccess:
    case ::permissions::RequestType::kVrSession:
    case ::permissions::RequestType::kWindowManagement:
    case ::permissions::RequestType::kHandTracking:
    case ::permissions::RequestType::kWebAppInstallation:
    // This one is only available on some platforms...
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    case ::permissions::RequestType::kProtectedMediaIdentifier:
#endif
      return false;
  }

  return false;
}

void CallbackContentSettingWrapper(
    base::WeakPtr<::permissions::PermissionRequest> request,
    bool allowed,
    const std::string&) {
  // Request weak ptr was already invalidated...
  if (!request)
    return;

  if (allowed) {
    request->PermissionGranted(false);
  } else {
    request->PermissionDenied();
  }
}

}  // namespace

// static
VivaldiPermissionHandlerImpl* VivaldiPermissionHandlerImpl::GetInstance() {
  static base::NoDestructor<VivaldiPermissionHandlerImpl> instance;
  return instance.get();
}

void VivaldiPermissionHandlerImpl::NotifyPermissionSet(
    const ::permissions::PermissionRequestID& id,
    ContentSettingsType type,
    ContentSetting setting) {
  // We're only interested in allow/block events.
  if (setting != CONTENT_SETTING_ALLOW && setting != CONTENT_SETTING_BLOCK)
    return;

  auto frame_id = id.global_render_frame_host_id();
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(frame_id);

  if (!render_frame_host) {
    DLOG(ERROR) << "RenderFrameHost not found for frame_id " << frame_id;
    return;
  }

  // Get web_contents from render_frame_host
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents) {
    DLOG(ERROR) << "WebContents not found for RenderFrameHost "
                << render_frame_host;
    return;
  }

  extensions::VivaldiPrivateTabObserver* private_tab =
      extensions::VivaldiPrivateTabObserver::FromWebContents(web_contents);
  if (!private_tab) {
    DLOG(ERROR) << "VivaldiPrivateTabObserver not found for WebContents "
                << web_contents;
    return;
  }

  // Blacklisting some permissions we don't want to be notified about.
  // This one is silent in chrome - and it's just confusing to get it next to
  // the clipboard permission.
  if (type == ContentSettingsType::CLIPBOARD_SANITIZED_WRITE) return;

  // Get the security origin URL from web_contents.
  GURL security_origin = render_frame_host->GetLastCommittedOrigin().GetURL();
  private_tab->OnPermissionAccessed(type, security_origin.spec(), setting);
}

bool VivaldiPermissionHandlerImpl::HandlePermissionRequest(
    const content::GlobalRenderFrameHostId& source_frame_id,
    ::permissions::PermissionRequest* request) {
  if (!::vivaldi::IsVivaldiRunning())
    return false;

  GURL requesting_frame_origin =
      request->requesting_origin().DeprecatedGetOriginAsURL();

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHostId(
          source_frame_id);

  if (!web_view_permission_helper)
    return false;

  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, requesting_frame_origin.spec());

  WebViewPermissionType permission_type;

  if (!ConvertPermissionType(request->request_type(), &permission_type))
    return false;

  web_view_permission_helper->RequestPermission(
      permission_type, std::move(request_info),
      base::BindOnce(&CallbackContentSettingWrapper, request->GetWeakPtr()),
      false);
  return true;
}

}  // namespace permissions
}  // namespace vivaldi
