// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_CLIPBOARD_UTILS_H_
#define BROWSER_VIVALDI_CLIPBOARD_UTILS_H_

#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/base/clipboard/clipboard_types.h"

namespace vivaldi {

namespace clipboard {

void OnInputEvent(const blink::WebInputEvent& input_event);
bool SuppressWrite(ui::ClipboardType clipboardType);

}  // namespace clipboard

}  // namespace vivaldi

#endif  // BROWSER_VIVALDI_CLIPBOARD_UTILS_H_
