// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_BROWSER_FINDER_H_
#define BROWSER_VIVALDI_BROWSER_FINDER_H_

#include "chrome/browser/ui/browser.h"

namespace content {
class WebContents;
class RenderWidgetHostView;
}

namespace vivaldi {

Browser* FindBrowserForEmbedderWebContents(
    const content::WebContents* contents);

Browser* FindBrowserWithWebContents(content::WebContents* web_contents);

int GetBrowserCountOfType(Browser::Type type);

}  // namespace vivaldi

#endif  // BROWSER_VIVALDI_BROWSER_FINDER_H_
