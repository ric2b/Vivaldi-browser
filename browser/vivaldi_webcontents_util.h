// Copyright (c) 2019 Vivaldi. All rights reserved.

#ifndef BROWSER_VIVALDI_WEBCONTENTS_UTIL_H_
#define BROWSER_VIVALDI_WEBCONTENTS_UTIL_H_

namespace content {
class WebContents;
}

namespace vivaldi {
bool IsVivaldiMail(content::WebContents* web_contents);
bool IsVivaldiWebPanel(content::WebContents* web_contents);
}  // namespace vivaldi

#endif  // BROWSER_VIVALDI_WEBCONTENTS_UTIL_H_
