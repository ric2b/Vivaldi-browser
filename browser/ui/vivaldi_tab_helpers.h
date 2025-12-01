// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef BROWSER_UI_VIVALDI_TAB_HELPERS_H_
#define BROWSER_UI_VIVALDI_TAB_HELPERS_H_

#include "base/values.h"
namespace content {
class WebContents;
}

namespace vivaldi {
bool IsWorkspacesEnabled(content::WebContents* contents);
base::Value::List getLinkRoutes(content::WebContents* contents);
void VivaldiAttachTabHelpers(content::WebContents* web_contents);
}

#endif  // BROWSER_UI_VIVALDI_TAB_HELPERS_H_
