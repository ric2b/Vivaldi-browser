// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import  "thirdparty/macsparkle/Sparkle/SUUpdater.h"
#import  "thirdparty/macsparkle/Sparkle/SUAppcastItem.h"
#import  "thirdparty/macsparkle/Sparkle/SUUpdaterDelegate.h"

#include "base/callback.h"

enum class AutoUpdateStatus;

@interface SparkleUpdaterDelegate : NSObject<SUUpdaterDelegate> {}

- (void)updater:(SUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)item;
- (void)updaterDidNotFindUpdate:(SUUpdater *)updater;
- (void)updater:(SUUpdater *)updater willDownloadUpdate:(SUAppcastItem *)item withRequest:(NSMutableURLRequest *)request;
- (void)updater:(SUUpdater *)updater didDownloadUpdate:(SUAppcastItem *)item;
- (void)updaterWillRelaunchApplication:(SUUpdater *)updater;
- (void)updaterDidRelaunchApplication:(SUUpdater *)updater;
- (void)updater:(SUUpdater *)updater willInstallUpdateOnQuit:(SUAppcastItem *)item immediateInstallationInvocation:(NSInvocation *)invocation;
- (void)updater:(SUUpdater *)updater didAbortWithError:(NSError *)error;

- (void)installUpdateAndRestart;
- (AutoUpdateStatus)getUpdateStatus;
- (std::string)getUpdateVersion;
- (std::string)getUpdateReleaseNotesUrl;

@end // @interface SparkleUpdaterDelegate