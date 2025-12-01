// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

#include "base/values.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/guest_view/vivaldi_guest_view_constants.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/vivaldi_browser_component_wrapper.h"

namespace extensions {

void WebViewPermissionHelper::SetDownloadInformation(
    const content::DownloadInformation& info) {
  download_info_ = info;
}

void WebViewPermissionHelperDelegate::SetDownloadInformation(
    const content::DownloadInformation& info) {
  download_info_ = info;
}

void WebViewPermissionHelper::RegisterProtocolHandler(
  content::RenderFrameHost* requesting_frame,
  const std::string& protocol,
  const GURL& url,
  bool user_gesture) {

  // TODO: Should we keep everything protocolhandler browser side.
  custom_handlers::ProtocolHandler handler =
      custom_handlers::ProtocolHandler::CreateProtocolHandler(
          protocol, url, blink::ProtocolHandlerSecurityLevel::kStrict);
  DCHECK(handler.IsValid());

  VivaldiBrowserComponentWrapper::GetInstance()->HandleRegisterHandlerRequest(
    web_view_guest()->web_contents(), &handler);

  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, url.spec());

  std::u16string protocolDisplay = handler.GetProtocolDisplayName();
  request_info.Set(guest_view::kProtocolDisplayName, protocolDisplay);
  request_info.Set(guest_view::kSuppressedPrompt, !user_gesture);
  WebViewPermissionType request_type = WEB_VIEW_PROTOCOL_HANDLING;

  RequestPermission(
    request_type, std::move(request_info),
    base::BindOnce(&WebViewPermissionHelper::OnProtocolPermissionResponse,
      weak_factory_.GetWeakPtr()),
    false);
}

void WebViewPermissionHelper::OnProtocolPermissionResponse(
  bool allow,
  const std::string& user_input) {

  VivaldiBrowserComponentWrapper::GetInstance()->SetOrRollbackProtocolHandler(
      web_view_guest()->web_contents(), allow);

}


}  // namespace extensions
