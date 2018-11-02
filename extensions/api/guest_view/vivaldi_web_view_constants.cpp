// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/guest_view/vivaldi_web_view_constants.h"

namespace webview {

const char kAttributeExtensionHost[] = "extensionhost";

const char kEventSSLStateChanged[] = "webViewPrivate.onSSLStateChanged";
const char kEventTargetURLChanged[] = "webViewPrivate.onTargetURLChanged";
const char kEventCreateSearch[] = "webViewPrivate.onCreateSearch";
const char kEventMediaStateChanged[] = "webViewPrivate.onMediaStateChanged";
const char kEventPasteAndGo[] = "webViewPrivate.onPasteAndGo";
const char kEventSimpleAction[] = "webViewPrivate.onSimpleAction";
const char kEventWebContentsDiscarded[] =
    "webViewPrivate.onWebcontentsDiscarded";
const char kEventOnFullscreen[] = "webViewPrivate.onFullscreen";
const char kEventContentBlocked[] = "webViewPrivate.onContentBlocked";

const char kNewSearchName[] = "Name";
const char kNewSearchUrl[] = "Url";
const char kClipBoardText[] = "clipBoardText";
const char kPasteTarget[] = "pasteTarget";  // url, search
const char kModifiers[] = "modifiers";
const char kGenCommand[] = "command";
const char kGenText[] = "text";
const char kGenUrl[] = "url";

const char kLoadedBytes[] = "loadedBytes";
const char kLoadedElements[] = "loadedElements";
const char kTotalElements[] = "totalElements";
}  // namespace webview
