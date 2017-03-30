// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/guest_view/vivaldi_web_view_constants.h"

namespace webview {

const char kAttributeExtensionHost[] = "extensionhost";

const char kEventRequestPageInfo[] = "webViewPrivate.onRequestPageInfo";
const char kEventSSLStateChanged[] = "webViewPrivate.onSSLStateChanged";
const char kEventTargetURLChanged[] = "webViewPrivate.onTargetURLChanged";
const char kEventCreateSearch[] = "webViewPrivate.onCreateSearch";
const char kEventMediaStateChanged[] = "webViewPrivate.onMediaStateChanged";
const char kEventPasteAndGo[] = "webViewPrivate.onPasteAndGo";
const char kEventWebContentsDiscarded[] =
    "webViewPrivate.onWebcontentsDiscarded";
const char kEventOnFullscreen[] = "webViewPrivate.onFullscreen";

const char kNewSearchName[] = "Name";
const char kNewSearchUrl[] = "Url";
const char kClipBoardText[] = "clipBoardText";

const char kLoadedBytes[] = "loadedBytes";
const char kLoadedElements[] = "loadedElements";
const char kTotalElements[] = "totalElements";

}
