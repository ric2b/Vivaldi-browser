// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_DIRECT_MATCH_DIRECT_MATCH_SERVICE_BRIDGE_H_
#define IOS_DIRECT_MATCH_DIRECT_MATCH_SERVICE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/compiler_specific.h"
#import "components/direct_match/direct_match_service.h"

using direct_match::DirectMatchService;

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol DirectMatchServiceBridgeObserver <NSObject>
- (void)directMatchUnitsDownloaded;
- (void)directMatchUnitsIconDownloaded;
@end

namespace direct_match_ios {
// A bridge that translates DirectMatchService Observers C++ callbacks into ObjC
// callbacks.
class DirectMatchServiceBridge:
    public direct_match::DirectMatchService::Observer {
 public:
  explicit DirectMatchServiceBridge(
        id<DirectMatchServiceBridgeObserver> observer,
        direct_match::DirectMatchService* directMatchService);
  ~DirectMatchServiceBridge() override;

 private:
  virtual void OnFinishedDownloadingDirectMatchUnits() override;
  virtual void OnFinishedDownloadingDirectMatchUnitsIcon() override;

  __weak id<DirectMatchServiceBridgeObserver> observer_;
  direct_match::DirectMatchService* direct_match_service_ = nullptr;
};
}  // namespace direct_match_ios

#endif  // IOS_DIRECT_MATCH_DIRECT_MATCH_SERVICE_BRIDGE_H_
