// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

// Included from extensions/browser/guest_view/guest_view_events.cc

#ifndef EXTENSIONS_API_GUEST_VIEW_VIVALDI_GUEST_VIEW_EVENTS_H_
#define EXTENSIONS_API_GUEST_VIEW_VIVALDI_GUEST_VIEW_EVENTS_H_

// struct NameAndValue {
//   const char* name;
//   events::HistogramValue value;
// } names_and_values[] = {

// clang-format off
    {webview::kEventOnFullscreen, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventSSLStateChanged, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventTargetURLChanged, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventCreateSearch, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventPasteAndGo, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventContentAllowed, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventContentBlocked, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventWebContentsCreated, events::VIVALDI_EXTENSION_EVENT},
    {webview::kEventWindowBlocked, events::VIVALDI_EXTENSION_EVENT},
// clang-format on

// };
#endif  // EXTENSIONS_API_GUEST_VIEW_VIVALDI_GUEST_VIEW_EVENTS_H_
