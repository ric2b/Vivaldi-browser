// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#ifndef PREFS_VIVALDI_TAB_ZOOM_PREF_H_
#define PREFS_VIVALDI_TAB_ZOOM_PREF_H_

namespace content {
class WebContents;
}

// NOTE(arnar@vivaldi.com): Helper function to allow usage of
// browser code in components. Prevents linker errors when
// compiled in components_unit_tests
namespace vivaldi {
bool IsTabZoomEnabled(content::WebContents* web_contents);
}  // namespace vivaldi

#endif  // PREFS_VIVALDI_TAB_ZOOM_PREF_H_
