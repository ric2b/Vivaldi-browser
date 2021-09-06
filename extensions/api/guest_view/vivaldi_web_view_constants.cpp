// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/guest_view/vivaldi_web_view_constants.h"

namespace webview {

const char kEventSSLStateChanged[] = "webViewPrivate.onSSLStateChanged";
const char kEventTargetURLChanged[] = "webViewPrivate.onTargetURLChanged";
const char kEventCreateSearch[] = "webViewPrivate.onCreateSearch";
const char kEventPasteAndGo[] = "webViewPrivate.onPasteAndGo";
const char kEventOnFullscreen[] = "webViewPrivate.onFullscreen";
const char kEventContentAllowed[] = "webViewPrivate.onContentAllowed";
const char kEventContentBlocked[] = "webViewPrivate.onContentBlocked";
const char kEventWebContentsCreated[] = "webViewPrivate.onWebcontentsCreated";
const char kEventWindowBlocked[] = "webViewPrivate.onWindowBlocked";

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

// Parameters/properties on events.
const char kInitialTop[] = "initialTop";
const char kInitialLeft[] = "initialLeft";

}  // namespace webview
