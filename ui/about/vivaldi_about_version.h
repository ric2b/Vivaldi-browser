// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#ifndef UI_ABOUT_VIVALDI_ABOUT_VERSION_H_
#define UI_ABOUT_VIVALDI_ABOUT_VERSION_H_

namespace content {
class WebUIDataSource;
}

namespace vivaldi {
void UpdateVersionUIDataSource(content::WebUIDataSource* html_source);
}
#endif  // UI_ABOUT_VIVALDI_ABOUT_VERSION_H_
