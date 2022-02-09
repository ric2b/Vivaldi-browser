// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLINK_VIVALDI_ADD_SEARCH_CONTEXT_MENU_H_
#define COMPONENTS_BLINK_VIVALDI_ADD_SEARCH_CONTEXT_MENU_H_

namespace blink {
class FrameSelection;
class HTMLInputElement;
class WebURL;
}  // namespace blink

using blink::FrameSelection;
using blink::HTMLInputElement;
using blink::WebURL;

namespace vivaldi {

WebURL GetWebSearchableUrl(const FrameSelection& currentSelection,
                           HTMLInputElement* element);
}

#endif  // COMPONENTS_BLINK_VIVALDI_ADD_SEARCH_CONTEXT_MENU_H_
