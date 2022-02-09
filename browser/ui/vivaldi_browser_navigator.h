// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_BROWSER_UI_VIVALDI_BROWSER_NAVIGATOR_H
#define VIVALDI_BROWSER_UI_VIVALDI_BROWSER_NAVIGATOR_H

namespace vivaldi {

void LoadURLAsPendingEntry(content::WebContents* target_contents,
                           const GURL& url,
                           NavigateParams* params);

}  // namespace vivaldi

#endif  // VIVALDI_BROWSER_UI_VIVALDI_BROWSER_NAVIGATOR_H
