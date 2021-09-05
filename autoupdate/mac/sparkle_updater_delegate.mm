// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "autoupdate/mac/sparkle_updater_delegate.h"

#include "app/vivaldi_constants.h"
#include "base/logging.h"
#include "extensions/tools/vivaldi_tools.h"
#include "extensions/api/auto_update/auto_update_api.h"

@implementation SparkleUpdaterDelegate

- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)item {
  std::string url = [[[item releaseNotesURL] absoluteString] UTF8String];
  extensions::AutoUpdateAPI::SendDidFindValidUpdate(url);
}

- (void)updater:(SUUpdater *)updater willDownloadUpdate:(SUAppcastItem *)item withRequest:(NSMutableURLRequest *)request {
  extensions::AutoUpdateAPI::SendWillDownloadUpdate();
}

- (void)updater:(SUUpdater *)updater didDownloadUpdate:(SUAppcastItem *)item {
  extensions::AutoUpdateAPI::SendDidDownloadUpdate();
}

- (void)updaterWillRelaunchApplication:(SUUpdater *)updater {
  extensions::AutoUpdateAPI::SendUpdaterWillRelaunchApplication();
}

- (void)updaterDidRelaunchApplication:(SUUpdater *)updater {
  extensions::AutoUpdateAPI::SendUpdaterDidRelaunchApplication();
}

- (void)updater:(SUUpdater *)updater willInstallUpdateOnQuit:(SUAppcastItem *)item immediateInstallationInvocation:(NSInvocation *)invocation {
  extensions::AutoUpdateAPI::SendWillInstallUpdateOnQuit();
}

- (void)updater:(SUUpdater *)updater didAbortWithError:(NSError *)error {
  LOG(INFO) << "Sparkle error: " << [[error localizedDescription] UTF8String];
  extensions::AutoUpdateAPI::SendDidAbortWithError();
}

@end  // @implementation SparkleUpdaterDelegate