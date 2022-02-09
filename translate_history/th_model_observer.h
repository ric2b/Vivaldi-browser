// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef TRANSLATE_HISTORY_TH_MODEL_OBSERVER_H_
#define TRANSLATE_HISTORY_TH_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace translate_history {

class TH_Model;

class TH_ModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading
  virtual void TH_ModelLoaded(TH_Model* model) {}

  // Invoked when the model has been changed after initial loading.
  virtual void TH_ModelChanged(TH_Model* model) {}

  // Invoked from the destructor of the MenuModel.
  virtual void TH_ModelBeingDeleted(TH_Model* model) {}

  virtual void TH_ModelElementAdded(TH_Model* model, int index) {}
  virtual void TH_ModelElementMoved(TH_Model* model, int index) {}
  virtual void TH_ModelElementsRemoved(TH_Model* model,
                                       const std::vector<std::string>& ids) {}
};

}  // namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_MODEL_OBSERVER_H_
