// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"

namespace content {

class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  // Returns the last committed navigation entry of the first tab. May be NULL
  // if there is no such entry.
  NavigationEntry* GetLastCommittedEntry() {
    return browser()->tab_strip_model()->GetWebContentsAt(0)->
        GetController().GetLastCommittedEntry();
  }

#if defined(OS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisableAboutInSettings);
  }
#endif
};

// TODO(vivaldi) Reenable for Vivaldi
#if defined(OS_MACOSX)
#define MAYBE_UberURLHandler_SettingsPage DISABLED_UberURLHandler_SettingsPage
#else
#define MAYBE_UberURLHandler_SettingsPage UberURLHandler_SettingsPage
#endif
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       MAYBE_UberURLHandler_SettingsPage) {
  const GURL url_short("chrome://settings/");
  const GURL url_long("chrome://chrome/settings/");

  ui_test_utils::NavigateToURL(browser(), url_short);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_long, entry->GetURL());
  EXPECT_EQ(url_short, entry->GetVirtualURL());
}

// TODO(vivaldi) Reenable for Vivaldi
#if defined(OS_MACOSX)
#define MAYBE_UberURLHandler_ContentSettingsPage DISABLED_UberURLHandler_ContentSettingsPage
#else
#define MAYBE_UberURLHandler_ContentSettingsPage UberURLHandler_ContentSettingsPage
#endif
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       MAYBE_UberURLHandler_ContentSettingsPage) {
  const GURL url_short("chrome://settings/content");
  const GURL url_long("chrome://chrome/settings/content");

  ui_test_utils::NavigateToURL(browser(), url_short);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_long, entry->GetURL());
  EXPECT_EQ(url_short, entry->GetVirtualURL());
}

// TODO(vivaldi) Reenable for Vivaldi
#if defined(OS_MACOSX)
#define MAYBE_UberURLHandler_AboutPage DISABLED_UberURLHandler_AboutPage
#else
#define MAYBE_UberURLHandler_AboutPage UberURLHandler_AboutPage
#endif
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       MAYBE_UberURLHandler_AboutPage) {
  const GURL url("chrome://chrome/");

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

// TODO(vivaldi) Reenable for Vivaldi
#if defined(OS_MACOSX)
#define MAYBE_UberURLHandler_EmptyHost DISABLED_UberURLHandler_EmptyHost
#else
#define MAYBE_UberURLHandler_EmptyHost UberURLHandler_EmptyHost
#endif
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       MAYBE_UberURLHandler_EmptyHost) {
  const GURL url("chrome://chrome//foo");

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->GetVirtualURL().is_valid());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       SitePerProcessNavigation) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kSitePerProcess);
  ASSERT_TRUE(test_server()->Start());
  const GURL url(test_server()->GetURL("files/title1.html"));

  ui_test_utils::NavigateToURL(browser(), url);
  NavigationEntry* entry = GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

}  // namespace content
