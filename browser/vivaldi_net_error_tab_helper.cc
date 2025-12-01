// Copyright (c) 2025 Vivaldi. All rights reserved.

#include "chrome/browser/net/net_error_tab_helper.h"

namespace chrome_browser_net {

void NetErrorTabHelper::OpenVivaldia() {
  content::OpenURLParams params(GURL("vivaldi://game/"), content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents()->OpenURL(params, /*navigation_handle_callback=*/{});
}

}  // namespace chrome_browser_net
