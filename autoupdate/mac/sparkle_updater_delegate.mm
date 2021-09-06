// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "autoupdate/mac/sparkle_updater_delegate.h"

#include "app/vivaldi_constants.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "extensions/api/auto_update/auto_update_api.h"
#include "extensions/tools/vivaldi_tools.h"

namespace {

base::Version GetItemVersion(SUAppcastItem* item) {
  std::string version = base::SysNSStringToUTF8(item.versionString);
  return base::Version(version);
}

}  // namespace

@implementation SparkleUpdaterDelegate {

NSInvocation* invocation_;

}

- (void)dealloc {
  if (invocation_) {
    [invocation_ release];
  }
  [super dealloc];
}

- (void)updater:(SUUpdater*)updater didFindValidUpdate:(SUAppcastItem*)item {
  std::string url =
      base::SysNSStringToUTF8(item.releaseNotesURL.absoluteString);
  extensions::AutoUpdateAPI::SendDidFindValidUpdate(url, GetItemVersion(item));
}

- (void)updater:(SUUpdater*)updater
    willDownloadUpdate:(SUAppcastItem*)item
           withRequest:(NSMutableURLRequest*)request {
  extensions::AutoUpdateAPI::SendWillDownloadUpdate(GetItemVersion(item));
}

- (void)updater:(SUUpdater*)updater didDownloadUpdate:(SUAppcastItem*)item {
  extensions::AutoUpdateAPI::SendDidDownloadUpdate(GetItemVersion(item));
}

- (void)updaterWillRelaunchApplication:(SUUpdater*)updater {
  extensions::AutoUpdateAPI::SendUpdaterWillRelaunchApplication();
}

- (void)updaterDidRelaunchApplication:(SUUpdater*)updater {
  extensions::AutoUpdateAPI::SendUpdaterDidRelaunchApplication();
}

- (void)updater:(SUUpdater*)updater
            willInstallUpdateOnQuit:(SUAppcastItem*)item
    immediateInstallationInvocation:(NSInvocation*)invocation {
  if (invocation_) {
    [invocation_ release];
  }
  invocation_ = [invocation retain];
  extensions::AutoUpdateAPI::SendWillInstallUpdateOnQuit(GetItemVersion(item));
}

- (void)updater:(SUUpdater*)updater didAbortWithError:(NSError*)error {
  std::string desc = [[error localizedDescription] UTF8String];
  std::string reason = [[error localizedFailureReason] UTF8String];

  LOG(INFO) << desc + "\n" + reason;

  extensions::AutoUpdateAPI::SendDidAbortWithError(desc, reason);
}

- (void)installUpdateAndRestart {
  if (invocation_) {
    [invocation_ invoke];
  } else {
    vivaldi::RestartBrowser();
  }
}

@end  // @implementation SparkleUpdaterDelegate