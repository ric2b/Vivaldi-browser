// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "menus/menu_model_loaded_observer.h"

#include "menus/menu_model.h"

namespace menus {

MenuModelLoadedObserver::MenuModelLoadedObserver() {}

void MenuModelLoadedObserver::MenuModelLoaded(Menu_Model* model) {
  model->RemoveObserver(this);
  delete this;
}

void MenuModelLoadedObserver::MenuModelBeingDeleted(Menu_Model* model) {
  model->RemoveObserver(this);
  delete this;
}

}  // namespace menus
