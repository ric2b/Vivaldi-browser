// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_CLIPBOARD_UTILS_H
#define BROWSER_VIVALDI_CLIPBOARD_UTILS_H

#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/clipboard/clipboard_types.h"

namespace vivaldi {

namespace clipboard {

void OnInputEvent(const blink::WebInputEvent& input_event);
bool SuppressWrite(ui::ClipboardType clipboardType);

}  // Clipboard

}  // vivaldi

#endif  // BROWSER_VIVALDI_CLIPBOARD_UTILS_H