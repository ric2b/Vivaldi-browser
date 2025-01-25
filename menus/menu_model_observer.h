// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MENUS_MENU_MODEL_OBSERVER_H_
#define MENUS_MENU_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace menus {

class Menu_Model;

class MenuModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading.
  virtual void MenuModelLoaded(Menu_Model* model) {}

  // Invoked when the model has be resat.
  virtual void MenuModelReset(Menu_Model* model, bool all) {}

  // Invoked when the model has been changed after initial loading. If select_id
  // is different from -1 it can be used by JS to set initial selection.
  virtual void MenuModelChanged(Menu_Model* model,
                                int64_t select_id,
                                const std::string& menu_name) {}

  // Invoked from the destructor of the MenuModel.
  virtual void MenuModelBeingDeleted(Menu_Model* model) {}
};

}  // namespace menus

#endif  // MENUS_MENU_MODEL_OBSERVER_H_
