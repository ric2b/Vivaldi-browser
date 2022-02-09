// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "translate_history/th_model_loaded_observer.h"

#include "translate_history/th_model.h"

namespace translate_history {

TH_ModelLoadedObserver::TH_ModelLoadedObserver() = default;

void TH_ModelLoadedObserver::TH_ModelLoaded(TH_Model* model) {
  model->RemoveObserver(this);
  delete this;
}

void TH_ModelLoadedObserver::TH_ModelBeingDeleted(TH_Model* model) {
  model->RemoveObserver(this);
  delete this;
}

}  // namespace translate_history
