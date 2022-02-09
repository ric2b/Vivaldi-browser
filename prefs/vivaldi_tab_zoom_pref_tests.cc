// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#include "prefs/vivaldi_tab_zoom_pref.h"  // nogncheck

// NOTE(arnar@vivaldi.com): Empty helper function to allow usage of
// browser code in components. Prevents linker errors when
// compiled in components_unit_tests.
namespace vivaldi {

bool IsTabZoomEnabled(content::WebContents* web_contents) {
  return false;
}

}  // namespace vivaldi
