// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "autoupdate/mac/sparkle_updater_delegate.h"

#include "app/vivaldi_constants.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "browser/init_sparkle.h"
#ifndef VIVALDI_SPARKLE_DISABLED
#include "extensions/api/auto_update/auto_update_api.h"
#include "extensions/api/auto_update/auto_update_status.h"
#endif
#include "extensions/tools/vivaldi_tools.h"

#import "vivaldi/Sparkle/SUErrors.h"

namespace {

base::Version GetItemVersion(SUAppcastItem* item) {
  std::string version = base::SysNSStringToUTF8(item.versionString);
  return base::Version(version);
}

}  // namespace

@interface SparkleUpdaterDelegate ()
@property (nonatomic, copy) void (^invocationBlock)(void);
@end

@implementation SparkleUpdaterDelegate {

AutoUpdateStatus status_;
NSString* version_;
NSString* feedURL_;
NSURL* rel_notes_url_;

uint64_t _totalLength;
uint64_t _bytesDownloaded;
}

@synthesize invocationBlock = _invocationBlock;

- (instancetype)init {
  if ((self = [super init])) {
    status_ = AutoUpdateStatus::kNoUpdate;
  }
  return self;
}

- (void)dealloc {
  if (self.invocationBlock != nil) {
    self.invocationBlock = nil;
  }
}

- (void)updater:(SPUUpdater*)updater didFindValidUpdate:(SUAppcastItem*)item {
  status_ = AutoUpdateStatus::kDidFindValidUpdate;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  extensions::AutoUpdateAPI::SendDidFindValidUpdate(
    [self getUpdateReleaseNotesUrl],
    GetItemVersion(item));
}

- (void)updaterDidNotFindUpdate:(nonnull SPUUpdater*)updater
                          error:(nonnull NSError*)error {
  NSDictionary* userInfo = error.userInfo;
  // Check if the SPUNoUpdateFoundReasonKey exists in the userInfo dictionary
  NSNumber* reasonNumber = userInfo[SPUNoUpdateFoundReasonKey];
  std::string outReason = "";
  if (reasonNumber) {
    // Cast the value to the SPUNoUpdateFoundReason enum type
    SPUNoUpdateFoundReason reason =
        (SPUNoUpdateFoundReason)[reasonNumber integerValue];
    switch (reason) {
      case SPUNoUpdateFoundReasonOnLatestVersion:
        outReason = "OnLatestVersion";
        break;
      case SPUNoUpdateFoundReasonOnNewerThanLatestVersion:
        outReason = "OnNewerThanLatestVersion";
        break;
      case SPUNoUpdateFoundReasonSystemIsTooOld:
        outReason = "SystemIsTooOld";
        break;
      case SPUNoUpdateFoundReasonSystemIsTooNew:
        outReason = "SystemIsTooNew";
        break;
      case SPUNoUpdateFoundReasonUnknown:
      default:
        outReason = "Unknown";
    }
  }

  extensions::AutoUpdateAPI::SendUpdaterDidNotFindUpdate(outReason);
}

- (void)updater:(SPUUpdater*)updater
    willDownloadUpdate:(SUAppcastItem*)item
           withRequest:(NSMutableURLRequest*)request {
  status_ = AutoUpdateStatus::kWillDownloadUpdate;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  extensions::AutoUpdateAPI::SendWillDownloadUpdate(GetItemVersion(item));
}

- (void)updater:(SPUUpdater*)updater didDownloadUpdate:(SUAppcastItem*)item {
  status_ = AutoUpdateStatus::kDidDownloadUpdate;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  extensions::AutoUpdateAPI::SendDidDownloadUpdate(GetItemVersion(item));
}

- (void)updaterWillRelaunchApplication:(SPUUpdater*)updater {
  extensions::AutoUpdateAPI::SendUpdaterWillRelaunchApplication();
}

- (void)updaterDidRelaunchApplication:(SPUUpdater*)updater {
  status_ = AutoUpdateStatus::kUpdaterDidRelaunchApplication;
  extensions::AutoUpdateAPI::SendUpdaterDidRelaunchApplication();
}

- (BOOL)updater:(SPUUpdater *)updater
          willInstallUpdateOnQuit:(SUAppcastItem *)item
      immediateInstallationBlock:(void (^)(void))immediateInstallHandler {
  status_ = AutoUpdateStatus::kWillInstallUpdateOnQuit;
  version_ = item.versionString;
  rel_notes_url_ = item.releaseNotesURL;
  self.invocationBlock = immediateInstallHandler;
  extensions::AutoUpdateAPI::SendWillInstallUpdateOnQuit(GetItemVersion(item));
  return YES;
}

- (void)updater:(SPUUpdater*)updater didAbortWithError:(NSError*)error {
  status_ = AutoUpdateStatus::kDidAbortWithError;
  std::string desc = "";
  std::string reason = "";

  if (error.code == SUNoUpdateError) {
    // Don't report this errors to the UI, instead send updateFinished
    // notification:
    // SUNoUpdateError - No update was found, no need to report that to the UI
    extensions::AutoUpdateAPI::SendUpdateFinished();
    return;
  }

  desc = [[error localizedDescription] UTF8String];
  reason = [error localizedFailureReason]
               ? [[error localizedFailureReason] UTF8String]
               : "";

  if (error.code == SUDownloadError || error.code == SUAppcastError) {
    // Report these as SUDownloadError to the UI. The update button in the
    // address bar will ignore these but the new about page will show an error.
    // SUDownloadError - will try again when internet connection is restored
    // SUAppcastError - will try again when internet connection is restored
    reason = "SUDownloadError";
  }

  extensions::AutoUpdateAPI::SendDidAbortWithError(desc, reason);
}

- (void)updater:(nonnull SPUUpdater*)updater
    didFinishUpdateCycleForUpdateCheck:(SPUUpdateCheck)updateCheck
                                 error:(nullable NSError*)error {
  if (!error || error.code == SUNoUpdateError ||
      error.code == SUInstallationCanceledError)
    extensions::AutoUpdateAPI::SendUpdateFinished();
}

- (NSString *)feedURLStringForUpdater:(SPUUpdater *)updater {
  GURL url = init_sparkle::GetAppcastUrl();
  return [NSString stringWithUTF8String:url.spec().c_str()];
}

- (void)installUpdateAndRestart {
  if (self.invocationBlock != nil) {
    self.invocationBlock();
    self.invocationBlock = nil;
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

- (void)vivaldiExpectedContentLength:(uint64_t)totalLength {
  _totalLength = totalLength;
  _bytesDownloaded = 0;
}

- (void)vivaldiUpdateDidReceiveDataOfLength:(uint64_t)length {
  _bytesDownloaded += length;
  int progress = 0;
  if (_totalLength > 0) {
    progress = (int)((double)_bytesDownloaded * 100 / (double)_totalLength);
  }

  extensions::AutoUpdateAPI::SendUpdateProgress(
      AutoUpdateStatus::kWillDownloadUpdate, base::SysNSStringToUTF8(version_),
      progress);
}

@end  // @implementation SparkleUpdaterDelegate
