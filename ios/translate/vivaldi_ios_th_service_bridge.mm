// Copyright 2025 Vivaldi Technologies. All rights reserved.

#import "ios/translate/vivaldi_ios_th_service_bridge.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "translate_history/th_model.h"

using translate_history::TH_Model;

namespace translate_history {

VivaldiIOSTHServiceBridge::VivaldiIOSTHServiceBridge(
    id<VivaldiIOSTHServiceBridgeObserver> observer,
    TH_Model* th_model)
    : observer_(observer), th_model_(th_model) {
  DCHECK(observer_);
  DCHECK(th_model_);
  th_model_->AddObserver(this);
}

VivaldiIOSTHServiceBridge::~VivaldiIOSTHServiceBridge() {
  if (th_model_) {
    th_model_->RemoveObserver(this);
  }
}

void VivaldiIOSTHServiceBridge::TH_ModelLoaded(TH_Model* model) {
  SEL selector = @selector(modelDidLoad);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ modelDidLoad];
  }
}

void VivaldiIOSTHServiceBridge::TH_ModelChanged(TH_Model* model) {
  SEL selector = @selector(modelDidChange);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ modelDidChange];
  }
}

void VivaldiIOSTHServiceBridge::TH_ModelBeingDeleted(TH_Model* model) {
  SEL selector = @selector(modelWillBeDeleted);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ modelWillBeDeleted];
  }
}

void VivaldiIOSTHServiceBridge::TH_ModelElementAdded(
    translate_history::TH_Model* model,
    int index) {
  SEL selector = @selector(modelDidAddElementAtIndex:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ modelDidAddElementAtIndex:index];
  }
}

void VivaldiIOSTHServiceBridge::TH_ModelElementMoved(
    translate_history::TH_Model* model,
    int index) {
  SEL selector = @selector(modelDidMoveElementAtIndex:);
  if ([observer_ respondsToSelector:selector]) {
    [observer_ modelDidMoveElementAtIndex:index];
  }
}

void VivaldiIOSTHServiceBridge::TH_ModelElementsRemoved(
    translate_history::TH_Model* model,
    const std::vector<std::string>& ids) {
  SEL selector = @selector(modelDidRemoveElementsWithIds:);
  if ([observer_ respondsToSelector:selector]) {
    NSMutableArray* nsIds = [NSMutableArray arrayWithCapacity:ids.size()];
    for (const auto& id : ids) {
      NSString *idNSSring = base::SysUTF8ToNSString(id);
      [nsIds addObject:idNSSring];
    }
    [observer_ modelDidRemoveElementsWithIds:nsIds];
  }
}

}  // namespace translate_history
