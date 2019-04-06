// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

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

}  // namespace extensions
