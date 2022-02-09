// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef TRANSLATE_HISTORY_TH_MODEL_LOADED_OBSERVER_H_
#define TRANSLATE_HISTORY_TH_MODEL_LOADED_OBSERVER_H_

#include "translate_history/th_model_observer.h"

namespace translate_history {

class TH_ModelLoadedObserver : public TH_ModelObserver {
 public:
  TH_ModelLoadedObserver();
  TH_ModelLoadedObserver(const TH_ModelLoadedObserver&) = delete;
  TH_ModelLoadedObserver& operator=(const TH_ModelLoadedObserver&) = delete;

 private:
  void TH_ModelLoaded(TH_Model* model) override;
  void TH_ModelBeingDeleted(TH_Model* model) override;
};

}  // namespace translate_history

#endif  // TRANSLATE_HISTORY_TH_MODEL_LOADED_OBSERVER_H_