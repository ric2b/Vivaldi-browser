// Copyright (c) 2016 Vivaldi. All rights reserved.

#include "browser/vivaldi_webcontents_util.h"

#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace vivaldi {

bool IsVivaldiMail(content::WebContents* web_contents) {
  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(web_contents);
  return web_view_guest && web_view_guest->IsVivaldiMail();
}

bool IsVivaldiWebPanel(content::WebContents* web_contents) {
  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(web_contents);
  return web_view_guest && web_view_guest->IsVivaldiWebPanel();
}

}  // namespace vivaldi