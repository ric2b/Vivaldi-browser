// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
#ifndef EXTENSIONS_VIVALDI_ASSOCIATED_TABS_H_
#define EXTENSIONS_VIVALDI_ASSOCIATED_TABS_H_

#include "base/values.h"

class TabStripModel;
class TabStripModelChange;

namespace content {
  class WebContents;
}

namespace vivaldi {
void HandleAssociatedTabs(TabStripModel* tab_strip_model,
                          const TabStripModelChange& change);

void AddVivaldiTabItemsToEvent(content::WebContents* contents,
                               base::Value::Dict& object_args);
}

#endif
