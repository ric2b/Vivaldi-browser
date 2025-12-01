// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PERMISSIONS_VIVALDI_PERMISSION_PERSISTENCE_H_
#define COMPONENTS_PERMISSIONS_VIVALDI_PERMISSION_PERSISTENCE_H_

namespace extensions {
class WebViewGuest;
}

namespace vivaldi {

// VB-111008: Persist geolocation permission for webview context
void PersistGeolocationPermission(extensions::WebViewGuest* web_view_guest,
                                  bool allow);

}  // namespace vivaldi

#endif  // COMPONENTS_PERMISSIONS_VIVALDI_PERMISSION_PERSISTENCE_H_