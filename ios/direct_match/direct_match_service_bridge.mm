// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/direct_match/direct_match_service_bridge.h"

#import "base/check.h"
#import "base/notreached.h"

namespace direct_match_ios {

DirectMatchServiceBridge::DirectMatchServiceBridge(
      id<DirectMatchServiceBridgeObserver> observer,
      direct_match::DirectMatchService* directMatchServie)
    : observer_(observer), direct_match_service_(directMatchServie) {
  DCHECK(observer_);
  DCHECK(direct_match_service_);
  direct_match_service_->AddObserver(this);
}

DirectMatchServiceBridge::~DirectMatchServiceBridge() {
  if (direct_match_service_) {
    direct_match_service_->RemoveObserver(this);
  }
}

void DirectMatchServiceBridge::OnFinishedDownloadingDirectMatchUnits() {
  SEL selector = @selector(directMatchUnitsDownloaded);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ directMatchUnitsDownloaded];
  }
}

void DirectMatchServiceBridge::OnFinishedDownloadingDirectMatchUnitsIcon() {
  SEL selector = @selector(directMatchUnitsIconDownloaded);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ directMatchUnitsIconDownloaded];
  }
}

} // namespace direct_match_ios
