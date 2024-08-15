// Copyright (c) 2016 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_BROWSER_FINDER_H_
#define BROWSER_VIVALDI_BROWSER_FINDER_H_

#include "chrome/browser/ui/browser.h"

class VivaldiBrowserWindow;

namespace content {
class WebContents;
class RenderWidgetHostView;
}  // namespace content

namespace vivaldi {

Browser* FindBrowserForEmbedderWebContents(
    const content::WebContents* contents);

VivaldiBrowserWindow* FindWindowForEmbedderWebContents(
    const content::WebContents* contents);

Browser* FindBrowserWithTab(const content::WebContents* web_contents);
Browser* FindBrowserWithNonTabContent(const content::WebContents* web_contents);
Browser* FindBrowserByWindowId(SessionID::id_type window_id);

int GetBrowserCountOfType(Browser::Type type);

}  // namespace vivaldi

#endif  // BROWSER_VIVALDI_BROWSER_FINDER_H_
