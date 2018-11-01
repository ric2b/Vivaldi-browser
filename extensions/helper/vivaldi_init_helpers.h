// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_VIVALDI_INIT_HELPERS_H_
#define EXTENSIONS_HELPER_VIVALDI_INIT_HELPERS_H_

#include "content/public/browser/web_contents_user_data.h"

namespace vivaldi {

// Function to init Vivaldi specific tap helpers
void InitHelpers(content::WebContents* web_contents);

}  // namespace vivaldi

#endif  // EXTENSIONS_HELPER_VIVALDI_INIT_HELPERS_H_
