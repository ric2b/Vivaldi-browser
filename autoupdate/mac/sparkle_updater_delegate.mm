// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "autoupdate/mac/sparkle_updater_delegate.h"

#include "app/vivaldi_constants.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "extensions/tools/vivaldi_tools.h"
#include "extensions/schema/autoupdate.h"

@implementation SparkleUpdaterDelegate

int window_id(Browser* browser) {
  return extensions::ExtensionTabUtil::GetWindowId(browser);
}

Browser* browser() {
  return chrome::FindBrowserWithWindow([NSApp mainWindow]);
}

- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)item {
    if (browser()) {
      ::vivaldi::BroadcastEvent(
        extensions::vivaldi::auto_update::OnDidFindValidUpdate::kEventName,
        extensions::vivaldi::auto_update::OnDidFindValidUpdate::Create(
          window_id(browser()),
          [[[item releaseNotesURL] absoluteString] UTF8String]),
        browser()->profile());
    }
}

- (void)updater:(SUUpdater *)updater willDownloadUpdate:(SUAppcastItem *)item withRequest:(NSMutableURLRequest *)request {
  if (browser()) {
    ::vivaldi::BroadcastEvent(
      extensions::vivaldi::auto_update::OnWillDownloadUpdate::kEventName,
      extensions::vivaldi::auto_update::OnWillDownloadUpdate::Create(
        window_id(browser())),
      browser()->profile());
  }
}

- (void)updater:(SUUpdater *)updater didDownloadUpdate:(SUAppcastItem *)item {
  if (browser()) {
    ::vivaldi::BroadcastEvent(
      extensions::vivaldi::auto_update::OnWillDownloadUpdate::kEventName,
      extensions::vivaldi::auto_update::OnWillDownloadUpdate::Create(
        window_id(browser())),
      browser()->profile());
  }
}

- (void)updaterWillRelaunchApplication:(SUUpdater *)updater {
  if (browser()) {
    ::vivaldi::BroadcastEvent(
      extensions::vivaldi::auto_update::OnUpdaterWillRelaunchApplication::kEventName,
      extensions::vivaldi::auto_update::OnUpdaterWillRelaunchApplication::Create(
        window_id(browser())),
      browser()->profile());
  }
}

- (void)updaterDidRelaunchApplication:(SUUpdater *)updater {
  if (browser()) {
    ::vivaldi::BroadcastEvent(
      extensions::vivaldi::auto_update::OnUpdaterDidRelaunchApplication::kEventName,
      extensions::vivaldi::auto_update::OnUpdaterDidRelaunchApplication::Create(
        window_id(browser())),
      browser()->profile());
  }
}

- (void)updater:(SUUpdater *)updater willInstallUpdateOnQuit:(SUAppcastItem *)item immediateInstallationInvocation:(NSInvocation *)invocation {
  if (browser()) {
    ::vivaldi::BroadcastEvent(
      extensions::vivaldi::auto_update::OnWillInstallUpdateOnQuit::kEventName,
      extensions::vivaldi::auto_update::OnWillInstallUpdateOnQuit::Create(
        window_id(browser())),
      browser()->profile());
  }
}

- (void)updater:(SUUpdater *)updater didAbortWithError:(NSError *)error {
  LOG(INFO) << "Sparkle error: " << [[error localizedDescription] UTF8String];
  if (browser()) {
    ::vivaldi::BroadcastEvent(
      extensions::vivaldi::auto_update::OnDidAbortWithError::kEventName,
      extensions::vivaldi::auto_update::OnDidAbortWithError::Create(
        window_id(browser())),
      browser()->profile());
  }
}

@end  // @implementation SparkleUpdaterDelegate