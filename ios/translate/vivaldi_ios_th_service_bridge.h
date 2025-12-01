// Copyright 2025 Vivaldi Technologies. All rights reserved.

#ifndef IOS_TRANSLATE_VIVALDI_IOS_TH_SERVICE_BRIDGE_H_
#define IOS_TRANSLATE_VIVALDI_IOS_TH_SERVICE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/compiler_specific.h"
#import "translate_history/th_model.h"
#import "translate_history/th_model_observer.h"

using translate_history::TH_Model;
using translate_history::TH_ModelObserver;

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol VivaldiIOSTHServiceBridgeObserver <NSObject>
  - (void)modelDidLoad;
  - (void)modelDidChange;
  - (void)modelWillBeDeleted;
  - (void)modelDidAddElementAtIndex:(NSInteger)index;
  - (void)modelDidMoveElementAtIndex:(NSInteger)index;
  - (void)modelDidRemoveElementsWithIds:(NSArray<NSString*>*)ids;
@end

namespace translate_history {
// A bridge that translates TH_Model Observers C++ callbacks into ObjC
// callbacks.
class VivaldiIOSTHServiceBridge : public TH_ModelObserver {
 public:
  explicit VivaldiIOSTHServiceBridge(
      id<VivaldiIOSTHServiceBridgeObserver> observer,
      TH_Model* th_model);
  ~VivaldiIOSTHServiceBridge() override;

 private:
  // TH_ModelObserver
  virtual void TH_ModelLoaded(TH_Model* model) override;
  virtual void TH_ModelChanged(TH_Model* model) override;
  virtual void TH_ModelBeingDeleted(TH_Model* model) override;
  virtual void TH_ModelElementAdded(translate_history::TH_Model* model,
                                    int index) override;
  virtual void TH_ModelElementMoved(translate_history::TH_Model* model,
                                    int index) override;
  virtual void TH_ModelElementsRemoved(
      translate_history::TH_Model* model,
      const std::vector<std::string>& ids) override;

  __weak id<VivaldiIOSTHServiceBridgeObserver> observer_;
  TH_Model* th_model_ = nullptr;
};
}  // namespace translate_history

#endif  // IOS_TRANSLATE_VIVALDI_IOS_TH_SERVICE_BRIDGE_H_
