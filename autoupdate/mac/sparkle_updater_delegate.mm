// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "autoupdate/mac/sparkle_updater_delegate.h"

#include "app/vivaldi_constants.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "extensions/api/auto_update/auto_update_api.h"
#include "extensions/api/auto_update/auto_update_status.h"
#include "extensions/tools/vivaldi_tools.h"

#import "thirdparty/macsparkle/Sparkle/SUErrors.h"

namespace {

base::Version GetItemVersion(SUAppcastItem* item) {
  std::string version = base::SysNSStringToUTF8(item.versionString);
  return base::Version(version);
}

}  // namespace

@implementation SparkleUpdaterDelegate {

NSInvocation* invocation_;
AutoUpdateStatus status_;
NSString* version_;
NSURL* rel_notes_url_;
}

- (instancetype)init {
  if ((self = [super init])) {
    status_ = AutoUpdateStatus::kNoUpdate;
  }
  return self;
}

- (void)dealloc {
  if (invocation_) {
    [invocation_ release];
  }
  [super dealloc];
}

- (void)updater:(SUUpdater*)updater didFindValidUpdate:(SUAppcastItem*)item {
  status_ = AutoUpdateStatus::kDidFindValidUpdate;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  extensions::AutoUpdateAPI::SendDidFindValidUpdate(
    [self getUpdateReleaseNotesUrl],
    GetItemVersion(item));
}

- (void)updater:(SUUpdater*)updater
    willDownloadUpdate:(SUAppcastItem*)item
           withRequest:(NSMutableURLRequest*)request {
  status_ = AutoUpdateStatus::kWillDownloadUpdate;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  extensions::AutoUpdateAPI::SendWillDownloadUpdate(GetItemVersion(item));
}

- (void)updater:(SUUpdater*)updater didDownloadUpdate:(SUAppcastItem*)item {
  status_ = AutoUpdateStatus::kDidDownloadUpdate;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  extensions::AutoUpdateAPI::SendDidDownloadUpdate(GetItemVersion(item));
}

- (void)updaterWillRelaunchApplication:(SUUpdater*)updater {
  extensions::AutoUpdateAPI::SendUpdaterWillRelaunchApplication();
}

- (void)updaterDidRelaunchApplication:(SUUpdater*)updater {
  status_ = AutoUpdateStatus::kUpdaterDidRelaunchApplication;
  extensions::AutoUpdateAPI::SendUpdaterDidRelaunchApplication();
}

- (void)updater:(SUUpdater*)updater
            willInstallUpdateOnQuit:(SUAppcastItem*)item
    immediateInstallationInvocation:(NSInvocation*)invocation {
  status_ = AutoUpdateStatus::kWillInstallUpdateOnQuit;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  if (invocation_) {
    [invocation_ release];
  }
  invocation_ = [invocation retain];
  extensions::AutoUpdateAPI::SendWillInstallUpdateOnQuit(GetItemVersion(item));
}

- (void)updater:(SUUpdater*)updater didAbortWithError:(NSError*)error {
  status_ = AutoUpdateStatus::kDidAbortWithError;
  std::string desc = "";
  std::string reason = "";

  if (error.code == SUDownloadError ||
      error.code == SUAppcastError) {
    // Don't report to UI, will try again when internet connection is restored
    return;
  }

  if (error && [error localizedDescription] && [error localizedFailureReason]) {
    desc = [[error localizedDescription] UTF8String];
    reason = [[error localizedFailureReason] UTF8String];
    LOG(INFO) << desc + "\n" + reason;
  }

  extensions::AutoUpdateAPI::SendDidAbortWithError(desc, reason);
}

- (void)installUpdateAndRestart {
  if (invocation_) {
    [invocation_ invoke];
  } else {
    vivaldi::RestartBrowser();
  }
}

- (AutoUpdateStatus)getUpdateStatus {
  return status_;
}

- (std::string)getUpdateVersion {
  return base::SysNSStringToUTF8(version_);
}

- (std::string)getUpdateReleaseNotesUrl {
  return base::SysNSStringToUTF8(rel_notes_url_.absoluteString);
}

@end  // @implementation SparkleUpdaterDelegate