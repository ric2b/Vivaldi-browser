// Copyright 2019 Vivaldi Technologies. All rights reserved.

#import "vivaldi/Sparkle/SPUUpdater.h"
#import "vivaldi/Sparkle/SUAppcastItem.h"
#import "vivaldi/Sparkle/SPUUpdaterDelegate.h"

#include "base/functional/callback.h"

enum class AutoUpdateStatus;

@interface SparkleUpdaterDelegate : NSObject<SPUUpdaterDelegate> {}

- (void)updater:(SPUUpdater *)updater didFindValidUpdate:(SUAppcastItem *)item;
- (void)updaterDidNotFindUpdate:(SPUUpdater *)updater;
- (void)updater:(SPUUpdater *)updater willDownloadUpdate:(SUAppcastItem *)item withRequest:(NSMutableURLRequest *)request;
- (void)updater:(SPUUpdater *)updater didDownloadUpdate:(SUAppcastItem *)item;
- (void)updaterWillRelaunchApplication:(SPUUpdater *)updater;
- (void)updaterDidRelaunchApplication:(SPUUpdater *)updater;
- (BOOL)updater:(SPUUpdater *)updater willInstallUpdateOnQuit:(SUAppcastItem *)item immediateInstallationBlock:(void (^)(void))immediateInstallHandler;
- (void)updater:(SPUUpdater *)updater didAbortWithError:(NSError *)error;
- (NSString *)feedURLStringForUpdater:(SPUUpdater *)updater;

- (void)installUpdateAndRestart;
- (AutoUpdateStatus)getUpdateStatus;
- (std::string)getUpdateVersion;
- (std::string)getUpdateReleaseNotesUrl;

@end // @interface SparkleUpdaterDelegate