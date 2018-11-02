// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/tools/vivaldi_tools.h"

#include <string>

#include "chrome/browser/ui/browser_list.h"

namespace vivaldi {

Browser* FindVivaldiBrowser() {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (browser_list_impl && browser_list_impl->size() > 0)
    return browser_list_impl->get(0);
  return NULL;
}

}  // namespace vivaldi
