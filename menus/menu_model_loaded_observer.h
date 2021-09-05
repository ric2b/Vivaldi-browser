// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef MENUS_MENU_MODEL_LOADED_OBSERVER_H_
#define MENUS_MENU_MODEL_LOADED_OBSERVER_H_

#include "menus/menu_model_observer.h"

namespace menus {

class Menu_Model;

class MenuModelLoadedObserver : public MenuModelObserver {
 public:
  MenuModelLoadedObserver();

 private:
  void MenuModelLoaded(Menu_Model* model) override;
  void MenuModelBeingDeleted(Menu_Model* model) override;

  DISALLOW_COPY_AND_ASSIGN(MenuModelLoadedObserver);
};

}  // namespace menus

#endif  // MENUS_MENU_MODEL_LOADED_OBSERVER_H_
