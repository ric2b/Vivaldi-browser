// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

// Included from extensions/browser/guest_view/guest_view_events.cc

#ifndef EXTENSIONS_GUEST_VIEW_VIVALDI_GUEST_VIEW_EVENTS_H
#define EXTENSIONS_GUEST_VIEW_VIVALDI_GUEST_VIEW_EVENTS_H

//struct NameAndValue {
//      const char* name;
//      events::HistogramValue value;
//    } names_and_values[] = {

        {webview::kEventOnFullscreen, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventRequestPageInfo, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventSSLStateChanged, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventTargetURLChanged, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventCreateSearch, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventMediaStateChanged, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventPasteAndGo, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventSimpleAction, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventWebContentsDiscarded, events::VIVALDI_EXTENSION_EVENT},
        {webview::kEventContentBlocked, events::VIVALDI_EXTENSION_EVENT},

//    };
#endif // EXTENSIONS_GUEST_VIEW_VIVALDI_GUEST_VIEW_EVENTS_H
