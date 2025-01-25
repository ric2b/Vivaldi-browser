// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

#include "base/values.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/register_protocol_handler_permission_request.h"
#include "components/guest_view/vivaldi_guest_view_constants.h"
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

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
  custom_handlers::ProtocolHandler handler =
    custom_handlers::ProtocolHandler::CreateProtocolHandler(
      protocol, url, blink::ProtocolHandlerSecurityLevel::kStrict);

  custom_handlers::ProtocolHandlerRegistry* registry =
    ProtocolHandlerRegistryFactory::GetForBrowserContext(
      web_view_guest()->web_contents()->GetBrowserContext());

  DCHECK(handler.IsValid());

  if (registry->SilentlyHandleRegisterHandlerRequest(handler)) {
    return;
  }

  auto* page_content_settings_delegate =
    PageSpecificContentSettingsDelegate::FromWebContents(
      web_view_guest()->web_contents());

  page_content_settings_delegate->set_pending_protocol_handler(handler);
  page_content_settings_delegate->set_previous_protocol_handler(
    registry->GetHandlerFor(handler.protocol()));

  //base::DictionaryValue request_info;
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
  custom_handlers::ProtocolHandlerRegistry* registry =
    ProtocolHandlerRegistryFactory::GetForBrowserContext(
      web_view_guest()->web_contents()->GetBrowserContext());

  auto* content_settings =
    PageSpecificContentSettingsDelegate::FromWebContents(
      web_view_guest()->web_contents());

  auto pending_handler = content_settings->pending_protocol_handler();

  if (allow) {
    registry->RemoveIgnoredHandler(pending_handler);

    registry->OnAcceptRegisterProtocolHandler(pending_handler);
    PageSpecificContentSettingsDelegate::FromWebContents(
      web_view_guest()->web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_ALLOW);

  }
  else {
    registry->OnIgnoreRegisterProtocolHandler(pending_handler);
    PageSpecificContentSettingsDelegate::FromWebContents(
      web_view_guest()->web_contents())
      ->set_pending_protocol_handler_setting(CONTENT_SETTING_BLOCK);

    auto previous_handler = content_settings->previous_protocol_handler();

    if (previous_handler.IsEmpty()) {
      registry->ClearDefault(pending_handler.protocol());
    }
    else {
      registry->OnAcceptRegisterProtocolHandler(previous_handler);
    }
  }
}


}  // namespace extensions
