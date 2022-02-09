// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_GUEST_VIEW_VIVALDI_WEB_VIEW_CONSTANTS_H_
#define EXTENSIONS_API_GUEST_VIEW_VIVALDI_WEB_VIEW_CONSTANTS_H_

// std::string
#include <string>

// WindowOpenDisposition
#include "ui/base/window_open_disposition.h"

namespace webview {

extern const char kEventSSLStateChanged[];
extern const char kEventTargetURLChanged[];
extern const char kEventCreateSearch[];
extern const char kEventPasteAndGo[];
extern const char kAttributeExtensionHost[];
extern const char kEventOnFullscreen[];
extern const char kEventContentAllowed[];
extern const char kEventContentBlocked[];
extern const char kEventWebContentsCreated[];
extern const char kEventWindowBlocked[];
extern const char kNewSearchName[];
extern const char kNewSearchUrl[];
extern const char kClipBoardText[];
extern const char kPasteTarget[];
extern const char kModifiers[];
extern const char kLoadedBytes[];
extern const char kLoadedElements[];
extern const char kTotalElements[];
extern const char kGenCommand[];
extern const char kGenText[];
extern const char kGenUrl[];

// Parameters/properties on events.
extern const char kInitialTop[];
extern const char kInitialLeft[];

}  // namespace webview

#endif  // EXTENSIONS_API_GUEST_VIEW_VIVALDI_WEB_VIEW_CONSTANTS_H_
